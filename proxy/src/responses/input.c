#include "responses/responses.h"
#include "responses/responses_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Extract text content from a content parts array.
 * Uses a thread-local buffer (each connection runs in its own thread).
 */
static const char *extract_text_from_parts(struct json_object *parts)
{
    static __thread char buf[8192];
    buf[0] = '\0';
    size_t pos = 0;

    if (!parts || !json_object_is_type(parts, json_type_array))
        return buf;

    int len = (int)json_object_array_length(parts);
    for (int i = 0; i < len; i++) {
        struct json_object *part = json_object_array_get_idx(parts,
                                                              (size_t)i);
        struct json_object *type_obj, *text_obj;
        if (!json_object_object_get_ex(part, "type", &type_obj))
            continue;
        const char *type = json_object_get_string(type_obj);
        if (!type)
            continue;
        if (strcmp(type, "text") != 0 &&
            strcmp(type, "input_text") != 0 &&
            strcmp(type, "output_text") != 0)
            continue;
        if (!json_object_object_get_ex(part, "text", &text_obj))
            continue;
        const char *text = json_object_get_string(text_obj);
        if (text) {
            size_t tlen = strlen(text);
            if (pos + tlen < sizeof(buf) - 1) {
                memcpy(buf + pos, text, tlen);
                pos += tlen;
            }
        }
    }

    buf[pos] = '\0';
    return buf;
}

/*
 * Normalize the Responses API "input" field to a standard messages array.
 *
 * Input can be:
 *   - A string: "Hello" -> [{"role":"user","content":"Hello"}]
 *   - An array of message items:
 *     [{"role":"user","content":"Hi"},
 *      {"type":"function_call_output","call_id":"...","output":"..."}]
 *
 * If instructions is non-empty, prepend a system message.
 *
 * Returns a new json_object array (caller must json_object_put).
 */
struct json_object *resp_input_to_messages(struct json_object *input,
                                           const char *instructions)
{
    struct json_object *messages = json_object_new_array();
    if (!messages) return NULL;

    /* Prepend system message from instructions */
    if (instructions && instructions[0]) {
        struct json_object *sys = json_object_new_object();
        json_object_object_add(sys, "role",
            json_object_new_string("system"));
        json_object_object_add(sys, "content",
            json_object_new_string(instructions));
        json_object_array_add(messages, sys);
    }

    if (!input) return messages;

    /* Case 1: input is a string */
    if (json_object_is_type(input, json_type_string)) {
        struct json_object *msg = json_object_new_object();
        json_object_object_add(msg, "role",
            json_object_new_string("user"));
        json_object_object_add(msg, "content",
            json_object_new_string(json_object_get_string(input)));
        json_object_array_add(messages, msg);
        return messages;
    }

    /* Case 2: input is an array of items */
    if (json_object_is_type(input, json_type_array)) {
        int len = (int)json_object_array_length(input);
        for (int i = 0; i < len; i++) {
            struct json_object *item = json_object_array_get_idx(input,
                                                                  (size_t)i);
            if (!item) continue;

            /* Check if it's a standard message with role */
            struct json_object *role_obj;
            if (json_object_object_get_ex(item, "role", &role_obj)) {
                /* If content is an array, extract text from parts */
                struct json_object *cnt;
                if (json_object_object_get_ex(item, "content", &cnt) &&
                    json_object_is_type(cnt, json_type_array)) {
                    const char *r = json_object_get_string(role_obj);
                    if (r && strcmp(r, "developer") == 0) r = "system";
                    const char *txt = extract_text_from_parts(cnt);
                    struct json_object *msg = json_object_new_object();
                    json_object_object_add(msg, "role",
                        json_object_new_string(r ? r : "user"));
                    json_object_object_add(msg, "content",
                        json_object_new_string(txt));
                    json_object_array_add(messages, msg);
                } else {
                    /* Remap developer -> system for pass-through items */
                    const char *r = json_object_get_string(role_obj);
                    if (r && strcmp(r, "developer") == 0) {
                        struct json_object *cnt_o;
                        const char *c = "";
                        if (json_object_object_get_ex(item, "content",
                                                       &cnt_o))
                            c = json_object_get_string(cnt_o);
                        struct json_object *msg = json_object_new_object();
                        json_object_object_add(msg, "role",
                            json_object_new_string("system"));
                        json_object_object_add(msg, "content",
                            json_object_new_string(c ? c : ""));
                        json_object_array_add(messages, msg);
                    } else {
                        json_object_array_add(messages,
                            json_object_get(item));
                    }
                }
                continue;
            }

            /* Check type field for special item types */
            struct json_object *type_obj;
            if (!json_object_object_get_ex(item, "type", &type_obj))
                continue;

            const char *type = json_object_get_string(type_obj);
            if (!type) continue;

            if (strcmp(type, "message") == 0) {
                struct json_object *msg_role, *msg_content;
                struct json_object *msg = json_object_new_object();
                const char *r = "user";
                const char *c = "";

                if (json_object_object_get_ex(item, "role", &msg_role))
                    r = json_object_get_string(msg_role);

                if (json_object_object_get_ex(item, "content",
                                               &msg_content)) {
                    if (json_object_is_type(msg_content, json_type_string)) {
                        c = json_object_get_string(msg_content);
                    } else if (json_object_is_type(msg_content,
                                                    json_type_array)) {
                        c = extract_text_from_parts(msg_content);
                    }
                }

                json_object_object_add(msg, "role",
                    json_object_new_string(r));
                json_object_object_add(msg, "content",
                    json_object_new_string(c));
                json_object_array_add(messages, msg);

            } else if (strcmp(type, "function_call") == 0) {
                /* Format as <tool_call> XML so the model sees the
                 * same delimiters it should output (in-context learning). */
                struct json_object *fn_obj, *args_obj;
                const char *fn = "unknown";
                const char *args = "{}";
                if (json_object_object_get_ex(item, "name", &fn_obj))
                    fn = json_object_get_string(fn_obj);
                if (json_object_object_get_ex(item, "arguments", &args_obj))
                    args = json_object_get_string(args_obj);

                char content[RESP_MAX_ARGS + 512];
                snprintf(content, sizeof(content),
                    "<tool_call>\n"
                    "{\"name\": \"%s\", \"arguments\": %s}\n"
                    "</tool_call>",
                    fn, args);

                struct json_object *msg = json_object_new_object();
                json_object_object_add(msg, "role",
                    json_object_new_string("assistant"));
                json_object_object_add(msg, "content",
                    json_object_new_string(content));
                json_object_array_add(messages, msg);

            } else if (strcmp(type, "function_call_output") == 0) {
                /* Kept for resp_fold_tool_results() to process */
                json_object_array_add(messages, json_object_get(item));
            }
        }
    }

    return messages;
}

