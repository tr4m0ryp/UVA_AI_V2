/*
 * tools.c - Tool call translation: OpenAI tools[] -> XML system prompt,
 * tool_result messages -> plain user messages, SSE text extraction.
 */
#include "translator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>

/* -----------------------------------------------------------------------
 * tool_definitions_to_sysprompt
 * Build the XML instruction block from an OpenAI tools[] array.
 * ----------------------------------------------------------------------- */
char *tool_definitions_to_sysprompt(struct json_object *tools_array)
{
    if (!tools_array || !json_object_is_type(tools_array, json_type_array))
        return NULL;

    int n = (int)json_object_array_length(tools_array);
    if (n == 0) return NULL;

    /* Conservative upper bound: 256 bytes header + 512 bytes per tool */
    size_t cap = 512 + (size_t)n * 512;
    char *buf = malloc(cap);
    if (!buf) return NULL;

    int pos = 0;
    pos += snprintf(buf + pos, cap - (size_t)pos,
        "You have access to the following tools. To use a tool, output EXACTLY:\n\n"
        "<tool_call name=\"TOOL_NAME\">\n"
        "{\"arg1\": \"value1\"}\n"
        "</tool_call>\n\n"
        "Output only one tool_call at a time. Wait for the result before continuing.\n"
        "When you have a final answer and no more tools to call, respond normally with no tags.\n\n"
        "Available tools:\n");

    for (int i = 0; i < n; i++) {
        struct json_object *tool = json_object_array_get_idx(tools_array, (size_t)i);
        if (!tool) continue;

        /* OpenAI format: {"type":"function","function":{"name":"...","description":"...","parameters":{...}}} */
        struct json_object *fn_obj;
        if (!json_object_object_get_ex(tool, "function", &fn_obj)) {
            /* May be bare function spec */
            fn_obj = tool;
        }

        struct json_object *name_o, *desc_o, *params_o;
        const char *name = "";
        const char *desc = "";

        if (json_object_object_get_ex(fn_obj, "name", &name_o))
            name = json_object_get_string(name_o);
        if (json_object_object_get_ex(fn_obj, "description", &desc_o))
            desc = json_object_get_string(desc_o);

        /* Build compact ARGS representation from parameters schema */
        char args_buf[256] = "{}";
        if (json_object_object_get_ex(fn_obj, "parameters", &params_o)) {
            struct json_object *props;
            if (json_object_object_get_ex(params_o, "properties", &props)) {
                int apos = 0;
                apos += snprintf(args_buf + apos, sizeof(args_buf) - (size_t)apos, "{");
                int first = 1;
                json_object_object_foreach(props, pname, pval) {
                    struct json_object *type_o;
                    const char *type_str = "string";
                    if (json_object_object_get_ex(pval, "type", &type_o))
                        type_str = json_object_get_string(type_o);
                    if (!first) apos += snprintf(args_buf + apos,
                        sizeof(args_buf) - (size_t)apos, ", ");
                    apos += snprintf(args_buf + apos,
                        sizeof(args_buf) - (size_t)apos,
                        "\"%s\": \"%s\"", pname, type_str);
                    first = 0;
                }
                snprintf(args_buf + apos, sizeof(args_buf) - (size_t)apos, "}");
            }
        }

        pos += snprintf(buf + pos, cap - (size_t)pos,
            "NAME: %s\nDESCRIPTION: %s\nARGS: %s\n\n",
            name, desc, args_buf);

        if ((size_t)pos > cap - 64) break; /* safety: stop before overflow */
    }

    return buf;
}

/* -----------------------------------------------------------------------
 * inject_tool_sysprompt
 * Prepend tool instructions to the system message (or insert one).
 * Returns a new JSON object; caller owns it. Input req is NOT modified.
 * ----------------------------------------------------------------------- */
struct json_object *inject_tool_sysprompt(struct json_object *req,
                                           const char *tool_sysprompt)
{
    if (!req || !tool_sysprompt) return NULL;

    /* Deep-copy via serialise/parse */
    const char *ser = json_object_to_json_string_ext(req, JSON_C_TO_STRING_PLAIN);
    struct json_object *copy = json_tokener_parse(ser);
    if (!copy) return NULL;

    struct json_object *msgs;
    if (!json_object_object_get_ex(copy, "messages", &msgs)) {
        /* No messages array - create one with the system message */
        msgs = json_object_new_array();
        json_object_object_add(copy, "messages", msgs);
    }

    int len = (int)json_object_array_length(msgs);

    /* Check if first message is system */
    if (len > 0) {
        struct json_object *first = json_object_array_get_idx(msgs, 0);
        struct json_object *role_o;
        if (json_object_object_get_ex(first, "role", &role_o) &&
            strcmp(json_object_get_string(role_o), "system") == 0) {
            /* Prepend to existing system message content */
            struct json_object *content_o;
            const char *existing = "";
            if (json_object_object_get_ex(first, "content", &content_o))
                existing = json_object_get_string(content_o);

            size_t total = strlen(tool_sysprompt) + strlen(existing) + 4;
            char *combined = malloc(total);
            if (!combined) { json_object_put(copy); return NULL; }
            snprintf(combined, total, "%s\n\n%s", tool_sysprompt, existing);
            json_object_object_add(first, "content",
                json_object_new_string(combined));
            free(combined);
            return copy;
        }
    }

    /* Insert new system message at index 0 */
    struct json_object *sys_msg = json_object_new_object();
    json_object_object_add(sys_msg, "role", json_object_new_string("system"));
    json_object_object_add(sys_msg, "content",
        json_object_new_string(tool_sysprompt));

    /* Shift existing messages right by rebuilding the array */
    struct json_object *new_msgs = json_object_new_array();
    json_object_array_add(new_msgs, sys_msg);
    for (int i = 0; i < len; i++)
        json_object_array_add(new_msgs,
            json_object_get(json_object_array_get_idx(msgs, (size_t)i)));

    json_object_object_del(copy, "messages");
    json_object_object_add(copy, "messages", new_msgs);
    return copy;
}

