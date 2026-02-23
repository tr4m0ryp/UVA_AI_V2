#include "dashboard/dashboard.h"
#include "dashboard/dashboard_internal.h"
#include "database/database.h"
#include "auth/browser_monitor.h"
#include "server/server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>

/* One active browser login session at a time. */
static browser_login_session_t *g_browser_session = NULL;

void dashboard_auth_browser_start(http_request_t *req)
{
    if (strcmp(req->method, "POST") != 0) {
        response_send_error(req->client_fd, 405, "Method not allowed");
        return;
    }

    /* Cancel any existing session */
    if (g_browser_session) {
        browser_login_cancel(g_browser_session);
        g_browser_session = NULL;
    }

    g_browser_session = browser_login_start(DEFAULT_BASE_URL);
    if (!g_browser_session) {
        response_send_error(req->client_fd, 500,
            "Failed to start browser login");
        return;
    }

    struct json_object *resp = json_object_new_object();
    json_object_object_add(resp, "status",
        json_object_new_string("pending"));
    json_object_object_add(resp, "message",
        json_object_new_string(
            "Browser opened. Log in to aichat.uva.nl and wait..."));

    const char *str = json_object_to_json_string_ext(resp,
        JSON_C_TO_STRING_PLAIN);
    response_send_json(req->client_fd, 200, str);
    json_object_put(resp);
}

void dashboard_auth_browser_status(http_request_t *req)
{
    if (strcmp(req->method, "GET") != 0) {
        response_send_error(req->client_fd, 405, "Method not allowed");
        return;
    }

    if (!g_browser_session) {
        response_send_error(req->client_fd, 400,
            "No browser login session active");
        return;
    }

    char cookie[MAX_COOKIE_LEN] = {0};
    char email[256] = {0};
    char name[256] = {0};
    int status = browser_login_poll(g_browser_session,
                                     cookie, sizeof(cookie),
                                     email, sizeof(email),
                                     name, sizeof(name));

    if (status == BROWSER_LOGIN_PENDING) {
        response_send_json(req->client_fd, 200, "{\"status\":\"pending\"}");
        return;
    }

    if (status != BROWSER_LOGIN_SUCCESS) {
        browser_login_cancel(g_browser_session);
        g_browser_session = NULL;

        const char *msg = (status == BROWSER_LOGIN_TIMEOUT)
            ? "Login timed out. Please try again."
            : "Login failed. Please try again.";
        struct json_object *resp = json_object_new_object();
        json_object_object_add(resp, "status",
            json_object_new_string("error"));
        json_object_object_add(resp, "message",
            json_object_new_string(msg));
        const char *str = json_object_to_json_string_ext(resp,
            JSON_C_TO_STRING_PLAIN);
        response_send_json(req->client_fd, 200, str);
        json_object_put(resp);
        return;
    }

    /* Success -- same completion as manual login */
    if (db_user_upsert(email, name, cookie) != 0) {
        browser_login_cancel(g_browser_session);
        g_browser_session = NULL;
        response_send_error(req->client_fd, 500,
            "Failed to create user record");
        return;
    }

    db_user_t user;
    if (db_user_find_by_email(email, &user) != 0) {
        browser_login_cancel(g_browser_session);
        g_browser_session = NULL;
        response_send_error(req->client_fd, 500,
            "Failed to find user after upsert");
        return;
    }

    char token[65];
    if (dashboard_generate_token(token, sizeof(token)) != 0) {
        browser_login_cancel(g_browser_session);
        g_browser_session = NULL;
        response_send_error(req->client_fd, 500,
            "Failed to generate auth token");
        return;
    }

    if (db_user_set_token(user.id, token) != 0) {
        browser_login_cancel(g_browser_session);
        g_browser_session = NULL;
        response_send_error(req->client_fd, 500,
            "Failed to save auth token");
        return;
    }

    browser_login_cancel(g_browser_session);
    g_browser_session = NULL;

    struct json_object *resp = json_object_new_object();
    json_object_object_add(resp, "status",
        json_object_new_string("success"));
    json_object_object_add(resp, "token",
        json_object_new_string(token));
    json_object_object_add(resp, "email",
        json_object_new_string(email));
    json_object_object_add(resp, "name",
        json_object_new_string(name));

    const char *str = json_object_to_json_string_ext(resp,
        JSON_C_TO_STRING_PLAIN);
    response_send_json(req->client_fd, 200, str);
    json_object_put(resp);

    fprintf(stderr, "Browser login complete: %s (%s)\n", email, name);
}

void dashboard_auth_browser_cancel(http_request_t *req)
{
    if (strcmp(req->method, "POST") != 0) {
        response_send_error(req->client_fd, 405, "Method not allowed");
        return;
    }

    if (g_browser_session) {
        browser_login_cancel(g_browser_session);
        g_browser_session = NULL;
    }

    response_send_json(req->client_fd, 200, "{\"ok\":true}");
}
