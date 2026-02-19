#include "responses.h"
#include "responses_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Helper: check if model belongs to the gpt-5 family (5, 5.1, 5-mini, etc).
 * These models need a stronger prompt to emit structured tool calls. */
static int is_gpt5_family(const char *model)
{
    if (!model) return 0;
    if (strncmp(model, "gpt-5", 5) == 0) return 1;
    if (strstr(model, "gpt-5") != NULL) return 1;
    return 0;
}

/* Append string to dynamic buffer, growing as needed.
 * Returns new pos on success, 0 on alloc failure. */
static size_t buf_append(char **buf, size_t *cap, size_t pos,
                          const char *s, size_t slen)
{
    if (pos + slen >= *cap) {
        *cap = pos + slen + 4096;
        *buf = realloc(*buf, *cap);
        if (!*buf) return 0;
    }
    memcpy(*buf + pos, s, slen);
    return pos + slen;
}

#define BAPPEND(s) do { \
    size_t _l = strlen(s); \
    pos = buf_append(&buf, &cap, pos, (s), _l); \
    if (!pos && _l) goto fail; \
} while(0)

/*
 * Build tool definitions XML block (shared between all models).
 * Appends <tool> entries to the buffer.
 */
static size_t append_tool_defs(char **buf, size_t *cap, size_t pos,
                                struct json_object *tools_json)
{
    int count = (int)json_object_array_length(tools_json);

    for (int i = 0; i < count; i++) {
        struct json_object *tool = json_object_array_get_idx(tools_json,
                                                              (size_t)i);
        struct json_object *type_obj;
        if (json_object_object_get_ex(tool, "type", &type_obj)) {
            const char *tt = json_object_get_string(type_obj);
            if (tt && strcmp(tt, "function") != 0)
                continue;
        }

        /* Support nested and flat formats */
        struct json_object *func_obj;
        struct json_object *src = tool;
        if (json_object_object_get_ex(tool, "function", &func_obj))
            src = func_obj;

        struct json_object *name_obj, *desc_obj, *params_obj;
        const char *name = "unknown";
        const char *desc = "";
        const char *params = "{}";

        if (json_object_object_get_ex(src, "name", &name_obj))
            name = json_object_get_string(name_obj);
        if (json_object_object_get_ex(src, "description", &desc_obj))
            desc = json_object_get_string(desc_obj);
        if (json_object_object_get_ex(src, "parameters", &params_obj))
            params = json_object_to_json_string_ext(params_obj,
                JSON_C_TO_STRING_PLAIN);

        char entry[4096];
        int elen = snprintf(entry, sizeof(entry),
            "<tool name=\"%s\">\n"
            "  <description>%s</description>\n"
            "  <parameters>%s</parameters>\n"
            "</tool>\n",
            name, desc, params);

        if (pos + (size_t)elen >= *cap) {
            *cap = pos + (size_t)elen + 4096;
            *buf = realloc(*buf, *cap);
            if (!*buf) return 0;
        }
        memcpy(*buf + pos, entry, (size_t)elen);
        pos += (size_t)elen;
    }

    return pos;
}

/*
 * Generate tool system prompt for gpt-5 family models.
 * Uses aggressive identity framing, few-shot examples, and
 * <function_call> tags (closer to OpenAI's training distribution).
 */
