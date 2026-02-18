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

void dashboard_keys_usage(http_request_t *req, const db_user_t *user,
                          int64_t key_id)
{
    (void)req;

    db_usage_stats_t stats;
    db_daily_stats_t *daily = NULL;
    int daily_count = 0;

    if (db_key_get_stats(key_id, user->id, &stats, &daily, &daily_count) != 0) {
        response_send_error(req->client_fd, 404,
            "Key not found or access denied");
        return;
    }

    struct json_object *obj = json_object_new_object();
    json_object_object_add(obj, "total_requests",
        json_object_new_int64(stats.total_requests));
    json_object_object_add(obj, "successful_requests",
        json_object_new_int64(stats.successful_requests));
    json_object_object_add(obj, "error_requests",
        json_object_new_int64(stats.error_requests));
    json_object_object_add(obj, "total_input_tokens",
        json_object_new_int64(stats.total_input_tokens));
    json_object_object_add(obj, "total_output_tokens",
        json_object_new_int64(stats.total_output_tokens));
    json_object_object_add(obj, "total_tokens",
        json_object_new_int64(stats.total_input_tokens +
                              stats.total_output_tokens));
    json_object_object_add(obj, "avg_latency_ms",
        json_object_new_int64(stats.avg_latency_ms));

    struct json_object *arr = json_object_new_array();
    for (int i = 0; i < daily_count; i++) {
        struct json_object *d = json_object_new_object();
        json_object_object_add(d, "day",
            json_object_new_string(daily[i].day));
        json_object_object_add(d, "requests",
            json_object_new_int(daily[i].requests));
        json_object_object_add(d, "input_tokens",
            json_object_new_int64(daily[i].input_tokens));
        json_object_object_add(d, "output_tokens",
            json_object_new_int64(daily[i].output_tokens));
        json_object_array_add(arr, d);
    }
    free(daily);

    json_object_object_add(obj, "daily", arr);

    const char *str = json_object_to_json_string_ext(obj,
        JSON_C_TO_STRING_PLAIN);
    response_send_json(req->client_fd, 200, str);
    json_object_put(obj);
}
