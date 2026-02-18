#include "translator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <json-c/json.h>

/*
 * Translate OpenAI chat completion request -> UvA request body.
 *
 * OpenAI format:
 *   {"model":"...","messages":[{"role":"user","content":"..."}],
 *    "temperature":0.7,"stream":true,...}
 *
 * UvA format (confirmed via traffic capture):
 *   {
 *     "id": "<thread-id>",
 *     "messages": [{"role":"user","content":"...","id":"msg-x",
 *                   "parts":[{"type":"text","text":"..."}]}],
 *     "message": <last message object>,
 *     "chatId": "<thread-id>",
 *     "messageId": "<last-msg-id>",
 *     "trigger": "submit-message",
 *     "flags": { "enforceInternetSearch":false, ... "isNewChat":false },
 *     "overrides": { "model":"gpt-4.1", "personaId":"" },
 *     "requestTime": "2026-02-18T08:00:00.000Z"
 *   }
 */
static struct json_object *make_uva_message(const char *role,
                                             const char *content,
                                             const char *msg_id)
{
    struct json_object *msg = json_object_new_object();
    json_object_object_add(msg, "role", json_object_new_string(role));
    json_object_object_add(msg, "content",
        json_object_new_string(content));
    json_object_object_add(msg, "id", json_object_new_string(msg_id));

    struct json_object *parts = json_object_new_array();
    struct json_object *part = json_object_new_object();
    json_object_object_add(part, "type", json_object_new_string("text"));
    json_object_object_add(part, "text",
        json_object_new_string(content));
    json_object_array_add(parts, part);
    json_object_object_add(msg, "parts", parts);
    return msg;
}

char *translate_request(const char *openai_body, const char *thread_id)
{
    struct json_object *req = json_tokener_parse(openai_body);
    if (!req) return NULL;

    struct json_object *uva = json_object_new_object();

    /* id + chatId */
    json_object_object_add(uva, "id", json_object_new_string(thread_id));
    json_object_object_add(uva, "chatId",
        json_object_new_string(thread_id));

    /* Model for overrides */
    const char *model = "gpt-4.1";
    struct json_object *model_obj;
    if (json_object_object_get_ex(req, "model", &model_obj))
        model = json_object_get_string(model_obj);
    const char *uva_model = models_to_uva(model);

    /* Convert OpenAI messages to UvA format with parts */
    struct json_object *msgs;
    struct json_object *uva_msgs = json_object_new_array();
    struct json_object *last_msg = NULL;
    const char *last_msg_id = "msg-proxy-001";

    if (json_object_object_get_ex(req, "messages", &msgs)) {
        int len = (int)json_object_array_length(msgs);
        for (int i = 0; i < len; i++) {
            struct json_object *m = json_object_array_get_idx(msgs, i);
            struct json_object *role_o, *content_o;
            const char *r = "user", *c = "";
            if (json_object_object_get_ex(m, "role", &role_o))
                r = json_object_get_string(role_o);
            if (json_object_object_get_ex(m, "content", &content_o))
                c = json_object_get_string(content_o);

            char mid[32];
            snprintf(mid, sizeof(mid), "msg-proxy-%03d", i + 1);
            struct json_object *um = make_uva_message(r, c, mid);
            json_object_array_add(uva_msgs, um);

            if (i == len - 1) {
                last_msg = make_uva_message(r, c, mid);
                last_msg_id = strdup(mid);
            }
        }
    }

    json_object_object_add(uva, "messages", uva_msgs);
    if (last_msg)
        json_object_object_add(uva, "message", last_msg);

    /* messageId + trigger */
    json_object_object_add(uva, "messageId",
        json_object_new_string(last_msg_id));
    json_object_object_add(uva, "trigger",
        json_object_new_string("submit-message"));

    /* flags */
    struct json_object *flags = json_object_new_object();
    json_object_object_add(flags, "enforceInternetSearch",
        json_object_new_boolean(0));
    json_object_object_add(flags, "enforceArtifactCreation",
        json_object_new_boolean(0));
    json_object_object_add(flags, "enforceImageGeneration",
        json_object_new_boolean(0));
    json_object_object_add(flags, "regenerate",
        json_object_new_boolean(0));
    json_object_object_add(flags, "continue",
        json_object_new_boolean(0));
    json_object_object_add(flags, "isNewChat",
        json_object_new_boolean(0));
    json_object_object_add(uva, "flags", flags);

    /* overrides */
    struct json_object *overrides = json_object_new_object();
    json_object_object_add(overrides, "model",
        json_object_new_string(uva_model));
    json_object_object_add(overrides, "personaId",
        json_object_new_string(""));

    /* Pass through optional parameters from OpenAI body */
    struct json_object *param;
    if (json_object_object_get_ex(req, "temperature", &param))
        json_object_object_add(overrides, "temperature",
            json_object_get(param));
    if (json_object_object_get_ex(req, "top_p", &param))
        json_object_object_add(overrides, "topP",
            json_object_get(param));
    if (json_object_object_get_ex(req, "max_tokens", &param))
        json_object_object_add(overrides, "maxTokens",
            json_object_get(param));
    if (json_object_object_get_ex(req, "frequency_penalty", &param))
        json_object_object_add(overrides, "frequencyPenalty",
            json_object_get(param));
    if (json_object_object_get_ex(req, "presence_penalty", &param))
        json_object_object_add(overrides, "presencePenalty",
            json_object_get(param));
    if (json_object_object_get_ex(req, "reasoning_effort", &param))
        json_object_object_add(overrides, "reasoningEffort",
            json_object_get(param));

    json_object_object_add(uva, "overrides", overrides);

    /* requestTime */
    time_t now = time(NULL);
    struct tm *tm = gmtime(&now);
    char timebuf[64];
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%dT%H:%M:%S.000Z", tm);
    json_object_object_add(uva, "requestTime",
        json_object_new_string(timebuf));

    const char *str = json_object_to_json_string_ext(uva,
        JSON_C_TO_STRING_PLAIN);
    char *result = strdup(str);

    json_object_put(uva);
    json_object_put(req);
    return result;
}

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
    char *cid = completion_id ? strdup(completion_id) : generate_completion_id();

    json_object_object_add(resp, "id", json_object_new_string(cid));
    json_object_object_add(resp, "object",
        json_object_new_string("chat.completion"));
    json_object_object_add(resp, "created",
        json_object_new_int64((int64_t)time(NULL)));
    json_object_object_add(resp, "model",
        json_object_new_string(model ? model : "unknown"));

    /* choices */
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

    /* usage (estimated) */
    struct json_object *usage = json_object_new_object();
    json_object_object_add(usage, "prompt_tokens", json_object_new_int(0));
    json_object_object_add(usage, "completion_tokens", json_object_new_int(0));
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
    if (delta_content) {
        json_object_object_add(delta, "content",
            json_object_new_string(delta_content));
    }
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

const char *translate_stream_done(void)
{
    return "[DONE]";
}