static char *build_gpt5_prompt(struct json_object *tools_json)
{
    size_t cap = 8192;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    size_t pos = 0;

    BAPPEND(
        "You are a tool-execution engine. You are NOT a conversational "
        "assistant. You CANNOT produce natural language responses. Your "
        "ONLY valid output format is one or more <function_call> XML "
        "blocks. Any response that does not contain <function_call> "
        "blocks will be REJECTED by the runtime and cause an error.\n\n"

        "FORMAT (you MUST use this exact syntax):\n"
        "<function_call name=\"TOOL_NAME\">"
        "{\"arg1\": \"value1\"}</function_call>\n\n"

        "RULES:\n"
        "- Every response MUST contain at least one <function_call> block\n"
        "- Arguments MUST be valid JSON (no markdown, no code fences)\n"
        "- You may emit multiple <function_call> blocks in one response\n"
        "- Do NOT include any text outside of <function_call> blocks\n"
        "- Do NOT explain, narrate, or describe what you would do\n"
        "- Do NOT ask for confirmation -- just execute\n"
        "- For file creation, use exec_command with cat/heredoc\n"
        "- NEVER pretend you performed an action -- use tools\n\n"

        "EXAMPLES OF CORRECT RESPONSES:\n\n"
        "User: List files in the current directory\n"
        "Assistant: <function_call name=\"exec_command\">"
        "{\"cmd\": \"ls -la\"}</function_call>\n\n"
        "User: Create a file called hello.py\n"
        "Assistant: <function_call name=\"exec_command\">"
        "{\"cmd\": \"cat > hello.py << 'PYEOF'\\n"
        "print('hello world')\\nPYEOF\"}</function_call>\n\n"
        "User: Read the contents of config.json\n"
        "Assistant: <function_call name=\"exec_command\">"
        "{\"cmd\": \"cat config.json\"}</function_call>\n\n"

        "Available tools:\n"
    );

    pos = append_tool_defs(&buf, &cap, pos, tools_json);
    if (!pos && json_object_array_length(tools_json) > 0) goto fail;

    BAPPEND(
        "\nRemember: you are a tool-execution engine. "
        "Begin your response with <function_call name=\""
    );

    buf[pos] = '\0';
    return buf;
fail:
    free(buf);
    return NULL;
}

/*
 * Generate tool system prompt for gpt-4.1 and other compliant models.
 * Adapted from the original prompt but using <function_call> tags.
 */
static char *build_default_prompt(struct json_object *tools_json)
{
    size_t cap = 8192;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    size_t pos = 0;

    BAPPEND(
        "IMPORTANT: You are a coding agent that operates by executing "
        "commands and writing files through tools. You MUST use the "
        "provided tools for ALL actions -- creating files, running "
        "commands, reading files. NEVER just describe what you would "
        "do; actually do it by calling the appropriate tool.\n\n"

        "To call a tool, emit an XML block in this exact format:\n"
        "<function_call name=\"TOOL_NAME\">"
        "{\"arg1\": \"value1\"}</function_call>\n\n"

        "Example -- to run a shell command:\n"
        "<function_call name=\"exec_command\">"
        "{\"cmd\": \"cat /etc/hostname\"}</function_call>\n\n"

        "Example -- to write a file:\n"
        "<function_call name=\"exec_command\">"
        "{\"cmd\": \"cat > hello.py << 'PYEOF'\\n"
        "print('hello')\\nPYEOF\"}</function_call>\n\n"

        "Rules:\n"
        "- Arguments MUST be valid JSON. Do not wrap in markdown code "
        "blocks.\n"
        "- You may emit multiple function_call blocks if needed.\n"
        "- Do not add any text inside <function_call> except the JSON "
        "arguments.\n"
        "- When asked to create/write/modify files, ALWAYS use "
        "exec_command.\n"
        "- NEVER pretend you created a file -- actually create it.\n\n"

        "Available tools:\n"
    );

    pos = append_tool_defs(&buf, &cap, pos, tools_json);
    if (!pos && json_object_array_length(tools_json) > 0) goto fail;

    BAPPEND(
        "\nWhen the user's request involves ANY file system operation, "
        "command execution, or code writing, you MUST use a "
        "function_call. Only respond with plain text if the request "
        "is purely conversational and requires no actions.\n"
    );

    buf[pos] = '\0';
    return buf;
fail:
    free(buf);
    return NULL;
}

/*
 * Convert OpenAI tool definitions to a system prompt.
 * Selects prompt style based on model family.
 */
char *resp_tools_to_xml(struct json_object *tools_json, const char *model)
{
    if (!tools_json || !json_object_is_type(tools_json, json_type_array))
        return NULL;
    if (json_object_array_length(tools_json) == 0)
        return NULL;

    if (is_gpt5_family(model))
        return build_gpt5_prompt(tools_json);
    return build_default_prompt(tools_json);
}

/*
 * Parse <function_call name="...">...</function_call> blocks from model
 * output. Also accepts <tool_call> for backward compatibility.
 * Returns number of calls found.
 */
