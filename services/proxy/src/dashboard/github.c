#include "dashboard.h"
#include "database.h"
#include "github.h"
#include "config.h"
#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>

extern const proxy_config_t *g_config;

/* response.c */
void response_send_json(int fd, int status, const char *json);
void response_send_error(int fd, int status, const char *message);
void response_send_redirect(int fd, const char *url);

/* Extract a query parameter value from a URL path.
 * Writes value to out. Returns 0 if found. */
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

/* GET /api/dashboard/github/auth -- redirect to GitHub OAuth */
void dashboard_github_auth(http_request_t *req)
{
    if (!g_config->github_client_id[0]) {
        response_send_error(req->client_fd, 503,
            "GitHub OAuth not configured (set GITHUB_CLIENT_ID)");
        return;
    }

    char url[1024];
    snprintf(url, sizeof(url),
        "https://github.com/login/oauth/authorize"
        "?client_id=%s&scope=repo&redirect_uri="
        "http%%3A%%2F%%2F127.0.0.1%%3A%d"
        "%%2Fapi%%2Fdashboard%%2Fgithub%%2Fcallback",
        g_config->github_client_id, g_config->listen_port);

    response_send_redirect(req->client_fd, url);
}

/* GET /api/dashboard/github/callback?code=... */
void dashboard_github_callback(http_request_t *req)
{
    char code[256];
    if (query_param(req->path, "code", code, sizeof(code)) != 0) {
        response_send_error(req->client_fd, 400,
            "Missing code parameter");
        return;
    }

    char token[256], login[64];
    if (github_exchange_code(g_config->github_client_id,
            g_config->github_client_secret, code,
            token, (int)sizeof(token),
            login, (int)sizeof(login)) != 0) {
        response_send_error(req->client_fd, 502,
            "Failed to exchange GitHub code for token");
        return;
    }

    /* Store in session -- we need a user_id from query param or cookie.
     * Since this is a redirect from GitHub, we pass the dashboard token
     * as state parameter. Extract it. */
    char state[256];
    if (query_param(req->path, "state", state, sizeof(state)) != 0 ||
        state[0] == '\0') {
        /* No state -- redirect to dashboard with instructions */
        char redirect[256];
        snprintf(redirect, sizeof(redirect),
            "/dashboard/#github-connected?login=%s", login);
        response_send_redirect(req->client_fd, redirect);
        return;
    }

    /* state = dashboard token -- look up user */
    db_user_t user;
    if (db_user_find_by_token(state, &user) == 0) {
        db_user_set_github(user.id, token, login);
        fprintf(stderr, "GitHub connected: %s -> user %lld (%s)\n",
                login, (long long)user.id, user.email);
    }

    char redirect[256];
    snprintf(redirect, sizeof(redirect),
        "/dashboard/#github-connected?login=%s", login);
    response_send_redirect(req->client_fd, redirect);
}

/* GET /api/dashboard/github/repos -- list user's repos */
void dashboard_github_repos(http_request_t *req, const db_user_t *user)
{
    if (!user->github_token[0]) {
        response_send_error(req->client_fd, 403,
            "GitHub not connected");
        return;
    }

    github_repo_t *repos = NULL;
    int count = 0;
    if (github_list_repos(user->github_token, &repos, &count) != 0) {
        response_send_error(req->client_fd, 502,
            "Failed to list GitHub repos");
        return;
    }

    struct json_object *arr = json_object_new_array();
    for (int i = 0; i < count; i++) {
        struct json_object *r = json_object_new_object();
        json_object_object_add(r, "full_name",
            json_object_new_string(repos[i].full_name));
        json_object_object_add(r, "description",
            json_object_new_string(repos[i].description));
        json_object_object_add(r, "clone_url",
            json_object_new_string(repos[i].clone_url));
        json_object_object_add(r, "private",
            json_object_new_boolean(repos[i].is_private));
        json_object_array_add(arr, r);
    }
    free(repos);

    const char *str = json_object_to_json_string_ext(arr,
        JSON_C_TO_STRING_PLAIN);
    response_send_json(req->client_fd, 200, str);
    json_object_put(arr);
}

/* GET /api/dashboard/github/status */
void dashboard_github_status(http_request_t *req, const db_user_t *user)
{
    char buf[256];
    if (user->github_token[0]) {
        snprintf(buf, sizeof(buf),
            "{\"connected\":true,\"login\":\"%s\"}", user->github_login);
    } else {
        snprintf(buf, sizeof(buf), "{\"connected\":false}");
    }
    response_send_json(req->client_fd, 200, buf);
}

/* POST /api/dashboard/github/disconnect */
void dashboard_github_disconnect(http_request_t *req, const db_user_t *user)
{
    db_user_clear_github(user->id);
    response_send_json(req->client_fd, 200, "{\"ok\":true}");
}
