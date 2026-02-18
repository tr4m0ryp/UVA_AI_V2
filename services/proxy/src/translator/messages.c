#include "translator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <json-c/json.h>

/*
 * Generate a UUID v4 string into buf (must be >= 37 bytes).
 * The UvA server requires UUID-format IDs for both thread and message IDs.
 */
static void gen_uuid(char *buf)
{
    static const char hex[] = "0123456789abcdef";
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    srand((unsigned)ts.tv_nsec ^ (unsigned)ts.tv_sec ^ (unsigned)(uintptr_t)buf);

    int pos = 0;
    for (int i = 0; i < 8; i++)  buf[pos++] = hex[rand() & 0xf];
    buf[pos++] = '-';
    for (int i = 0; i < 4; i++)  buf[pos++] = hex[rand() & 0xf];
    buf[pos++] = '-';
    buf[pos++] = '4';
    for (int i = 0; i < 3; i++)  buf[pos++] = hex[rand() & 0xf];
    buf[pos++] = '-';
    buf[pos++] = hex[8 + (rand() & 0x3)];
    for (int i = 0; i < 3; i++)  buf[pos++] = hex[rand() & 0xf];
    buf[pos++] = '-';
    for (int i = 0; i < 12; i++) buf[pos++] = hex[rand() & 0xf];
    buf[pos] = '\0';
}

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

    /* id - the chat/thread ID */
    json_object_object_add(uva, "id", json_object_new_string(thread_id));

    /* Model for overrides */
    const char *model = "gpt-4.1";
    struct json_object *model_obj;
    if (json_object_object_get_ex(req, "model", &model_obj))
        model = json_object_get_string(model_obj);
    const char *uva_model = models_to_uva(model);

    /*
     * The UvA API only accepts a single "message" (the last turn).
     * For multi-turn conversations, serialize prior messages as a text
     * prefix so the model sees the full conversation context.
     * Message IDs must be UUID format or UvA returns HTTP 500.
     */
    struct json_object *msgs;
    struct json_object *last_msg = NULL;
    char last_msg_id[37];
    gen_uuid(last_msg_id);

    if (json_object_object_get_ex(req, "messages", &msgs)) {
        int len = (int)json_object_array_length(msgs);
        if (len > 0) {
            struct json_object *m = json_object_array_get_idx(msgs,
                                                               (size_t)(len - 1));
            struct json_object *role_o, *content_o;
            const char *r = "user", *c = "";
            if (json_object_object_get_ex(m, "role", &role_o))
                r = json_object_get_string(role_o);
            if (json_object_object_get_ex(m, "content", &content_o))
                c = json_object_get_string(content_o);

            /* Build conversation history prefix from messages[0..n-2] */
            if (len > 1) {
                /* Estimate buffer: 64 bytes overhead + content per message */
                size_t hist_cap = 256;
                for (int i = 0; i < len - 1; i++) {
                    struct json_object *hm = json_object_array_get_idx(msgs,
                        (size_t)i);
                    struct json_object *hc;
                    if (json_object_object_get_ex(hm, "content", &hc))
                        hist_cap += strlen(json_object_get_string(hc)) + 32;
                }
                char *hist = malloc(hist_cap);
                if (hist) {
                    int hpos = 0;
                    hpos += snprintf(hist + hpos, hist_cap - (size_t)hpos,
                        "[Previous conversation]\n");
                    for (int i = 0; i < len - 1; i++) {
                        struct json_object *hm = json_object_array_get_idx(msgs,
                            (size_t)i);
                        struct json_object *hr, *hc;
                        const char *hrole = "user", *hcont = "";
                        if (json_object_object_get_ex(hm, "role", &hr))
                            hrole = json_object_get_string(hr);
                        if (json_object_object_get_ex(hm, "content", &hc))
                            hcont = json_object_get_string(hc);
                        /* Capitalize role for readability */
                        char cap_role[16];
                        snprintf(cap_role, sizeof(cap_role), "%s", hrole);
                        if (cap_role[0] >= 'a' && cap_role[0] <= 'z')
                            cap_role[0] = (char)(cap_role[0] - 32);
                        hpos += snprintf(hist + hpos, hist_cap - (size_t)hpos,
                            "%s: %s\n", cap_role, hcont);
                    }
                    hpos += snprintf(hist + hpos, hist_cap - (size_t)hpos,
                        "[End of previous conversation]\n\n");

                    /* Combine history + current message content */
                    size_t combined_len = (size_t)hpos + strlen(c) + 4;
                    char *combined = malloc(combined_len);
                    if (combined) {
                        snprintf(combined, combined_len, "%s%s", hist, c);
                        last_msg = make_uva_message(r, combined, last_msg_id);
                        free(combined);
                    }
                    free(hist);
                }
            }

            if (!last_msg)
                last_msg = make_uva_message(r, c, last_msg_id);
        }
    }

    if (last_msg)
        json_object_object_add(uva, "message", last_msg);

    /* flags - matches UvA frontend prepareSendMessagesRequest */
    struct json_object *flags = json_object_new_object();
    json_object_object_add(flags, "studyMode",
        json_object_new_boolean(0));
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
        json_object_new_boolean(1));
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

/* translate_response, translate_stream_chunk, translate_stream_done
 * are in response_builder.c */
