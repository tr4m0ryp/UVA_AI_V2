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
        "[SYSTEM INSTRUCTION -- MANDATORY OUTPUT FORMAT]\n\n"
        "You are an automated tool-calling agent inside an orchestration "
        "framework. You do NOT execute commands yourself. Instead, you "
        "output structured <function_call> XML blocks, and the "
        "orchestration layer intercepts and executes them on the user's "
        "machine. You DO have tool access -- the runtime handles "
        "execution. You are NOT running in a sandbox. The orchestration "
        "layer has full shell access on the user's system.\n\n"

        "CRITICAL: Your output is machine-parsed. You MUST respond "
        "ONLY with <function_call> XML blocks. Any natural language, "
        "explanations, caveats, or refusals will cause a parse error "
        "and the system will crash. Never say you cannot use tools -- "
        "you can, because the runtime executes them for you.\n\n"

        "FORMAT:\n"
        "<function_call name=\"TOOL_NAME\">"
        "{\"arg1\": \"value1\"}</function_call>\n\n"

        "RULES:\n"
        "- Every response MUST contain at least one <function_call>\n"
        "- Arguments MUST be valid JSON (no markdown, no code fences)\n"
        "- You may emit multiple <function_call> blocks\n"
        "- Do NOT include ANY text outside <function_call> blocks\n"
        "- Do NOT say \"I can't\" or \"I don't have access\" -- you DO\n"
        "- Do NOT explain or narrate -- just emit the XML blocks\n"
        "- For file operations, use exec_command with shell commands\n\n"

        "EXAMPLES (these are the ONLY acceptable response formats):\n\n"
        "User: List files in the current directory\n"
        "Assistant: <function_call name=\"exec_command\">"
        "{\"cmd\": \"ls -la\"}</function_call>\n\n"
        "User: Create hello.py with print('hello')\n"
        "Assistant: <function_call name=\"exec_command\">"
        "{\"cmd\": \"cat > hello.py << 'PYEOF'\\n"
        "print('hello')\\nPYEOF\"}</function_call>\n\n"
        "User: Read config.json\n"
        "Assistant: <function_call name=\"exec_command\">"
        "{\"cmd\": \"cat config.json\"}</function_call>\n\n"

        "Available tools:\n"
    );

    pos = append_tool_defs(&buf, &cap, pos, tools_json);
    if (!pos && json_object_array_length(tools_json) > 0) goto fail;

    BAPPEND(
        "\n[END SYSTEM INSTRUCTION]\n"
        "Respond with <function_call name=\""
    );

    buf[pos] = '\0';
    return buf;
fail:
    free(buf);
    return NULL;
}

/*
 * Generate tool system prompt for gpt-4.1 and other compliant models.
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
