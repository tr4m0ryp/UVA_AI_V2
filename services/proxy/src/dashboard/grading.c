#include "dashboard.h"
#include "database.h"
#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>

/* response.c */
void response_send_json(int fd, int status, const char *json);
void response_send_error(int fd, int status, const char *message);

static const char *json_get_str(struct json_object *obj, const char *key,
                                 const char *def)
{
    struct json_object *v;
    if (json_object_object_get_ex(obj, key, &v))
        return json_object_get_string(v);
    return def;
}

static int json_get_int(struct json_object *obj, const char *key, int def)
{
    struct json_object *v;
    if (json_object_object_get_ex(obj, key, &v))
        return json_object_get_int(v);
    return def;
}

void dashboard_grading_list(http_request_t *req, const db_user_t *user)
{
    (void)req;
    db_grading_session_t *sessions = NULL;
    int count = 0;

    if (db_grading_list(user->id, &sessions, &count) != 0) {
        response_send_error(req->client_fd, 500,
            "Failed to list grading sessions");
        return;
    }

    struct json_object *arr = json_object_new_array();
    for (int i = 0; i < count; i++) {
        db_grading_session_t *s = &sessions[i];
        struct json_object *obj = json_object_new_object();
        json_object_object_add(obj, "id", json_object_new_int64(s->id));
        json_object_object_add(obj, "title",
            json_object_new_string(s->title));
        json_object_object_add(obj, "submission_count",
            json_object_new_int(s->submission_count));
        json_object_object_add(obj, "created_at",
            json_object_new_string(s->created_at));
        json_object_array_add(arr, obj);
    }
    free(sessions);

    const char *str = json_object_to_json_string_ext(arr,
        JSON_C_TO_STRING_PLAIN);
    response_send_json(req->client_fd, 200, str);
    json_object_put(arr);
}

void dashboard_grading_save(http_request_t *req, const db_user_t *user)
{
    if (!req->body || req->body_len == 0) {
        response_send_error(req->client_fd, 400, "Request body required");
        return;
    }

    struct json_object *body = json_tokener_parse(req->body);
    if (!body) {
        response_send_error(req->client_fd, 400, "Invalid JSON");
        return;
    }

    const char *title = json_get_str(body, "title", NULL);
    if (!title || !title[0]) {
        json_object_put(body);
        response_send_error(req->client_fd, 400, "Missing 'title' field");
        return;
    }

    int submission_count = json_get_int(body, "submission_count", 0);
    const char *results_json = json_get_str(body, "results_json", "{}");

    int64_t new_id = 0;
    int rc = db_grading_create(user->id, title, submission_count,
                               results_json, &new_id);
    json_object_put(body);

    if (rc != 0) {
        response_send_error(req->client_fd, 500,
            "Failed to save grading session");
        return;
    }

    struct json_object *resp = json_object_new_object();
    json_object_object_add(resp, "id", json_object_new_int64(new_id));
    const char *str = json_object_to_json_string_ext(resp,
        JSON_C_TO_STRING_PLAIN);
    response_send_json(req->client_fd, 201, str);
    json_object_put(resp);

    fprintf(stderr, "Grading session saved: id=%lld user=%s\n",
            (long long)new_id, user->email);
}

void dashboard_grading_get(http_request_t *req, const db_user_t *user,
                            int64_t id)
{
    (void)req;
    char *results_json = NULL;
    if (db_grading_get_json(id, user->id, &results_json) != 0) {
        response_send_error(req->client_fd, 404,
            "Grading session not found");
        return;
    }

    struct json_object *resp = json_object_new_object();
    json_object_object_add(resp, "id", json_object_new_int64(id));
    json_object_object_add(resp, "results_json",
        json_object_new_string(results_json));
    free(results_json);

    const char *str = json_object_to_json_string_ext(resp,
        JSON_C_TO_STRING_PLAIN);
    response_send_json(req->client_fd, 200, str);
    json_object_put(resp);
}

void dashboard_grading_delete(http_request_t *req, const db_user_t *user,
                               int64_t id)
{
    (void)req;
    if (db_grading_delete(id, user->id) != 0) {
        response_send_error(req->client_fd, 404,
            "Grading session not found or access denied");
        return;
    }
    response_send_json(req->client_fd, 200, "{}");
}
