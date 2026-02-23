#include "core/server/server.h"
#include "core/config/config.h"
#include "core/platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>

/* Defined in router.c */
void router_handle(http_request_t *req);

static volatile int g_running = 1;
static int g_server_fd = -1;

void server_stop(void)
{
    g_running = 0;
    if (g_server_fd >= 0) {
        shutdown(g_server_fd, SHUT_RDWR);
        platform_close_socket(g_server_fd);
        g_server_fd = -1;
    }
}

const char *request_get_header(const http_request_t *req, const char *name)
{
    for (int i = 0; i < req->header_count; i++) {
        if (strcasecmp(req->headers[i].name, name) == 0)
            return req->headers[i].value;
    }
    return NULL;
}

static int parse_request(int fd, http_request_t *req)
{
    memset(req, 0, sizeof(*req));
    req->client_fd = fd;

    /* Read the full request into a buffer */
    char *buf = malloc(MAX_BODY_LEN + 8192);
    if (!buf) return -1;

    size_t total = 0;
    int n;

    /* Read headers first (until \r\n\r\n) */
    char *header_end = NULL;
    while (total < MAX_BODY_LEN + 8192 - 1) {
        n = recv(fd, buf + total, 4096, 0);
        if (n <= 0) break;
        total += (size_t)n;
        buf[total] = '\0';
        header_end = strstr(buf, "\r\n\r\n");
        if (header_end) break;
    }

    if (!header_end) {
        free(buf);
        return -1;
    }

    /* Parse request line */
    char *line = buf;
    char *sp1 = strchr(line, ' ');
    if (!sp1) { free(buf); return -1; }
    *sp1 = '\0';
    strncpy(req->method, line, MAX_METHOD_LEN - 1);

    char *path = sp1 + 1;
    char *sp2 = strchr(path, ' ');
    if (!sp2) { free(buf); return -1; }
    *sp2 = '\0';
    strncpy(req->path, path, MAX_PATH_LEN - 1);

    /* Parse headers */
    char *hdr_start = strchr(sp2 + 1, '\n');
    if (hdr_start) hdr_start++;
    char *hdr_end_ptr = header_end;

    req->header_count = 0;
    char *cur = hdr_start;
    while (cur && cur < hdr_end_ptr && req->header_count < MAX_HEADERS) {
        char *eol = strstr(cur, "\r\n");
        if (!eol || eol == cur) break;
        *eol = '\0';

        char *colon = strchr(cur, ':');
        if (colon) {
            *colon = '\0';
            char *val = colon + 1;
            while (*val == ' ') val++;
            strncpy(req->headers[req->header_count].name, cur,
                    MAX_HEADER_NAME - 1);
            strncpy(req->headers[req->header_count].value, val,
                    MAX_HEADER_VALUE - 1);
            req->header_count++;
        }
        cur = eol + 2;
    }

    /* Parse body based on Content-Length */
    size_t body_offset = (size_t)(header_end + 4 - buf);
    size_t body_read = total - body_offset;

    const char *cl = request_get_header(req, "Content-Length");
    size_t content_length = cl ? (size_t)atol(cl) : 0;

    if (content_length > 0) {
        /* Read remaining body if needed */
        while (body_read < content_length && total < MAX_BODY_LEN + 8192 - 1) {
            n = recv(fd, buf + total, 4096, 0);
            if (n <= 0) break;
            total += (size_t)n;
            body_read += (size_t)n;
        }

        req->body = malloc(content_length + 1);
        if (req->body) {
            memcpy(req->body, buf + body_offset, content_length);
            req->body[content_length] = '\0';
            req->body_len = content_length;
        }
    }

    free(buf);
    return 0;
}

static void *handle_connection(void *arg)
{
    int fd = *(int *)arg;
    free(arg);

    pthread_detach(pthread_self());

    http_request_t req;
    if (parse_request(fd, &req) == 0) {
        router_handle(&req);
    }

    free(req.body);
    if (!req.fd_claimed)
        platform_close_socket(fd);
    return NULL;
}

int server_listen(const proxy_config_t *cfg)
{
    g_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_server_fd < 0) {
        perror("socket");
        return -1;
    }

    int opt = 1;
    setsockopt(g_server_fd, SOL_SOCKET, SO_REUSEADDR,
               (const char *)&opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(cfg->listen_port);

    if (bind(g_server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        platform_close_socket(g_server_fd);
        return -1;
    }

    if (listen(g_server_fd, 32) < 0) {
        perror("listen");
        platform_close_socket(g_server_fd);
        return -1;
    }

    fprintf(stderr, "uva-proxy listening on http://0.0.0.0:%d\n",
            cfg->listen_port);
    return 0;
}

int server_accept_loop(void)
{
    g_running = 1;

    while (g_running) {
        int *client_fd = malloc(sizeof(int));
        if (!client_fd) continue;

        *client_fd = accept(g_server_fd, NULL, NULL);
        if (*client_fd < 0) {
            free(client_fd);
            if (!g_running) break;
#ifndef PLATFORM_WINDOWS
            if (errno == EINTR) continue;
#endif
            perror("accept");
            continue;
        }

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_connection, client_fd) != 0) {
            perror("pthread_create");
            platform_close_socket(*client_fd);
            free(client_fd);
        }
    }

    return 0;
}

int server_start(const proxy_config_t *cfg)
{
    if (server_listen(cfg) != 0)
        return -1;
    return server_accept_loop();
}
