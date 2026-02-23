#include "responses/responses.h"
#include "responses/responses_internal.h"
#include <stdlib.h>
#include <string.h>

/* Normalize <\/ -> </ (some models emit escaped closing tags). */
static char *normalize_close_tags(const char *text)
{
    size_t len = strlen(text);
    char *out = malloc(len + 1);
    if (!out) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (text[i] == '<' && i + 2 < len &&
            text[i+1] == '\\' && text[i+2] == '/')
            { out[j++] = '<'; i++; }
        else out[j++] = text[i];
    }
    out[j] = '\0';
    return out;
}

/*
 * Strip <tool_call>...</tool_call> blocks from text.
 * Returns the remaining text (malloc'd). Caller frees.
 */
char *resp_strip_tool_calls(const char *text)
{
    if (!text) return NULL;
    char *norm = normalize_close_tags(text);
    if (!norm) return NULL;

    size_t len = strlen(norm);
    char *out = malloc(len + 1);
    if (!out) { free(norm); return NULL; }
    size_t opos = 0;
    const char *p = norm;

    while (*p) {
        /* Find the earliest <tool_call> or <function_call> */
        const char *s_tc = strstr(p, "<tool_call>");
        const char *s_fc = strstr(p, "<function_call>");
        const char *start = NULL;
        const char *close_tag = NULL;

        if (s_tc && (!s_fc || s_tc <= s_fc)) {
            start = s_tc;
            close_tag = "</tool_call>";
        } else if (s_fc) {
            start = s_fc;
            close_tag = "</function_call>";
        }
        if (!start) {
            size_t remain = strlen(p);
            memcpy(out + opos, p, remain);
            opos += remain;
            break;
        }

        size_t before = (size_t)(start - p);
        if (before > 0) {
            memcpy(out + opos, p, before);
            opos += before;
        }

        const char *end = strstr(start, close_tag);
        if (end) {
            p = end + strlen(close_tag);
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
                         out[opos - 1] == '\r' || out[opos - 1] == '\t'))
        out[--opos] = '\0';

    free(norm);
    return out;
}