/* -----------------------------------------------------------------------
 * fold_tool_results
 * Convert {"role":"tool","tool_call_id":"...","content":"..."} messages
 * to plain {"role":"user","content":"Tool result (id):\n..."} messages
 * so UvA can understand the conversation context.
 * Returns a new JSON array; caller owns it.
 * ----------------------------------------------------------------------- */
struct json_object *fold_tool_results(struct json_object *messages_array)
{
    if (!messages_array) return json_object_new_array();

    int n = (int)json_object_array_length(messages_array);
    struct json_object *out = json_object_new_array();

    for (int i = 0; i < n; i++) {
        struct json_object *msg = json_object_array_get_idx(messages_array, (size_t)i);
        struct json_object *role_o;
        const char *role = "";
        if (json_object_object_get_ex(msg, "role", &role_o))
            role = json_object_get_string(role_o);

        if (strcmp(role, "tool") == 0) {
            struct json_object *id_o, *content_o;
            const char *call_id = "unknown";
            const char *content = "";
            if (json_object_object_get_ex(msg, "tool_call_id", &id_o))
                call_id = json_object_get_string(id_o);
            if (json_object_object_get_ex(msg, "content", &content_o))
                content = json_object_get_string(content_o);

            size_t len = strlen(call_id) + strlen(content) + 32;
            char *text = malloc(len);
            if (!text) continue;
            snprintf(text, len, "Tool result (%s):\n%s", call_id, content);

            struct json_object *converted = json_object_new_object();
            json_object_object_add(converted, "role",
                json_object_new_string("user"));
            json_object_object_add(converted, "content",
                json_object_new_string(text));
            free(text);
            json_object_array_add(out, converted);
        } else {
            /* Pass through; grab a ref so array owns it */
            json_object_array_add(out, json_object_get(msg));
        }
    }

    return out;
}

/* -----------------------------------------------------------------------
 * extract_text_from_sse
 * Parse a buffered UvA SSE response and concatenate all text-delta
 * content into a single heap string. Caller frees.
 * ----------------------------------------------------------------------- */
char *extract_text_from_sse(const char *data, size_t len)
{
    /* Result buffer - start at 4 KB, grow as needed */
    size_t cap = 4096;
    size_t used = 0;
    char *result = malloc(cap);
    if (!result) return NULL;
    result[0] = '\0';

    const char *p = data;
    const char *end = data + len;

    while (p < end) {
        /* Find next "data: " line */
        const char *line_start = p;
        const char *nl = memchr(p, '\n', (size_t)(end - p));
        const char *line_end = nl ? nl : end;

        size_t line_len = (size_t)(line_end - line_start);
        if (line_len > 6 && memcmp(line_start, "data: ", 6) == 0) {
            const char *json_str = line_start + 6;
            size_t json_len = line_len - 6;

            /* Skip [DONE] */
            if (json_len >= 6 && memcmp(json_str, "[DONE]", 6) == 0) {
                p = line_end + 1;
                continue;
            }

            /* Null-terminate for json-c */
            char *tmp = malloc(json_len + 1);
            if (!tmp) { p = line_end + 1; continue; }
            memcpy(tmp, json_str, json_len);
            tmp[json_len] = '\0';

            struct json_object *chunk = json_tokener_parse(tmp);
            free(tmp);

            if (chunk) {
                /* Vercel AI SDK format: {"type":"text-delta","delta":"..."} */
                struct json_object *type_o, *delta_o;
                if (json_object_object_get_ex(chunk, "type", &type_o) &&
                    strcmp(json_object_get_string(type_o), "text-delta") == 0 &&
                    json_object_object_get_ex(chunk, "delta", &delta_o)) {
                    const char *delta = json_object_get_string(delta_o);
                    size_t dlen = strlen(delta);
                    if (used + dlen + 1 > cap) {
                        cap = (used + dlen + 1) * 2;
                        char *nr = realloc(result, cap);
                        if (!nr) { json_object_put(chunk); break; }
                        result = nr;
                    }
                    memcpy(result + used, delta, dlen);
                    used += dlen;
                    result[used] = '\0';
                }
                json_object_put(chunk);
            }
        }

        p = line_end + 1;
    }

    return result;
}
