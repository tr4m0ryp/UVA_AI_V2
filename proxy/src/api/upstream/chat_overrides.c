#include "core/server/server.h"
#include "core/database/database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>

/* Build a modified request body with API key overrides applied.
 * Injects system prompt and overrides model if API key provides them. */
char *apply_apikey_overrides(const char *body,
                              const db_api_key_t *ak,
                              char *model_buf, size_t model_size)
{
    struct json_object *parsed = json_tokener_parse(body);
    if (!parsed) return NULL;

    /* Use client's model if present, fall back to key's model */
    struct json_object *existing_model;
    if (json_object_object_get_ex(parsed, "model", &existing_model)) {
        snprintf(model_buf, model_size, "%s",
            json_object_get_string(existing_model));
    } else {
        snprintf(model_buf, model_size, "%s", ak->model);
        json_object_object_add(parsed, "model",
            json_object_new_string(ak->model));
    }

    /* Inject system prompt at front of messages */
    if (ak->system_prompt[0]) {
        struct json_object *msgs;
        if (json_object_object_get_ex(parsed, "messages", &msgs)) {
            struct json_object *sys_msg = json_object_new_object();
            json_object_object_add(sys_msg, "role",
                json_object_new_string("system"));
            json_object_object_add(sys_msg, "content",
                json_object_new_string(ak->system_prompt));
            json_object_array_put_idx(msgs,
                (size_t)json_object_array_length(msgs), NULL);
            /* Shift all elements right by 1 */
            int len = (int)json_object_array_length(msgs);
            for (int i = len - 1; i > 0; i--)
                json_object_array_put_idx(msgs, (size_t)i,
                    json_object_get(json_object_array_get_idx(msgs,
                        (size_t)(i - 1))));
            json_object_array_put_idx(msgs, 0, sys_msg);
        }
    }

    /* Pass through parameters for translate_request to pick up */
    if (ak->temperature >= 0)
        json_object_object_add(parsed, "temperature",
            json_object_new_double(ak->temperature));
    if (ak->top_p >= 0)
        json_object_object_add(parsed, "top_p",
            json_object_new_double(ak->top_p));
    if (ak->max_tokens > 0)
        json_object_object_add(parsed, "max_tokens",
            json_object_new_int(ak->max_tokens));
    if (ak->frequency_penalty != 0.0)
        json_object_object_add(parsed, "frequency_penalty",
            json_object_new_double(ak->frequency_penalty));
    if (ak->presence_penalty != 0.0)
        json_object_object_add(parsed, "presence_penalty",
            json_object_new_double(ak->presence_penalty));
    if (ak->reasoning_effort[0])
        json_object_object_add(parsed, "reasoning_effort",
            json_object_new_string(ak->reasoning_effort));

    const char *str = json_object_to_json_string_ext(parsed,
        JSON_C_TO_STRING_PLAIN);
    char *result = strdup(str);
    json_object_put(parsed);
    return result;
}
