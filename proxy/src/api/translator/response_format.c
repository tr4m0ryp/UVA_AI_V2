#include "api/translator/translator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <json-c/json.h>

static char *generate_completion_id(void)
{
    char id[64];
    snprintf(id, sizeof(id), "chatcmpl-%lx%04x",
             (unsigned long)time(NULL), rand() & 0xFFFF);
    return strdup(id);
}

char *translate_response(const char *content, const char *model,
                         const char *completion_id)
{
    struct json_object *resp = json_object_new_object();
    char *cid = completion_id ? strdup(completion_id)
                              : generate_completion_id();

    json_object_object_add(resp, "id", json_object_new_string(cid));
    json_object_object_add(resp, "object",
        json_object_new_string("chat.completion"));
    json_object_object_add(resp, "created",
        json_object_new_int64((int64_t)time(NULL)));
    json_object_object_add(resp, "model",
        json_object_new_string(model ? model : "unknown"));

    struct json_object *choices = json_object_new_array();
    struct json_object *choice = json_object_new_object();
    json_object_object_add(choice, "index", json_object_new_int(0));

    struct json_object *message = json_object_new_object();
    json_object_object_add(message, "role",
        json_object_new_string("assistant"));
    json_object_object_add(message, "content",
        json_object_new_string(content ? content : ""));
    json_object_object_add(choice, "message", message);
    json_object_object_add(choice, "finish_reason",
        json_object_new_string("stop"));

    json_object_array_add(choices, choice);
    json_object_object_add(resp, "choices", choices);

    struct json_object *usage = json_object_new_object();
    json_object_object_add(usage, "prompt_tokens", json_object_new_int(0));
    json_object_object_add(usage, "completion_tokens",
        json_object_new_int(0));
    json_object_object_add(usage, "total_tokens", json_object_new_int(0));
    json_object_object_add(resp, "usage", usage);

    const char *str = json_object_to_json_string_ext(resp,
        JSON_C_TO_STRING_PLAIN);
    char *result = strdup(str);

    if (!completion_id) free(cid);
    json_object_put(resp);
    return result;
}

char *translate_stream_chunk(const char *delta_content, const char *model,
                             const char *completion_id, int index)
{
    struct json_object *chunk = json_object_new_object();

    json_object_object_add(chunk, "id",
        json_object_new_string(completion_id));
    json_object_object_add(chunk, "object",
        json_object_new_string("chat.completion.chunk"));
    json_object_object_add(chunk, "created",
        json_object_new_int64((int64_t)time(NULL)));
    json_object_object_add(chunk, "model",
        json_object_new_string(model ? model : "unknown"));

    struct json_object *choices = json_object_new_array();
    struct json_object *choice = json_object_new_object();
    json_object_object_add(choice, "index", json_object_new_int(index));

    struct json_object *delta = json_object_new_object();
    if (delta_content)
        json_object_object_add(delta, "content",
            json_object_new_string(delta_content));
    json_object_object_add(choice, "delta", delta);
    json_object_object_add(choice, "finish_reason",
        delta_content ? NULL : json_object_new_string("stop"));

    json_object_array_add(choices, choice);
    json_object_object_add(chunk, "choices", choices);

    const char *str = json_object_to_json_string_ext(chunk,
        JSON_C_TO_STRING_PLAIN);
    char *result = strdup(str);
    json_object_put(chunk);
    return result;
}

char *translate_stream_chunk_ex(const char *delta_content,
                                const char *delta_key, const char *model,
                                const char *completion_id, int index)
{
    struct json_object *chunk = json_object_new_object();

    json_object_object_add(chunk, "id",
        json_object_new_string(completion_id));
    json_object_object_add(chunk, "object",
        json_object_new_string("chat.completion.chunk"));
    json_object_object_add(chunk, "created",
        json_object_new_int64((int64_t)time(NULL)));
    json_object_object_add(chunk, "model",
        json_object_new_string(model ? model : "unknown"));

    struct json_object *choices = json_object_new_array();
    struct json_object *choice = json_object_new_object();
    json_object_object_add(choice, "index", json_object_new_int(index));

    struct json_object *delta = json_object_new_object();
    if (delta_content)
        json_object_object_add(delta, delta_key,
            json_object_new_string(delta_content));
    json_object_object_add(choice, "delta", delta);
    json_object_object_add(choice, "finish_reason",
        delta_content ? NULL : json_object_new_string("stop"));

    json_object_array_add(choices, choice);
    json_object_object_add(chunk, "choices", choices);

    const char *str = json_object_to_json_string_ext(chunk,
        JSON_C_TO_STRING_PLAIN);
    char *result = strdup(str);
    json_object_put(chunk);
    return result;
}

char *translate_response_ex(const char *content, const char *reasoning,
                            const char *model, const char *completion_id)
{
    struct json_object *resp = json_object_new_object();
    char *cid = completion_id ? strdup(completion_id)
                              : generate_completion_id();

    json_object_object_add(resp, "id", json_object_new_string(cid));
    json_object_object_add(resp, "object",
        json_object_new_string("chat.completion"));
    json_object_object_add(resp, "created",
        json_object_new_int64((int64_t)time(NULL)));
    json_object_object_add(resp, "model",
        json_object_new_string(model ? model : "unknown"));

    struct json_object *choices = json_object_new_array();
    struct json_object *choice = json_object_new_object();
    json_object_object_add(choice, "index", json_object_new_int(0));

    struct json_object *message = json_object_new_object();
    json_object_object_add(message, "role",
        json_object_new_string("assistant"));
    json_object_object_add(message, "content",
        json_object_new_string(content ? content : ""));
    if (reasoning && *reasoning)
        json_object_object_add(message, "reasoning_content",
            json_object_new_string(reasoning));
    json_object_object_add(choice, "message", message);
    json_object_object_add(choice, "finish_reason",
        json_object_new_string("stop"));

    json_object_array_add(choices, choice);
    json_object_object_add(resp, "choices", choices);

    struct json_object *usage = json_object_new_object();
    json_object_object_add(usage, "prompt_tokens", json_object_new_int(0));
    json_object_object_add(usage, "completion_tokens",
        json_object_new_int(0));
    json_object_object_add(usage, "total_tokens", json_object_new_int(0));
    json_object_object_add(resp, "usage", usage);

    const char *str = json_object_to_json_string_ext(resp,
        JSON_C_TO_STRING_PLAIN);
    char *result = strdup(str);

    if (!completion_id) free(cid);
    json_object_put(resp);
    return result;
}

const char *translate_stream_done(void)
{
    return "[DONE]";
}
