#include "ui/dashboard/dashboard.h"
#include "core/database/database.h"
#include "core/database/grading.h"
#include "core/server/server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>

/* response.c */
void response_send_json(int fd, int status, const char *json);
void response_send_error(int fd, int status, const char *message);

static const char *jstr(struct json_object *obj, const char *key,
                        const char *def)
{
    struct json_object *v;
    if (json_object_object_get_ex(obj, key, &v))
        return json_object_get_string(v);
    return def;
}

static double jdbl(struct json_object *obj, const char *key, double def)
{
    struct json_object *v;
    if (json_object_object_get_ex(obj, key, &v))
        return json_object_get_double(v);
    return def;
}

static struct json_object *session_to_json(const db_grading_session_t *s)
{
    struct json_object *obj = json_object_new_object();
    json_object_object_add(obj, "id", json_object_new_int64(s->id));
    json_object_object_add(obj, "name",
        json_object_new_string(s->name));
    json_object_object_add(obj, "submission_count",
        json_object_new_int(s->submission_count));
    json_object_object_add(obj, "avg_percentage",
        json_object_new_double(s->avg_percentage));
    json_object_object_add(obj, "status",
        json_object_new_string(s->status));
    json_object_object_add(obj, "created_at",
        json_object_new_string(s->created_at));
    return obj;
}

static struct json_object *result_to_json(const db_grading_result_t *r)
{
    struct json_object *obj = json_object_new_object();
    json_object_object_add(obj, "id", json_object_new_int64(r->id));
    json_object_object_add(obj, "session_id",
        json_object_new_int64(r->session_id));
    json_object_object_add(obj, "student_name",
        json_object_new_string(r->student_name));
    json_object_object_add(obj, "student_id_str",
        json_object_new_string(r->student_id_str));
    json_object_object_add(obj, "file_name",
        json_object_new_string(r->file_name));
    json_object_object_add(obj, "total_score",
        json_object_new_double(r->total_score));
    json_object_object_add(obj, "total_max",
        json_object_new_double(r->total_max));
    json_object_object_add(obj, "percentage",
        json_object_new_double(r->percentage));
    json_object_object_add(obj, "overall_feedback",
        json_object_new_string(r->overall_feedback));

    /* Parse criteria_json string back into a JSON array */
    struct json_object *crit = json_tokener_parse(r->criteria_json);
    if (crit)
        json_object_object_add(obj, "criteria", crit);
    else
        json_object_object_add(obj, "criteria",
            json_object_new_array());

    json_object_object_add(obj, "status",
        json_object_new_string(r->status));
    json_object_object_add(obj, "error_text",
        json_object_new_string(r->error_text));
    return obj;
}

void dashboard_grading_list(http_request_t *req, const db_user_t *user)
{
    (void)req;
    db_grading_session_t *sessions = NULL;
    int count = 0;
    if (db_grading_session_list(user->id, &sessions, &count) != 0) {
        response_send_error(req->client_fd, 500,
            "Failed to list grading sessions");
        return;
    }

    struct json_object *arr = json_object_new_array();
    for (int i = 0; i < count; i++)
        json_object_array_add(arr, session_to_json(&sessions[i]));
    free(sessions);

    const char *str = json_object_to_json_string_ext(arr,
        JSON_C_TO_STRING_PLAIN);
    response_send_json(req->client_fd, 200, str);
    json_object_put(arr);
}

void dashboard_grading_create(http_request_t *req, const db_user_t *user)
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

    const char *name = jstr(body, "name", "Untitled Session");
    const char *assignment = jstr(body, "assignment_text", "");
    const char *rubric = jstr(body, "rubric_text", "");
    const char *example = jstr(body, "example_text", "");

    int64_t session_id = 0;
    if (db_grading_session_create(user->id, name, assignment, rubric,
                                  example, &session_id) != 0) {
        json_object_put(body);
        response_send_error(req->client_fd, 500,
            "Failed to create grading session");
        return;
    }
    json_object_put(body);

    struct json_object *resp = json_object_new_object();
    json_object_object_add(resp, "id", json_object_new_int64(session_id));
    const char *str = json_object_to_json_string_ext(resp,
        JSON_C_TO_STRING_PLAIN);
    response_send_json(req->client_fd, 201, str);
    json_object_put(resp);
}