/* Scan backward for a preceding assistant message with a <tool_call>
 * block containing the function name. Returns name or NULL. */
static const char *find_func_name_for_call(struct json_object *messages,
                                            int pos, const char *call_id)
{
    (void)call_id; /* call_id not in tool_call XML; match by proximity */

    for (int j = pos - 1; j >= 0 && j >= pos - 4; j--) {
        struct json_object *prev = json_object_array_get_idx(messages,
                                                              (size_t)j);
        if (!prev) continue;

        struct json_object *role_o, *content_o;
        if (!json_object_object_get_ex(prev, "role", &role_o))
            continue;
        const char *r = json_object_get_string(role_o);
        if (!r || strcmp(r, "assistant") != 0)
            continue;
        if (!json_object_object_get_ex(prev, "content", &content_o))
            continue;
        const char *c = json_object_get_string(content_o);
        if (!c) continue;

        /* Look for "name": "..." in the tool_call block */
        const char *tc = strstr(c, "<tool_call>");
        if (!tc) continue;

        const char *nq = strstr(tc, "\"name\"");
        if (!nq) continue;
        nq = strchr(nq + 6, '"');
        if (!nq) continue;
        nq++; /* skip opening quote */
        const char *ne = strchr(nq, '"');
        if (!ne) continue;

        static __thread char name_buf[RESP_MAX_NAME];
        size_t nlen = (size_t)(ne - nq);
        if (nlen >= RESP_MAX_NAME) nlen = RESP_MAX_NAME - 1;
        memcpy(name_buf, nq, nlen);
        name_buf[nlen] = '\0';
        return name_buf;
    }
    return NULL;
}

void resp_fold_tool_results(struct json_object *messages)
{
    if (!messages || !json_object_is_type(messages, json_type_array))
        return;

    int len = (int)json_object_array_length(messages);
    for (int i = 0; i < len; i++) {
        struct json_object *item = json_object_array_get_idx(messages,
                                                              (size_t)i);
        if (!item) continue;

        struct json_object *type_obj;
        if (!json_object_object_get_ex(item, "type", &type_obj))
            continue;

        const char *type = json_object_get_string(type_obj);
        if (!type || strcmp(type, "function_call_output") != 0)
            continue;

        struct json_object *call_id_obj, *output_obj;
        const char *call_id = "";
        const char *output = "";

        if (json_object_object_get_ex(item, "call_id", &call_id_obj))
            call_id = json_object_get_string(call_id_obj);
        if (json_object_object_get_ex(item, "output", &output_obj))
            output = json_object_get_string(output_obj);

        /* Format as <tool_response> XML for in-context learning.
         * Use json-c to properly escape the output string. */
        (void)find_func_name_for_call(messages, i, call_id);

        struct json_object *out_str = json_object_new_string(output);
        const char *escaped_out = json_object_to_json_string(out_str);

        char content[RESP_MAX_ARGS + 512];
        snprintf(content, sizeof(content),
                 "<tool_response>\n"
                 "{\"call_id\": \"%s\", \"output\": %s}\n"
                 "</tool_response>",
                 call_id, escaped_out);
        json_object_put(out_str);

        struct json_object *msg = json_object_new_object();
        json_object_object_add(msg, "role",
            json_object_new_string("user"));
        json_object_object_add(msg, "content",
            json_object_new_string(content));

        json_object_array_put_idx(messages, (size_t)i, msg);
    }
}
