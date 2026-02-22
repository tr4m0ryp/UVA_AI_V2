#include "responses.h"
#include "responses_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>

/*
 * Build a Hermes-style tool system prompt.
 *
 * Single prompt for ALL models -- no model-specific branching.
 * Tools are serialized as their original JSON inside <tools> tags.
 * The model outputs <tool_call> blocks with JSON payloads, or plain
 * text if no tool use is needed.
 *
 * Returns malloc'd string; caller frees.
 */
char *resp_build_tool_prompt(struct json_object *tools_json)
{
    if (!tools_json || !json_object_is_type(tools_json, json_type_array))
        return NULL;
    int count = (int)json_object_array_length(tools_json);
    if (count == 0)
        return NULL;

    /* Estimate size: header + tools + footer */
    size_t cap = 4096;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    size_t pos = 0;

    /* Header */
    static const char header[] =
        "You are a function-calling AI model. You are provided with "
        "function signatures within <tools></tools> XML tags. You may "
        "call one or more functions to assist with the user query. Do "
        "not make assumptions about what values to plug into functions."
        "\n\nHere are the available tools:\n<tools>\n";

    size_t hlen = sizeof(header) - 1;
    if (pos + hlen >= cap) {
        cap = pos + hlen + 4096;
        buf = realloc(buf, cap);
        if (!buf) return NULL;
    }
    memcpy(buf + pos, header, hlen);
    pos += hlen;

    /* Each tool as one JSON line */
    for (int i = 0; i < count; i++) {
        struct json_object *tool = json_object_array_get_idx(tools_json,
                                                              (size_t)i);
        /* Skip non-function tools */
        struct json_object *type_obj;
        if (json_object_object_get_ex(tool, "type", &type_obj)) {
            const char *tt = json_object_get_string(type_obj);
            if (tt && strcmp(tt, "function") != 0)
                continue;
        }

        /* Resolve nested vs flat format: if "function" key exists,
         * serialize that object; otherwise serialize the tool itself
         * (minus the "type" field). We build a clean object with
         * type, name, description, parameters. */
        struct json_object *func_obj;
        struct json_object *src = tool;
        if (json_object_object_get_ex(tool, "function", &func_obj))
            src = func_obj;

        /* Build a normalized tool object for output */
        struct json_object *out_tool = json_object_new_object();
        json_object_object_add(out_tool, "type",
            json_object_new_string("function"));

        struct json_object *fn_out = json_object_new_object();

        struct json_object *name_obj;
        if (json_object_object_get_ex(src, "name", &name_obj))
            json_object_object_add(fn_out, "name",
                json_object_get(name_obj));

        struct json_object *desc_obj;
        if (json_object_object_get_ex(src, "description", &desc_obj))
            json_object_object_add(fn_out, "description",
                json_object_get(desc_obj));

        struct json_object *params_obj;
        if (json_object_object_get_ex(src, "parameters", &params_obj))
            json_object_object_add(fn_out, "parameters",
                json_object_get(params_obj));

        json_object_object_add(out_tool, "function", fn_out);

        const char *line = json_object_to_json_string_ext(out_tool,
            JSON_C_TO_STRING_PLAIN);
        size_t llen = strlen(line);

        if (pos + llen + 2 >= cap) {
            cap = pos + llen + 4096;
            buf = realloc(buf, cap);
            if (!buf) { json_object_put(out_tool); return NULL; }
        }
        memcpy(buf + pos, line, llen);
        pos += llen;
        buf[pos++] = '\n';

        json_object_put(out_tool);
    }

    /* Footer */
    static const char footer[] =
        "</tools>\n\n"
        "For each function call, return a <tool_call> block with a JSON "
        "object containing \"name\" and \"arguments\":\n"
        "<tool_call>\n"
        "{\"name\": \"function_name\", \"arguments\": "
        "{\"param1\": \"value1\"}}\n"
        "</tool_call>\n\n"
        "You may output multiple <tool_call> blocks if needed.\n"
        "If the user's request does not require tool use, respond "
        "normally with plain text -- do not wrap text in tags.\n";

    size_t flen = sizeof(footer) - 1;
    if (pos + flen + 1 >= cap) {
        cap = pos + flen + 4096;
        buf = realloc(buf, cap);
        if (!buf) return NULL;
    }
    memcpy(buf + pos, footer, flen);
    pos += flen;
    buf[pos] = '\0';

    return buf;
}