void dashboard_grading_get(http_request_t *req, const db_user_t *user,
                           int64_t session_id)
{
    (void)req;
    db_grading_session_t session;
    if (db_grading_session_get(session_id, user->id, &session) != 0) {
        response_send_error(req->client_fd, 404,
            "Session not found");
        return;
    }

    /* Get results for this session */
    db_grading_result_t *results = NULL;
    int rcount = 0;
    db_grading_result_list(session_id, &results, &rcount);

    struct json_object *resp = session_to_json(&session);
    struct json_object *rarr = json_object_new_array();
    for (int i = 0; i < rcount; i++)
        json_object_array_add(rarr, result_to_json(&results[i]));
    free(results);
    json_object_object_add(resp, "results", rarr);

    const char *str = json_object_to_json_string_ext(resp,
        JSON_C_TO_STRING_PLAIN);
    response_send_json(req->client_fd, 200, str);
    json_object_put(resp);
}

void dashboard_grading_delete(http_request_t *req, const db_user_t *user,
                              int64_t session_id)
{
    (void)req;
    if (db_grading_session_delete(session_id, user->id) != 0) {
        response_send_error(req->client_fd, 404,
            "Session not found or access denied");
        return;
    }
    response_send_json(req->client_fd, 200, "{\"ok\":true}");
}

void dashboard_grading_save_result(http_request_t *req,
                                   const db_user_t *user,
                                   int64_t session_id)
{
    /* Verify session belongs to user */
    db_grading_session_t session;
    if (db_grading_session_get(session_id, user->id, &session) != 0) {
        response_send_error(req->client_fd, 404, "Session not found");
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

    db_grading_result_t r;
    memset(&r, 0, sizeof(r));
    r.session_id = session_id;
    snprintf(r.student_name, sizeof(r.student_name), "%s",
             jstr(body, "student_name", ""));
    snprintf(r.student_id_str, sizeof(r.student_id_str), "%s",
             jstr(body, "student_id_str", ""));
    snprintf(r.file_name, sizeof(r.file_name), "%s",
             jstr(body, "file_name", ""));
    r.total_score = jdbl(body, "total_score", 0);
    r.total_max = jdbl(body, "total_max", 0);
    r.percentage = jdbl(body, "percentage", 0);
    snprintf(r.overall_feedback, sizeof(r.overall_feedback), "%s",
             jstr(body, "overall_feedback", ""));

    /* Store criteria as JSON string */
    struct json_object *crit;
    if (json_object_object_get_ex(body, "criteria_json", &crit))
        snprintf(r.criteria_json, sizeof(r.criteria_json), "%s",
                 json_object_to_json_string(crit));
    else
        snprintf(r.criteria_json, sizeof(r.criteria_json), "[]");

    snprintf(r.status, sizeof(r.status), "%s",
             jstr(body, "status", "done"));
    snprintf(r.error_text, sizeof(r.error_text), "%s",
             jstr(body, "error_text", ""));
    json_object_put(body);

    if (db_grading_result_save(session_id, &r) != 0) {
        response_send_error(req->client_fd, 500, "Failed to save result");
        return;
    }

    response_send_json(req->client_fd, 201, "{\"ok\":true}");
}

void dashboard_grading_finish(http_request_t *req, const db_user_t *user,
                              int64_t session_id)
{
    /* Verify ownership */
    db_grading_session_t session;
    if (db_grading_session_get(session_id, user->id, &session) != 0) {
        response_send_error(req->client_fd, 404, "Session not found");
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

    struct json_object *v;
    int count = 0;
    double avg = 0;
    const char *status = "completed";

    if (json_object_object_get_ex(body, "submission_count", &v))
        count = json_object_get_int(v);
    if (json_object_object_get_ex(body, "avg_percentage", &v))
        avg = json_object_get_double(v);
    if (json_object_object_get_ex(body, "status", &v))
        status = json_object_get_string(v);
    json_object_put(body);

    if (db_grading_session_update_stats(session_id, count, avg,
                                        status) != 0) {
        response_send_error(req->client_fd, 500,
            "Failed to update session");
        return;
    }

    response_send_json(req->client_fd, 200, "{\"ok\":true}");
}
