#include "translator/translator.h"
#include "utils/idgen.h"
#include "upstream/upstream.h"   /* buffer_t */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <json-c/json.h>

/*
 * Build a UvA-format message object.
 * Uses parts-only format (no content field) matching the real frontend.
 */
static struct json_object *make_uva_message(const char *role,
                                             const char *content,
                                             const char *msg_id)
{
    struct json_object *msg = json_object_new_object();

    struct json_object *parts = json_object_new_array();
    struct json_object *part = json_object_new_object();
    json_object_object_add(part, "type", json_object_new_string("text"));
    json_object_object_add(part, "text",
        json_object_new_string(content));
    json_object_array_add(parts, part);
    json_object_object_add(msg, "parts", parts);

    json_object_object_add(msg, "id", json_object_new_string(msg_id));
    json_object_object_add(msg, "role", json_object_new_string(role));
    return msg;
}

/* Build the standard flags object. */
static struct json_object *make_flags(int is_new_chat)
{
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
        json_object_new_boolean(is_new_chat));
    return flags;
}

/* Append ISO 8601 requestTime to a JSON object. */
static void add_request_time(struct json_object *obj)
{
    time_t now = time(NULL);
    struct tm *tm = gmtime(&now);
    char timebuf[64];
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%dT%H:%M:%S.000Z", tm);
    json_object_object_add(obj, "requestTime",
        json_object_new_string(timebuf));
}

/*
 * Extract text content from an OpenAI message content field.
 * Handles both string and array-of-parts formats.
 * Returns pointer to content string (may be thread-local static buffer).
 */
static const char *extract_content(struct json_object *content_o)
{
    if (!content_o) return "";

    if (json_object_is_type(content_o, json_type_string))
        return json_object_get_string(content_o);

    if (json_object_is_type(content_o, json_type_array)) {
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
        return parts_buf;
    }

    return "";
}

char *translate_request(const char *openai_body, const char *thread_id)
{
    struct json_object *req = json_tokener_parse(openai_body);
    if (!req) return NULL;

    struct json_object *uva = json_object_new_object();

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
     * message so the model sees the complete prompt.
     */
    struct json_object *msgs;
    struct json_object *last_msg = NULL;
    char last_msg_id[NANOID_MSG_LEN + 1];
    gen_nanoid(last_msg_id, NANOID_MSG_LEN);

    if (json_object_object_get_ex(req, "messages", &msgs)) {
        int len = (int)json_object_array_length(msgs);

        buffer_t combined;
        buffer_init(&combined);
        const char *final_content = "";

        for (int i = 0; i < len; i++) {
            struct json_object *m = json_object_array_get_idx(msgs,
                                                               (size_t)i);
            struct json_object *role_o, *content_o;
            const char *r = "", *c = "";
            if (json_object_object_get_ex(m, "role", &role_o))
                r = json_object_get_string(role_o);
            if (json_object_object_get_ex(m, "content", &content_o))
                c = extract_content(content_o);

            if (i == len - 1) {
                final_content = c;
            } else if (c && c[0]) {
                if (combined.size > 0)
                    buffer_append(&combined, "\n\n", 2);

                /* Prepend role label for context preservation */
                const char *label = "[USER]";
                if (r && strcmp(r, "system") == 0)
                    label = "[SYSTEM]";
                else if (r && strcmp(r, "developer") == 0)
                    label = "[SYSTEM]";
                else if (r && strcmp(r, "assistant") == 0)
                    label = "[ASSISTANT]";
                else if (c && strstr(c, "<tool_response>"))
                    label = "[TOOL RESULT]";

                buffer_append(&combined, label, strlen(label));
                buffer_append(&combined, "\n", 1);
                buffer_append(&combined, c, strlen(c));
            }
        }

        /* Always send as "user" role to UvA */
        if (combined.size > 0 && final_content[0]) {
            buffer_append(&combined, "\n\n", 2);
            buffer_append(&combined, "[USER]\n", 7);
            buffer_append(&combined, final_content, strlen(final_content));
            last_msg = make_uva_message("user", combined.data,
                                        last_msg_id);
        } else if (combined.size > 0) {
            last_msg = make_uva_message("user", combined.data,
                                        last_msg_id);
        } else {
            last_msg = make_uva_message("user", final_content,
                                        last_msg_id);
        }
        buffer_free(&combined);
    }

    if (last_msg)
        json_object_object_add(uva, "message", last_msg);

    json_object_object_add(uva, "flags", make_flags(1));

    /* overrides */
    struct json_object *overrides = json_object_new_object();
    json_object_object_add(overrides, "model",
        json_object_new_string(uva_model));
    json_object_object_add(overrides, "personaId",
        json_object_new_string(""));

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
    add_request_time(uva);

    const char *str = json_object_to_json_string_ext(uva,
        JSON_C_TO_STRING_PLAIN);
    char *result = strdup(str);

    json_object_put(uva);
    json_object_put(req);
    return result;
}

/*
 * Build an incremental (continuation) UvA request.
 * Sends only the new message on an existing thread with isNewChat:false.
 */
char *translate_request_incremental(const char *content, const char *role,
                                     const char *thread_id,
                                     const char *model)
{
    if (!content || !role || !thread_id) return NULL;

    struct json_object *uva = json_object_new_object();

    json_object_object_add(uva, "id", json_object_new_string(thread_id));

    /* Message with nanoid ID */
    char msg_id[NANOID_MSG_LEN + 1];
    gen_nanoid(msg_id, NANOID_MSG_LEN);
    struct json_object *msg = make_uva_message(role, content, msg_id);
    json_object_object_add(uva, "message", msg);

    json_object_object_add(uva, "flags", make_flags(0));

    /* Overrides: include model only on first message, empty otherwise */
    struct json_object *overrides = json_object_new_object();
    if (model && model[0]) {
        const char *uva_model = models_to_uva(model);
        json_object_object_add(overrides, "model",
            json_object_new_string(uva_model));
        json_object_object_add(overrides, "personaId",
            json_object_new_string(""));
    }
    json_object_object_add(uva, "overrides", overrides);

    add_request_time(uva);

    const char *str = json_object_to_json_string_ext(uva,
        JSON_C_TO_STRING_PLAIN);
    char *result = strdup(str);

    json_object_put(uva);
    return result;
}

/* translate_response, translate_stream_chunk, translate_stream_done
 * are now in response_format.c */
