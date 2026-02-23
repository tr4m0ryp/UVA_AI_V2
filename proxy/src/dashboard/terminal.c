#include "dashboard/dashboard.h"
#include "database/project.h"
#include "database/database.h"
#include "server/server.h"
#include "websocket/websocket.h"
#include "terminal/terminal.h"
#include "vps/vps.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>

/* response.c */
void response_send_error(int fd, int status, const char *message);

static int query_param(const char *path, const char *key,
                        char *out, size_t out_size)
{
    char search[128];
    snprintf(search, sizeof(search), "%s=", key);

    const char *q = strchr(path, '?');
    if (!q) return -1;
    q++;

    const char *p = strstr(q, search);
    if (!p) return -1;
    p += strlen(search);

    const char *end = strchr(p, '&');
    size_t len = end ? (size_t)(end - p) : strlen(p);
    if (len >= out_size) len = out_size - 1;

    memcpy(out, p, len);
    out[len] = '\0';
    return 0;
}

static int auth_from_query(const char *path, db_user_t *user)
{
    char token[128];
    if (query_param(path, "token", token, sizeof(token)) != 0)
        return -1;
    if (db_user_find_by_token(token, user) != 0)
        return -1;
    return 0;
}

void dashboard_terminal_handle(http_request_t *req, const db_user_t *user)
{
    char pid_str[32];
    if (query_param(req->path, "project_id", pid_str, sizeof(pid_str)) != 0) {
        response_send_error(req->client_fd, 400,
            "Missing project_id parameter");
        return;
    }
    int64_t project_id = (int64_t)atoll(pid_str);
    if (project_id <= 0) {
        response_send_error(req->client_fd, 400, "Invalid project_id");
        return;
    }

    db_project_t proj;
    if (db_project_get(project_id, user->id, &proj) != 0) {
        response_send_error(req->client_fd, 404, "Project not found");
        return;
    }

    /* Determine terminal mode: VPS container or local */
    int use_vps = (proj.container_id[0] != '\0');

    if (!use_vps) {
        /* Local mode: check codex is available */
        const char *codex = terminal_find_codex();
        if (!codex) {
            response_send_error(req->client_fd, 503,
                "Codex CLI not found on server");
            return;
        }
    }

    /* Perform WebSocket handshake */
    if (ws_handshake(req->client_fd, req) != 0) {
        fprintf(stderr, "terminal: WebSocket handshake failed\n");
        return;
    }
    req->fd_claimed = 1;

    /* Read initial message for terminal dimensions */
    int cols = 80, rows = 24;
    ws_frame_t frame;
    if (ws_read_frame(req->client_fd, &frame) == 0 && frame.payload) {
        char *tmp = malloc(frame.payload_len + 1);
        if (tmp) {
            memcpy(tmp, frame.payload, frame.payload_len);
            tmp[frame.payload_len] = '\0';

            struct json_object *init = json_tokener_parse(tmp);
            if (init) {
                struct json_object *v;
                if (json_object_object_get_ex(init, "cols", &v))
                    cols = json_object_get_int(v);
                if (json_object_object_get_ex(init, "rows", &v))
                    rows = json_object_get_int(v);
                json_object_put(init);
            }
            free(tmp);
        }
        free(frame.payload);
    }

    terminal_session_t session;
    memset(&session, 0, sizeof(session));
    session.ws_fd = req->client_fd;
    session.master_fd = -1;
    session.child_pid = -1;

    if (use_vps) {
        /* VPS mode: ssh + docker exec */
        int master_fd = vps_attach_terminal(proj.container_id,
            &session.child_pid, cols, rows);
        if (master_fd < 0) {
            ws_send_close(req->client_fd, 1011);
            return;
        }
        session.master_fd = master_fd;

        fprintf(stderr, "terminal: VPS session for project '%s' "
                "(container=%.12s, pid=%d, %dx%d)\n",
                proj.name, proj.container_id, session.child_pid,
                cols, rows);
    } else {
        /* Local mode */
        const char *work_dir = "/tmp";
        if (terminal_spawn(&session, proj.api_key_plain, proj.model,
                           work_dir, cols, rows) != 0) {
            ws_send_close(req->client_fd, 1011);
            return;
        }

        fprintf(stderr, "terminal: local session for project '%s' "
                "(pid=%d, %dx%d)\n",
                proj.name, session.child_pid, cols, rows);
    }

    /* Bridge blocks until session ends */
    terminal_bridge(&session);

    fprintf(stderr, "terminal: session ended for project '%s'\n", proj.name);
}

void dashboard_terminal_handle_ws(http_request_t *req)
{
    db_user_t user;
    if (auth_from_query(req->path, &user) != 0) {
        response_send_error(req->client_fd, 401, "Unauthorized");
        return;
    }
    dashboard_terminal_handle(req, &user);
}
