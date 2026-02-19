#include "translator.h"
#include "upstream.h"   /* buffer_t */
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

/* Translate OpenAI chat completion request -> UvA request body. */
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
     * UvA's API accepts only a single "message" object (not full history).
     * We fold system messages and assistant context into the final user
     * message so the model sees the complete prompt. Message IDs must be
     * UUID format or UvA returns HTTP 500.
     */
    struct json_object *msgs;
    struct json_object *last_msg = NULL;
    char last_msg_id[37];
    gen_uuid(last_msg_id);

    if (json_object_object_get_ex(req, "messages", &msgs)) {
        int len = (int)json_object_array_length(msgs);

        /* Collect system/context messages and the final user message */
        buffer_t combined;
        buffer_init(&combined);
        const char *final_role = "user";
        const char *final_content = "";

        for (int i = 0; i < len; i++) {
            struct json_object *m = json_object_array_get_idx(msgs,
                                                               (size_t)i);
            struct json_object *role_o, *content_o;
            const char *r = "", *c = "";
            if (json_object_object_get_ex(m, "role", &role_o))
                r = json_object_get_string(role_o);
            if (json_object_object_get_ex(m, "content", &content_o)) {
                if (json_object_is_type(content_o, json_type_string)) {
                    c = json_object_get_string(content_o);
                } else if (json_object_is_type(content_o, json_type_array)) {
                    /* Extract text from content parts array */
                    static __thread char parts_buf[8192];
                    parts_buf[0] = '\0';
                    size_t ppos = 0;
                    int plen = (int)json_object_array_length(content_o);
                    for (int k = 0; k < plen; k++) {
                        struct json_object *part =
                            json_object_array_get_idx(content_o, (size_t)k);
                        struct json_object *txt_o;
                        if (json_object_object_get_ex(part, "text", &txt_o)) {
                            const char *t = json_object_get_string(txt_o);
                            if (t) {
                                size_t tl = strlen(t);
                                if (ppos + tl < sizeof(parts_buf) - 1) {
                                    memcpy(parts_buf + ppos, t, tl);
                                    ppos += tl;
                                }
                            }
                        }
                    }
                    parts_buf[ppos] = '\0';
                    c = parts_buf;
                }
            }

            if (i == len - 1) {
                /* Last message: preserve its role */
                final_role = r;
                final_content = c;
            } else if (c && c[0]) {
                /* Prefix from system/assistant/user context messages */
                if (combined.size > 0)
                    buffer_append(&combined, "\n\n", 2);
                buffer_append(&combined, c, strlen(c));
            }
        }

        /* If there were prefix messages, combine them with final */
        if (combined.size > 0 && final_content[0]) {
            buffer_append(&combined, "\n\n", 2);
            buffer_append(&combined, final_content, strlen(final_content));
            last_msg = make_uva_message(final_role, combined.data,
                                        last_msg_id);
        } else {
            last_msg = make_uva_message(final_role, final_content,
                                        last_msg_id);
        }
        buffer_free(&combined);
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
 * are now in response_format.c */
