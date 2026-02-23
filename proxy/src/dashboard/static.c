#include "dashboard/dashboard.h"
#include "server/server.h"
#include "platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* response.c */
void response_send(int fd, int status, const char *content_type,
                   const char *body, size_t body_len);
void response_send_error(int fd, int status, const char *message);

static const char *get_content_type(const char *path)
{
    const char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    if (strcmp(ext, ".html") == 0) return "text/html; charset=utf-8";
    if (strcmp(ext, ".css") == 0) return "text/css; charset=utf-8";
    if (strcmp(ext, ".js") == 0) return "application/javascript; charset=utf-8";
    if (strcmp(ext, ".json") == 0) return "application/json";
    if (strcmp(ext, ".svg") == 0) return "image/svg+xml";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".ico") == 0) return "image/x-icon";
    return "application/octet-stream";
}

void dashboard_serve_static(http_request_t *req)
{
    if (strcmp(req->method, "GET") != 0) {
        response_send_error(req->client_fd, 405, "Method not allowed");
        return;
    }

    /* Strip /dashboard prefix to get relative file path */
    const char *rel = req->path + 10; /* skip "/dashboard" */
    if (rel[0] == '/') rel++;
    if (rel[0] == '\0') rel = "index.html";

    /* Reject path traversal */
    if (strstr(rel, "..") != NULL) {
        response_send_error(req->client_fd, 403, "Forbidden");
        return;
    }

    /* Build filesystem path */
    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "dashboard/%s", rel);

    /* Check if file exists */
    struct stat st;
    if (stat(filepath, &st) != 0 || !S_ISREG(st.st_mode)) {
        /* SPA fallback: serve index.html for non-file paths */
        snprintf(filepath, sizeof(filepath), "dashboard/index.html");
        if (stat(filepath, &st) != 0) {
            response_send_error(req->client_fd, 404, "Not found");
            return;
        }
    }

    /* Read file */
    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        response_send_error(req->client_fd, 500, "Cannot read file");
        return;
    }

    size_t fsize = (size_t)st.st_size;
    char *buf = malloc(fsize);
    if (!buf) {
        fclose(fp);
        response_send_error(req->client_fd, 500, "Out of memory");
        return;
    }

    size_t nread = fread(buf, 1, fsize, fp);
    fclose(fp);

    const char *ctype = get_content_type(filepath);
    response_send(req->client_fd, 200, ctype, buf, nread);
    free(buf);
}
