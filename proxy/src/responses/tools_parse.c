#include "responses.h"
#include "responses_internal.h"
#include <stdlib.h>
#include <string.h>

/* Parse <function_call>/<tool_call> XML blocks from model output.
 * Returns number of calls found. */
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

/* Find next <function_call or <tool_call opening tag. */
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

/* Strip function_call/tool_call XML blocks from text. Caller frees. */
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

/* Extract shell command from first markdown code block. Caller frees. */
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

/* Extract command from inline `backticks`. Prefers first with space. */
char *resp_extract_inline_cmd(const char *text)
{
    if (!text) return NULL;
    const char *best = NULL;
    size_t best_len = 0;

    for (const char *p = text; *p; ) {
        /* Skip triple-backtick code blocks */
        if (p[0] == '`' && p[1] == '`' && p[2] == '`') {
            const char *e = strstr(p + 3, "```");
            p = e ? e + 3 : p + strlen(p);
            continue;
        }
        if (*p != '`') { p++; continue; }
        const char *s = ++p;
        const char *e = strchr(s, '`');
        if (!e || e == s) continue;
        size_t len = (size_t)(e - s);
        p = e + 1;
        if (len < 2 || len >= RESP_MAX_ARGS) continue;
        /* Prefer first content with a space (command + args) */
        if (len >= 4 && memchr(s, ' ', len)) {
            char *cmd = malloc(len + 1);
            if (!cmd) return NULL;
            memcpy(cmd, s, len);
            cmd[len] = '\0';
            return cmd;
        }
        if (len > best_len) { best = s; best_len = len; }
    }
    if (!best) return NULL;
    char *cmd = malloc(best_len + 1);
    if (!cmd) return NULL;
    memcpy(cmd, best, best_len);
    cmd[best_len] = '\0';
    return cmd;
}

/* Build synthetic exec_command tool call from a command string. */
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
