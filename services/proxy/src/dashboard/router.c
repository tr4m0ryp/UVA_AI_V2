#include "dashboard.h"
#include "database.h"
#include "server.h"
#include "websocket.h"
#include "webui.h"
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void response_send_json(int fd, int status, const char *json);
void response_send_error(int fd, int status, const char *message);
void response_send_options(int fd);
void dashboard_auth_login(http_request_t *req);
void dashboard_auth_me(http_request_t *req);
void dashboard_auth_logout(http_request_t *req);
void dashboard_auth_browser_start(http_request_t *req);
void dashboard_auth_browser_status(http_request_t *req);
void dashboard_auth_browser_cancel(http_request_t *req);
void dashboard_keys_list(http_request_t *req, const db_user_t *user);
void dashboard_keys_create(http_request_t *req, const db_user_t *user);
void dashboard_keys_update(http_request_t *req, const db_user_t *user, int64_t key_id);
void dashboard_keys_delete(http_request_t *req, const db_user_t *user, int64_t key_id);
void dashboard_keys_toggle(http_request_t *req, const db_user_t *user, int64_t key_id);
void dashboard_keys_usage(http_request_t *req, const db_user_t *user, int64_t key_id);
void dashboard_chat_handle(http_request_t *req, const db_user_t *user);
void dashboard_projects_list(http_request_t *req, const db_user_t *user);
void dashboard_projects_create(http_request_t *req, const db_user_t *user);
void dashboard_projects_get(http_request_t *req, const db_user_t *user, int64_t id);
void dashboard_projects_update(http_request_t *req, const db_user_t *user, int64_t id);
void dashboard_projects_delete(http_request_t *req, const db_user_t *user, int64_t id);
void dashboard_projects_status(http_request_t *req, const db_user_t *user, int64_t id);
void dashboard_projects_logs(http_request_t *req, const db_user_t *user, int64_t id);
void dashboard_terminal_handle_ws(http_request_t *req);
void dashboard_github_auth(http_request_t *req);
void dashboard_github_callback(http_request_t *req);
void dashboard_github_repos(http_request_t *req, const db_user_t *user);
void dashboard_github_status(http_request_t *req, const db_user_t *user);
void dashboard_github_disconnect(http_request_t *req, const db_user_t *user);
void dashboard_grading_list(http_request_t *req, const db_user_t *user);
void dashboard_grading_create(http_request_t *req, const db_user_t *user);
void dashboard_grading_get(http_request_t *req, const db_user_t *user, int64_t sid);
void dashboard_grading_delete(http_request_t *req, const db_user_t *user, int64_t sid);
void dashboard_grading_save_result(http_request_t *req, const db_user_t *user, int64_t sid);
void dashboard_grading_finish(http_request_t *req, const db_user_t *user, int64_t sid);

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

    /* GitHub OAuth -- public (user redirected from GitHub) */
    if (strcmp(path, "/api/dashboard/github/auth") == 0) {
        dashboard_github_auth(req);
        return;
    }
    if (strncmp(path, "/api/dashboard/github/callback", 30) == 0) {
        dashboard_github_callback(req);
        return;
    }

    /* WebSocket terminal (authenticates via query string token) */
    if (strncmp(path, "/api/dashboard/terminal", 23) == 0 &&
        ws_is_upgrade(req)) {
        dashboard_terminal_handle_ws(req);
        return;
    }

    /* WebUI status (no auth required -- local only) */
    if (strcmp(path, "/api/dashboard/webui/status") == 0 &&
        strcmp(req->method, "GET") == 0) {
        char json[128];
        snprintf(json, sizeof(json),
                 "{\"ready\":%s,\"port\":%d}",
                 webui_is_ready() ? "true" : "false",
                 webui_get_port());
        response_send_json(req->client_fd, 200, json);
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

    /* GitHub API (authenticated) */
    if (strcmp(path, "/api/dashboard/github/repos") == 0) {
        dashboard_github_repos(req, &user);
        return;
    }
    if (strcmp(path, "/api/dashboard/github/status") == 0) {
        dashboard_github_status(req, &user);
        return;
    }
    if (strcmp(path, "/api/dashboard/github/disconnect") == 0) {
        dashboard_github_disconnect(req, &user);
        return;
    }

    /* Chat endpoint (streaming, uses user's UvA session) */
    if (strcmp(path, "/api/dashboard/chat") == 0) {
        if (strcmp(req->method, "POST") == 0)
            dashboard_chat_handle(req, &user);
        else
            response_send_error(req->client_fd, 405, "Method not allowed");
        return;
    }

    /* Projects: /api/dashboard/projects[/{id}] */
    if (strcmp(path, "/api/dashboard/projects") == 0) {
        if (strcmp(req->method, "GET") == 0)
            dashboard_projects_list(req, &user);
        else if (strcmp(req->method, "POST") == 0)
            dashboard_projects_create(req, &user);
        else
            response_send_error(req->client_fd, 405, "Method not allowed");
        return;
    }
    if (strncmp(path, "/api/dashboard/projects/", 24) == 0) {
        int64_t pid = extract_path_id(path, "/api/dashboard/projects");
        if (pid <= 0) {
            response_send_error(req->client_fd, 400, "Invalid project ID");
            return;
        }
        /* Check for sub-resources: /status, /logs */
        const char *sub = strchr(path + 24, '/');
        if (sub && strcmp(sub, "/status") == 0) {
            dashboard_projects_status(req, &user, pid);
            return;
        }
        if (sub && strcmp(sub, "/logs") == 0) {
            dashboard_projects_logs(req, &user, pid);
            return;
        }
        if (strcmp(req->method, "GET") == 0)
            dashboard_projects_get(req, &user, pid);
        else if (strcmp(req->method, "PUT") == 0)
            dashboard_projects_update(req, &user, pid);
        else if (strcmp(req->method, "DELETE") == 0)
            dashboard_projects_delete(req, &user, pid);
        else
            response_send_error(req->client_fd, 405, "Method not allowed");
        return;
    }

    /* Grading sessions: /api/dashboard/grading/sessions[/{id}[/results]] */
    if (strncmp(path, "/api/dashboard/grading/sessions", 31) == 0) {
        const char *rest = path + 31;
        if (rest[0] == '\0') {
            /* /api/dashboard/grading/sessions */
            if (strcmp(req->method, "GET") == 0)
                dashboard_grading_list(req, &user);
            else if (strcmp(req->method, "POST") == 0)
                dashboard_grading_create(req, &user);
            else
                response_send_error(req->client_fd, 405,
                    "Method not allowed");
            return;
        }
        if (rest[0] == '/') {
            int64_t sid = (int64_t)atoll(rest + 1);
            if (sid <= 0) {
                response_send_error(req->client_fd, 400,
                    "Invalid session ID");
                return;
            }
            /* Check for sub-resource: /results */
            const char *slash2 = strchr(rest + 1, '/');
            if (slash2 && strcmp(slash2, "/results") == 0) {
                if (strcmp(req->method, "POST") == 0)
                    dashboard_grading_save_result(req, &user, sid);
                else
                    response_send_error(req->client_fd, 405,
                        "Method not allowed");
                return;
            }
            if (!slash2) {
                if (strcmp(req->method, "GET") == 0)
                    dashboard_grading_get(req, &user, sid);
                else if (strcmp(req->method, "DELETE") == 0)
                    dashboard_grading_delete(req, &user, sid);
                else if (strcmp(req->method, "PUT") == 0)
                    dashboard_grading_finish(req, &user, sid);
                else
                    response_send_error(req->client_fd, 405,
                        "Method not allowed");
                return;
            }
        }
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

    /* /api/dashboard/keys/{id}/usage */
    if (strncmp(path, "/api/dashboard/keys/", 20) == 0 &&
        strstr(path, "/usage")) {
        int64_t kid = extract_path_id(path, "/api/dashboard/keys");
        if (kid > 0)
            dashboard_keys_usage(req, &user, kid);
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
