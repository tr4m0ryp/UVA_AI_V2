#include "responses.h"
#include "responses_internal.h"
#include <stdlib.h>
#include <string.h>

/*
 * Parse <function_call name="...">...</function_call> blocks from model
 * output. Also accepts <tool_call> for backward compatibility.
 * Uses simple pointer-based string search (no regex).
 * Generates a call_id for each match.
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
 * Find the next opening tag (either <function_call or <tool_call).
 * Sets *close_str and *close_len for the corresponding closing tag.
 * Returns pointer to the opening tag, or NULL if not found.
 */
static const char *find_next_open_tag(const char *p,
                                       const char **close_str,
                                       size_t *close_len)
{
    const char *fc = strstr(p, "<function_call");
    const char *tc = strstr(p, "<tool_call");
    const char *start = NULL;

    if (fc && tc)
        start = (fc < tc) ? fc : tc;
    else if (fc)
        start = fc;
    else if (tc)
        start = tc;
    else
        return NULL;

    if (start == fc) {
        *close_str = "</function_call>";
        *close_len = 16;
    } else {
        *close_str = "</tool_call>";
        *close_len = 12;
    }

    return start;
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
        const char *close_str = NULL;
        size_t close_len = 0;
        const char *start = find_next_open_tag(p, &close_str, &close_len);

        if (!start) {
            size_t remain = strlen(p);
            memcpy(out + opos, p, remain);
            opos += remain;
            break;
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

/*
 * Extract a shell command from the first markdown code block in text.
 * Looks for ```bash, ```sh, ```cmd, ```powershell, or bare ```.
 * Returns a newly allocated string with the command, or NULL.
 */
char *resp_extract_code_block_cmd(const char *text)
{
    if (!text) return NULL;

    const char *p = text;
    while ((p = strstr(p, "```")) != NULL) {
        p += 3;
        /* Skip language identifier (bash, sh, cmd, etc.) */
        while (*p && *p != '\n' && *p != '`') p++;
        if (*p == '\n') p++;
        else if (*p == '`') continue;

        /* Find closing ``` */
        const char *end = strstr(p, "```");
        if (!end || end == p) continue;

        /* Extract content, trim whitespace */
        size_t len = (size_t)(end - p);
        while (len > 0 && (p[len - 1] == '\n' || p[len - 1] == '\r' ||
               p[len - 1] == ' ')) len--;
        while (len > 0 && (*p == '\n' || *p == '\r' || *p == ' ')) {
            p++; len--;
        }
        if (len == 0 || len >= RESP_MAX_ARGS) continue;

        char *cmd = malloc(len + 1);
        if (!cmd) return NULL;
        memcpy(cmd, p, len);
        cmd[len] = '\0';
        return cmd;
    }
    return NULL;
}

/*
 * Build a synthetic exec_command tool call from a command string.
 * Used as fallback when the model refuses XML but includes commands
 * in markdown code blocks. Properly escapes JSON.
 */
void resp_synthesize_tool_call(resp_result_t *result, const char *cmd)
{
    result->tool_call_count = 1;
    snprintf(result->tool_calls[0].name, RESP_MAX_NAME, "exec_command");

    /* Build JSON arguments, escaping the command string */
    char *args = result->tool_calls[0].arguments;
    size_t apos = 0;
    apos += (size_t)snprintf(args + apos, RESP_MAX_ARGS - apos,
                              "{\"cmd\": \"");
    for (const char *c = cmd; *c && apos < RESP_MAX_ARGS - 4; c++) {
        if (*c == '"' || *c == '\\') args[apos++] = '\\';
        if (*c == '\n') { args[apos++] = '\\'; args[apos++] = 'n'; }
        else if (*c == '\t') { args[apos++] = '\\'; args[apos++] = 't'; }
        else args[apos++] = *c;
    }
    apos += (size_t)snprintf(args + apos, RESP_MAX_ARGS - apos, "\"}");
    args[apos] = '\0';

    resp_gen_id(result->tool_calls[0].call_id, "call_", 24);

    free(result->full_text);
    result->full_text = strdup("");
}