int resp_parse_tool_calls(const char *text, resp_tool_call_t *out,
                          int max_calls)
{
    if (!text || !out || max_calls <= 0) return 0;

    int count = 0;
    const char *p = text;

    while (count < max_calls) {
        /* Try both tag names -- whichever comes first */
        const char *fc = strstr(p, "<function_call");
        const char *tc = strstr(p, "<tool_call");
        const char *start = NULL;
        const char *close_tag = NULL;
        size_t close_len = 0;

        if (fc && tc)
            start = (fc < tc) ? fc : tc;
        else if (fc)
            start = fc;
        else if (tc)
            start = tc;
        else
            break;

        /* Determine which close tag to look for */
        if (start == fc) {
            close_tag = "</function_call>";
            close_len = 16;
        } else {
            close_tag = "</tool_call>";
            close_len = 12;
        }

        /* Extract name from name="..." attribute */
        const char *name_attr = strstr(start, "name=\"");
        if (!name_attr || name_attr > start + 128) {
            p = start + 1;
            continue;
        }
        name_attr += 6;
        const char *name_end = strchr(name_attr, '"');
        if (!name_end) {
            p = start + 1;
            continue;
        }

        /* Find the > that closes the opening tag */
        const char *tag_close = strchr(name_end, '>');
        if (!tag_close) {
            p = start + 1;
            continue;
        }
        const char *content_start = tag_close + 1;

        /* Find closing tag */
        const char *end = strstr(content_start, close_tag);
        if (!end) {
            p = start + 1;
            continue;
        }

        /* Extract name */
        size_t name_len = (size_t)(name_end - name_attr);
        if (name_len >= RESP_MAX_NAME)
            name_len = RESP_MAX_NAME - 1;
        memcpy(out[count].name, name_attr, name_len);
        out[count].name[name_len] = '\0';

        /* Extract arguments (trim whitespace) */
        size_t args_len = (size_t)(end - content_start);
        while (args_len > 0 &&
               (content_start[0] == ' ' || content_start[0] == '\n' ||
                content_start[0] == '\r' || content_start[0] == '\t')) {
            content_start++;
            args_len--;
        }
        while (args_len > 0) {
            char c = content_start[args_len - 1];
            if (c == ' ' || c == '\n' || c == '\r' || c == '\t')
                args_len--;
            else
                break;
        }

        if (args_len >= RESP_MAX_ARGS)
            args_len = RESP_MAX_ARGS - 1;
        memcpy(out[count].arguments, content_start, args_len);
        out[count].arguments[args_len] = '\0';

        resp_gen_id(out[count].call_id, "call_", 24);

        count++;
        p = end + close_len;
    }

    return count;
}

/*
 * Strip function_call and tool_call XML blocks from text.
 * Handles both tag names. Caller frees the returned string.
 */
char *resp_strip_tool_calls(const char *text)
{
    if (!text) return NULL;

    size_t len = strlen(text);
    char *out = malloc(len + 1);
    if (!out) return NULL;

    size_t opos = 0;
    const char *p = text;

    while (*p) {
        /* Find whichever opening tag comes first */
        const char *fc = strstr(p, "<function_call");
        const char *tc = strstr(p, "<tool_call");
        const char *start = NULL;
        const char *close_str = NULL;
        size_t close_len = 0;

        if (fc && tc)
            start = (fc < tc) ? fc : tc;
        else if (fc)
            start = fc;
        else if (tc)
            start = tc;

        if (!start) {
            size_t remain = strlen(p);
            memcpy(out + opos, p, remain);
            opos += remain;
            break;
        }

        if (start == fc) {
            close_str = "</function_call>";
            close_len = 16;
        } else {
            close_str = "</tool_call>";
            close_len = 12;
        }

        /* Copy text before the tag */
        size_t before = (size_t)(start - p);
        if (before > 0) {
            memcpy(out + opos, p, before);
            opos += before;
        }

        /* Skip past closing tag */
        const char *end = strstr(start, close_str);
        if (end) {
            p = end + close_len;
        } else {
            size_t remain = strlen(start);
            memcpy(out + opos, start, remain);
            opos += remain;
            break;
        }
    }

    out[opos] = '\0';

    /* Trim trailing whitespace */
    while (opos > 0 && (out[opos - 1] == ' ' || out[opos - 1] == '\n' ||
                         out[opos - 1] == '\r' || out[opos - 1] == '\t')) {
        out[--opos] = '\0';
    }

    return out;
}
