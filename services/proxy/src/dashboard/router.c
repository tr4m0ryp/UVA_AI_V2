#include "dashboard.h"
#include "database.h"
#include "server.h"
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* response.c */
void response_send_json(int fd, int status, const char *json);
void response_send_error(int fd, int status, const char *message);
void response_send_options(int fd);

/* auth.c */
void dashboard_auth_login(http_request_t *req);
void dashboard_auth_me(http_request_t *req);
void dashboard_auth_logout(http_request_t *req);
void dashboard_auth_browser_start(http_request_t *req);
void dashboard_auth_browser_status(http_request_t *req);
void dashboard_auth_browser_cancel(http_request_t *req);

/* keys.c */
void dashboard_keys_list(http_request_t *req, const db_user_t *user);
void dashboard_keys_create(http_request_t *req, const db_user_t *user);
void dashboard_keys_update(http_request_t *req, const db_user_t *user,
                           int64_t key_id);
void dashboard_keys_delete(http_request_t *req, const db_user_t *user,
                           int64_t key_id);
void dashboard_keys_toggle(http_request_t *req, const db_user_t *user,
                           int64_t key_id);

static int authenticate_dashboard(http_request_t *req, db_user_t *user)
{
    const char *auth = request_get_header(req, "Authorization");
    if (!auth || strncmp(auth, "Bearer ", 7) != 0) return -1;
    const char *token = auth + 7;
    if (db_user_find_by_token(token, user) != 0) return -1;
    return 0;
}

static int64_t extract_path_id(const char *path, const char *prefix)
{
    const char *p = path + strlen(prefix);
    if (*p == '/') p++;
    if (*p == '\0') return -1;
    return (int64_t)atoll(p);
}

void dashboard_handle(http_request_t *req)
{
    if (strcmp(req->method, "OPTIONS") == 0) {
        response_send_options(req->client_fd);
        return;
    }

    const char *path = req->path;

    /* Auth endpoints (no token required) */
    if (strcmp(path, "/api/dashboard/auth/login") == 0) {
        dashboard_auth_login(req);
        return;
    }
    if (strcmp(path, "/api/dashboard/auth/me") == 0) {
        dashboard_auth_me(req);
        return;
    }
    if (strcmp(path, "/api/dashboard/auth/logout") == 0) {
        dashboard_auth_logout(req);
        return;
    }
    if (strcmp(path, "/api/dashboard/auth/browser-login") == 0) {
        dashboard_auth_browser_start(req);
        return;
    }
    if (strcmp(path, "/api/dashboard/auth/browser-status") == 0) {
        dashboard_auth_browser_status(req);
        return;
    }
    if (strcmp(path, "/api/dashboard/auth/browser-cancel") == 0) {
        dashboard_auth_browser_cancel(req);
        return;
    }

    /* Shutdown endpoint (no auth required -- local only) */
    if (strcmp(path, "/api/dashboard/shutdown") == 0 &&
        strcmp(req->method, "POST") == 0) {
        response_send_json(req->client_fd, 200,
                           "{\"status\":\"shutting_down\"}");
        server_stop();
        return;
    }

    /* All key endpoints require authentication */
    db_user_t user;
    if (authenticate_dashboard(req, &user) != 0) {
        response_send_error(req->client_fd, 401, "Unauthorized");
        return;
    }

    if (strcmp(path, "/api/dashboard/keys") == 0) {
        if (strcmp(req->method, "GET") == 0)
            dashboard_keys_list(req, &user);
        else if (strcmp(req->method, "POST") == 0)
            dashboard_keys_create(req, &user);
        else
            response_send_error(req->client_fd, 405, "Method not allowed");
        return;
    }

    /* /api/dashboard/keys/{id}/toggle */
    if (strncmp(path, "/api/dashboard/keys/", 20) == 0 &&
        strstr(path, "/toggle")) {
        int64_t kid = extract_path_id(path, "/api/dashboard/keys");
        if (kid > 0)
            dashboard_keys_toggle(req, &user, kid);
        else
            response_send_error(req->client_fd, 400, "Invalid key ID");
        return;
    }

    /* /api/dashboard/keys/{id} */
    if (strncmp(path, "/api/dashboard/keys/", 20) == 0) {
        int64_t kid = extract_path_id(path, "/api/dashboard/keys");
        if (kid <= 0) {
            response_send_error(req->client_fd, 400, "Invalid key ID");
            return;
        }
        if (strcmp(req->method, "PUT") == 0)
            dashboard_keys_update(req, &user, kid);
        else if (strcmp(req->method, "DELETE") == 0)
            dashboard_keys_delete(req, &user, kid);
        else
            response_send_error(req->client_fd, 405, "Method not allowed");
        return;
    }

    response_send_error(req->client_fd, 404, "Not found");
}
