#include "api/responses/responses.h"
#include "api/responses/responses_internal.h"
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

    size_t cap = 8192;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    size_t pos = 0;

    /* Header: agent identity framing */
    static const char header[] =
        "You are an autonomous coding agent. You operate in a loop: "
        "receive a task or tool result, decide the next action, call "
        "the appropriate tool, and continue until the task is fully "
        "complete.\n"
        "You MUST use tools to interact with the environment. You "
        "cannot run commands, read files, or modify anything without "
        "calling a tool. Do not describe what you would do -- do it.\n"
        "Plan before each tool call. After receiving a tool result, "
        "evaluate whether the task is done. If not, call the next "
        "tool immediately.\n\n"
        "Here are the available tools:\n<tools>\n";

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

    /* Footer: format instruction + few-shot examples */
    static const char footer[] =
        "</tools>\n\n"
        "To call a tool, respond with a <tool_call> block containing "
        "JSON with \"name\" and \"arguments\" keys:\n\n"
        "<tool_call>\n"
        "{\"name\": \"TOOL_NAME\", \"arguments\": {ARGS}}\n"
        "</tool_call>\n\n"
        "RULES:\n"
        "- Respond ONLY with <tool_call> blocks when action is needed\n"
        "- No text before or around <tool_call> blocks\n"
        "- Only use plain text after ALL actions are complete\n\n"
        "EXAMPLES:\n\n"
        "User: List the files in the current directory\n"
        "Response:\n"
        "<tool_call>\n"
        "{\"name\": \"exec_command\", \"arguments\": "
        "{\"command\": [\"bash\", \"-lc\", \"ls -la\"]}}\n"
        "</tool_call>\n\n"
        "User: Read the contents of main.py\n"
        "Response:\n"
        "<tool_call>\n"
        "{\"name\": \"exec_command\", \"arguments\": "
        "{\"command\": [\"bash\", \"-lc\", \"cat main.py\"]}}\n"
        "</tool_call>\n\n"
        "User: Create hello.py with a hello world program\n"
        "Response:\n"
        "<tool_call>\n"
        "{\"name\": \"exec_command\", \"arguments\": "
        "{\"command\": [\"bash\", \"-lc\", "
        "\"printf 'print(\\\"Hello, world!\\\")\\n' > hello.py\"]}}\n"
        "</tool_call>\n";

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

/*
 * Build a compact tool reminder for retry prompts.
 * Lists tool names only (no full schemas) with format reminder.
 * Returns malloc'd string; caller frees.
 */
char *resp_build_tool_reminder(struct json_object *tools_json)
{
    if (!tools_json || !json_object_is_type(tools_json, json_type_array))
        return NULL;
    int count = (int)json_object_array_length(tools_json);
    if (count == 0)
        return NULL;

    size_t cap = 2048;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    size_t pos = 0;

    static const char prefix[] =
        "Available tools: ";
    memcpy(buf, prefix, sizeof(prefix) - 1);
    pos = sizeof(prefix) - 1;

    for (int i = 0; i < count; i++) {
        struct json_object *tool = json_object_array_get_idx(
            tools_json, (size_t)i);
        struct json_object *src = tool;
        struct json_object *func_obj;
        if (json_object_object_get_ex(tool, "function", &func_obj))
            src = func_obj;
        struct json_object *name_obj;
        if (!json_object_object_get_ex(src, "name", &name_obj))
            continue;
        const char *name = json_object_get_string(name_obj);
        if (!name) continue;
        size_t nlen = strlen(name);
        if (pos + nlen + 4 >= cap) {
            cap = pos + nlen + 1024;
            buf = realloc(buf, cap);
            if (!buf) return NULL;
        }
        if (i > 0) { buf[pos++] = ','; buf[pos++] = ' '; }
        memcpy(buf + pos, name, nlen);
        pos += nlen;
    }

    static const char suffix[] =
        "\nUse <tool_call>{\"name\":\"...\",\"arguments\":{...}}"
        "</tool_call> format.";
    size_t slen = sizeof(suffix) - 1;
    if (pos + slen + 1 >= cap) {
        cap = pos + slen + 64;
        buf = realloc(buf, cap);
        if (!buf) return NULL;
    }
    memcpy(buf + pos, suffix, slen);
    pos += slen;
    buf[pos] = '\0';
    return buf;
}
