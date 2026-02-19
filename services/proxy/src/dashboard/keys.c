#include "dashboard.h"
#include "database.h"
#include "apikey.h"
#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>

/* response.c */
void response_send_json(int fd, int status, const char *json);
void response_send_error(int fd, int status, const char *message);

static struct json_object *key_to_json(const db_api_key_t *k)
{
    struct json_object *obj = json_object_new_object();
    json_object_object_add(obj, "id", json_object_new_int64(k->id));
    json_object_object_add(obj, "key_prefix",
        json_object_new_string(k->key_prefix));
    json_object_object_add(obj, "name",
        json_object_new_string(k->name));
    json_object_object_add(obj, "model",
        json_object_new_string(k->model));
    json_object_object_add(obj, "temperature",
        json_object_new_double(k->temperature));
    json_object_object_add(obj, "top_p",
        json_object_new_double(k->top_p));
    json_object_object_add(obj, "max_tokens",
        json_object_new_int(k->max_tokens));
    json_object_object_add(obj, "frequency_penalty",
        json_object_new_double(k->frequency_penalty));
    json_object_object_add(obj, "presence_penalty",
        json_object_new_double(k->presence_penalty));
    json_object_object_add(obj, "reasoning_effort",
        json_object_new_string(k->reasoning_effort));
    json_object_object_add(obj, "system_prompt",
        json_object_new_string(k->system_prompt));
    json_object_object_add(obj, "is_active",
        json_object_new_boolean(k->is_active));
    json_object_object_add(obj, "created_at",
        json_object_new_string(k->created_at));
    json_object_object_add(obj, "last_used",
        json_object_new_string(k->last_used[0] ? k->last_used : ""));
    return obj;
}

void dashboard_keys_list(http_request_t *req, const db_user_t *user)
{
    (void)req;
    db_api_key_t *keys = NULL;
    int count = 0;
    if (db_key_list_by_user(user->id, &keys, &count) != 0) {
        response_send_error(req->client_fd, 500,
            "Failed to list keys");
        return;
    }

    struct json_object *arr = json_object_new_array();
    for (int i = 0; i < count; i++)
        json_object_array_add(arr, key_to_json(&keys[i]));
    free(keys);

    const char *str = json_object_to_json_string_ext(arr,
        JSON_C_TO_STRING_PLAIN);
    response_send_json(req->client_fd, 200, str);
    json_object_put(arr);
}

static double json_get_double(struct json_object *obj, const char *key,
                               double def)
{
    struct json_object *v;
    if (json_object_object_get_ex(obj, key, &v))
        return json_object_get_double(v);
    return def;
}

static int json_get_int(struct json_object *obj, const char *key, int def)
{
    struct json_object *v;
    if (json_object_object_get_ex(obj, key, &v))
        return json_object_get_int(v);
    return def;
}

static const char *json_get_str(struct json_object *obj, const char *key,
                                 const char *def)
{
    struct json_object *v;
    if (json_object_object_get_ex(obj, key, &v))
        return json_object_get_string(v);
    return def;
}

void dashboard_keys_create(http_request_t *req, const db_user_t *user)
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

    const char *model = json_get_str(body, "model", NULL);
    if (!model || !model[0]) {
        json_object_put(body);
        response_send_error(req->client_fd, 400,
            "Missing 'model' field");
        return;
    }

    /* Generate key */
    char plaintext[128], hash[65], prefix[32];
    if (apikey_generate(plaintext, sizeof(plaintext),
                        hash, sizeof(hash),
                        prefix, sizeof(prefix)) != 0) {
        json_object_put(body);
        response_send_error(req->client_fd, 500,
            "Failed to generate API key");
        return;
    }

    /* Fill key record */
    db_api_key_t key;
    memset(&key, 0, sizeof(key));
    key.user_id = user->id;
    snprintf(key.key_hash, sizeof(key.key_hash), "%s", hash);
    snprintf(key.key_prefix, sizeof(key.key_prefix), "%s", prefix);
    snprintf(key.name, sizeof(key.name), "%s",
             json_get_str(body, "name", ""));
    snprintf(key.model, sizeof(key.model), "%s", model);
    key.temperature = json_get_double(body, "temperature", 0.5);
    key.top_p = json_get_double(body, "top_p", 0.5);
    key.max_tokens = json_get_int(body, "max_tokens", 4096);
    key.frequency_penalty = json_get_double(body, "frequency_penalty", 0.0);
    key.presence_penalty = json_get_double(body, "presence_penalty", 0.0);
    snprintf(key.reasoning_effort, sizeof(key.reasoning_effort), "%s",
             json_get_str(body, "reasoning_effort", "medium"));
    snprintf(key.system_prompt, sizeof(key.system_prompt), "%s",
             json_get_str(body, "system_prompt", ""));
    key.is_active = 1;
    json_object_put(body);

    int64_t key_id = 0;
    if (db_key_create(&key, &key_id) != 0) {
        response_send_error(req->client_fd, 500,
            "Failed to store API key");
        return;
    }

    /* Return plaintext key (shown once) */
    struct json_object *resp = json_object_new_object();
    json_object_object_add(resp, "id", json_object_new_int64(key_id));
    json_object_object_add(resp, "key",
        json_object_new_string(plaintext));
    json_object_object_add(resp, "key_prefix",
        json_object_new_string(prefix));
    json_object_object_add(resp, "model",
        json_object_new_string(key.model));

    const char *str = json_object_to_json_string_ext(resp,
        JSON_C_TO_STRING_PLAIN);
    response_send_json(req->client_fd, 201, str);
    json_object_put(resp);

    fprintf(stderr, "API key created: %s for %s (model: %s)\n",
            prefix, user->email, key.model);
}

