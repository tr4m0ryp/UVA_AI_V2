#include "ui/dashboard/dashboard.h"
#include "ui/dashboard/dashboard_internal.h"
#include "core/database/database.h"
#include "api/upstream/upstream.h"
#include "core/server/server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/rand.h>
#include <json-c/json.h>

int dashboard_generate_token(char *out, size_t size)
{
    unsigned char raw[32];
    if (RAND_bytes(raw, 32) != 1) return -1;
    if (size < 65) return -1;
    for (int i = 0; i < 32; i++)
        snprintf(out + i * 2, 3, "%02x", raw[i]);
    return 0;
}

void dashboard_auth_login(http_request_t *req)
{
    if (strcmp(req->method, "POST") != 0) {
        response_send_error(req->client_fd, 405, "Method not allowed");
        return;
    }

    if (!req->body || req->body_len == 0) {
        response_send_error(req->client_fd, 400, "Request body required");
        return;
    }

    struct json_object *body = json_tokener_parse(req->body);
    if (!body) {
        response_send_error(req->client_fd, 400, "Invalid JSON");
        return;
    }

    struct json_object *cookie_obj;
    if (!json_object_object_get_ex(body, "cookie", &cookie_obj)) {
        json_object_put(body);
        response_send_error(req->client_fd, 400,
            "Missing 'cookie' field");
        return;
    }

    const char *cookie = json_object_get_string(cookie_obj);
    if (!cookie || !cookie[0]) {
        json_object_put(body);
        response_send_error(req->client_fd, 400, "Empty cookie");
        return;
    }

    /* Validate cookie against UvA */
    char email[256] = {0};
    char name[256] = {0};
    int valid = upstream_validate_cookie(cookie, email, sizeof(email),
                                         name, sizeof(name));
    json_object_put(body);

    if (!valid || !email[0]) {
        response_send_error(req->client_fd, 401,
            "Invalid session cookie. Make sure you copied the full "
            "cookie from aichat.uva.nl");
        return;
    }

    /* Upsert user */
    if (db_user_upsert(email, name, cookie) != 0) {
        response_send_error(req->client_fd, 500,
            "Failed to create user record");
        return;
    }

    /* Find user to get ID */
    db_user_t user;
    if (db_user_find_by_email(email, &user) != 0) {
        response_send_error(req->client_fd, 500,
            "Failed to find user after upsert");
        return;
    }

    /* Generate dashboard token */
    char token[65];
    if (dashboard_generate_token(token, sizeof(token)) != 0) {
        response_send_error(req->client_fd, 500,
            "Failed to generate auth token");
        return;
    }

    if (db_user_set_token(user.id, token) != 0) {
        response_send_error(req->client_fd, 500,
            "Failed to save auth token");
        return;
    }

    /* Build response */
    struct json_object *resp = json_object_new_object();
    json_object_object_add(resp, "token", json_object_new_string(token));
    json_object_object_add(resp, "email", json_object_new_string(email));
    json_object_object_add(resp, "name", json_object_new_string(name));

    const char *str = json_object_to_json_string_ext(resp,
        JSON_C_TO_STRING_PLAIN);
    response_send_json(req->client_fd, 200, str);
    json_object_put(resp);

    fprintf(stderr, "Dashboard login: %s (%s)\n", email, name);
}

void dashboard_auth_me(http_request_t *req)
{
    if (strcmp(req->method, "GET") != 0) {
        response_send_error(req->client_fd, 405, "Method not allowed");
        return;
    }

    const char *auth = request_get_header(req, "Authorization");
    if (!auth || strncmp(auth, "Bearer ", 7) != 0) {
        response_send_error(req->client_fd, 401, "Unauthorized");
        return;
    }

    db_user_t user;
    if (db_user_find_by_token(auth + 7, &user) != 0) {
        response_send_error(req->client_fd, 401, "Invalid token");
        return;
    }

    struct json_object *resp = json_object_new_object();
    json_object_object_add(resp, "email",
        json_object_new_string(user.email));
    json_object_object_add(resp, "name",
        json_object_new_string(user.name));

    const char *str = json_object_to_json_string_ext(resp,
        JSON_C_TO_STRING_PLAIN);
    response_send_json(req->client_fd, 200, str);
    json_object_put(resp);
}

void dashboard_auth_logout(http_request_t *req)
{
    if (strcmp(req->method, "POST") != 0) {
        response_send_error(req->client_fd, 405, "Method not allowed");
        return;
    }

    const char *auth = request_get_header(req, "Authorization");
    if (auth && strncmp(auth, "Bearer ", 7) == 0) {
        db_user_t user;
        if (db_user_find_by_token(auth + 7, &user) == 0)
            db_user_clear_token(user.id);
    }

    response_send_json(req->client_fd, 200, "{\"ok\":true}");
}
