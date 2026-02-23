#include "core/server/server.h"
#include "api/translator/translator.h"
#include "api/upstream/upstream.h"
#include "api/stream/stream.h"
#include "ui/dashboard/dashboard.h"
#include "api/responses/responses.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>

/* response.c functions */
void response_send_json(int fd, int status, const char *json);
void response_send_error(int fd, int status, const char *message);
void response_send_options(int fd);
void response_start_sse(int fd);
void response_send_sse_data(int fd, const char *data);
void response_end_sse(int fd);

/* chat.c handler */
void handle_chat_completions(http_request_t *req);

static void handle_health(http_request_t *req)
{
    response_send_json(req->client_fd, 200, "{\"status\":\"ok\"}");
}

static void handle_models(http_request_t *req)
{
    if (strcmp(req->method, "GET") != 0) {
        response_send_error(req->client_fd, 405, "Method not allowed");
        return;
    }

    char *json = models_list_json();
    if (!json) {
        response_send_error(req->client_fd, 500, "Failed to build model list");
        return;
    }

    response_send_json(req->client_fd, 200, json);
    free(json);
}

void router_handle(http_request_t *req)
{
    if (strcmp(req->method, "OPTIONS") == 0) {
        response_send_options(req->client_fd);
        return;
    }

    fprintf(stderr, "[%s] %s\n", req->method, req->path);

    if (strcmp(req->path, "/health") == 0 ||
        strcmp(req->path, "/") == 0) {
        handle_health(req);
    } else if (strcmp(req->path, "/v1/models") == 0 ||
               strcmp(req->path, "/models") == 0) {
        handle_models(req);
    } else if (strcmp(req->path, "/v1/chat/completions") == 0 ||
               strcmp(req->path, "/chat/completions") == 0) {
        handle_chat_completions(req);
    } else if (strcmp(req->path, "/v1/responses") == 0 ||
               strcmp(req->path, "/responses") == 0) {
        handle_responses(req);
    } else if (strncmp(req->path, "/api/dashboard/", 15) == 0) {
        dashboard_handle(req);
    } else if (strncmp(req->path, "/dashboard", 10) == 0) {
        dashboard_serve_static(req);
    } else {
        response_send_error(req->client_fd, 404, "Not found");
    }
}