void dashboard_keys_update(http_request_t *req, const db_user_t *user,
                           int64_t key_id)
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

    db_api_key_t key;
    memset(&key, 0, sizeof(key));
    snprintf(key.name, sizeof(key.name), "%s",
             json_get_str(body, "name", ""));
    snprintf(key.model, sizeof(key.model), "%s",
             json_get_str(body, "model", ""));
    key.temperature = json_get_double(body, "temperature", 0.5);
    key.top_p = json_get_double(body, "top_p", 0.5);
    key.max_tokens = json_get_int(body, "max_tokens", 4096);
    key.frequency_penalty = json_get_double(body, "frequency_penalty", 0.0);
    key.presence_penalty = json_get_double(body, "presence_penalty", 0.0);
    snprintf(key.reasoning_effort, sizeof(key.reasoning_effort), "%s",
             json_get_str(body, "reasoning_effort", "medium"));
    snprintf(key.system_prompt, sizeof(key.system_prompt), "%s",
             json_get_str(body, "system_prompt", ""));
    json_object_put(body);

    if (!key.model[0]) {
        response_send_error(req->client_fd, 400, "Missing 'model'");
        return;
    }

    if (db_key_update(key_id, user->id, &key) != 0) {
        response_send_error(req->client_fd, 404,
            "Key not found or access denied");
        return;
    }

    response_send_json(req->client_fd, 200, "{\"ok\":true}");
}

void dashboard_keys_delete(http_request_t *req, const db_user_t *user,
                           int64_t key_id)
{
    (void)req;
    if (db_key_delete(key_id, user->id) != 0) {
        response_send_error(req->client_fd, 404,
            "Key not found or access denied");
        return;
    }
    response_send_json(req->client_fd, 200, "{\"ok\":true}");
}

void dashboard_keys_toggle(http_request_t *req, const db_user_t *user,
                           int64_t key_id)
{
    (void)req;
    if (db_key_toggle(key_id, user->id) != 0) {
        response_send_error(req->client_fd, 404,
            "Key not found or access denied");
        return;
    }
    response_send_json(req->client_fd, 200, "{\"ok\":true}");
}

void dashboard_keys_usage(http_request_t *req, const db_user_t *user,
                          int64_t key_id)
{
    (void)req;

    /* Get summary */
    db_usage_summary_t summary;
    if (db_key_usage_summary(key_id, user->id, &summary) != 0) {
        response_send_error(req->client_fd, 500, "Failed to get usage");
        return;
    }

    /* Get daily breakdown (last 30 days) */
    db_usage_day_t *days = NULL;
    int day_count = 0;
    db_key_usage_daily(key_id, user->id, 30, &days, &day_count);

    struct json_object *resp = json_object_new_object();

    /* Summary */
    struct json_object *sum = json_object_new_object();
    json_object_object_add(sum, "total_requests",
        json_object_new_int(summary.total_requests));
    json_object_object_add(sum, "prompt_tokens",
        json_object_new_int(summary.prompt_tokens));
    json_object_object_add(sum, "completion_tokens",
        json_object_new_int(summary.completion_tokens));
    json_object_object_add(sum, "total_tokens",
        json_object_new_int(summary.total_tokens));
    json_object_object_add(resp, "summary", sum);

    /* Daily array */
    struct json_object *arr = json_object_new_array();
    for (int i = 0; i < day_count; i++) {
        struct json_object *d = json_object_new_object();
        json_object_object_add(d, "date",
            json_object_new_string(days[i].date));
        json_object_object_add(d, "requests",
            json_object_new_int(days[i].total_requests));
        json_object_object_add(d, "prompt_tokens",
            json_object_new_int(days[i].prompt_tokens));
        json_object_object_add(d, "completion_tokens",
            json_object_new_int(days[i].completion_tokens));
        json_object_object_add(d, "total_tokens",
            json_object_new_int(days[i].total_tokens));
        json_object_array_add(arr, d);
    }
    json_object_object_add(resp, "daily", arr);
    free(days);

    const char *str = json_object_to_json_string_ext(resp,
        JSON_C_TO_STRING_PLAIN);
    response_send_json(req->client_fd, 200, str);
    json_object_put(resp);
}
