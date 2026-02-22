#include "responses.h"
#include "responses_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>

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
 * Parse <tool_call> blocks from model output.
 *
 * Hermes format: the model outputs blocks like:
 *   <tool_call>
 *   {"name": "exec_command", "arguments": {"command": ["ls"]}}
 *   </tool_call>
 *
 * We parse the JSON inside each block, extracting "name" as a string
 * and "arguments" as a re-serialized JSON string.
 *
 * Returns number of calls found.
 */
int resp_parse_tool_calls(const char *text, resp_tool_call_t *out,
                          int max_calls)
{
    if (!text || !out || max_calls <= 0) return 0;
    char *norm = normalize_close_tags(text);
    if (!norm) return 0;
    int count = 0;
    const char *p = norm;

    while (count < max_calls) {
        const char *start = strstr(p, "<tool_call>");
        if (!start) break;

        const char *content_start = start + 11; /* strlen("<tool_call>") */
        const char *end = strstr(content_start, "</tool_call>");
        if (!end) break;

        /* Extract and trim the content between tags */
        size_t content_len = (size_t)(end - content_start);
        char *content = malloc(content_len + 1);
        if (!content) break;
        memcpy(content, content_start, content_len);
        content[content_len] = '\0';

        /* Trim whitespace */
        char *trimmed = content;
        while (*trimmed == ' ' || *trimmed == '\n' ||
               *trimmed == '\r' || *trimmed == '\t')
            trimmed++;
        size_t tlen = strlen(trimmed);
        while (tlen > 0) {
            char c = trimmed[tlen - 1];
            if (c == ' ' || c == '\n' || c == '\r' || c == '\t')
                tlen--;
            else
                break;
        }
        trimmed[tlen] = '\0';

        /* Parse as JSON */
        struct json_object *obj = json_tokener_parse(trimmed);
        if (!obj) {
            free(content);
            p = end + 12; /* strlen("</tool_call>") */
            continue;
        }

        /* Extract "name" */
        struct json_object *name_obj;
        if (!json_object_object_get_ex(obj, "name", &name_obj)) {
            json_object_put(obj);
            free(content);
            p = end + 12;
            continue;
        }
        const char *name = json_object_get_string(name_obj);
        if (!name) name = "unknown";
        snprintf(out[count].name, RESP_MAX_NAME, "%s", name);

        /* Extract "arguments" and re-serialize as string */
        struct json_object *args_obj;
        if (json_object_object_get_ex(obj, "arguments", &args_obj)) {
            const char *args_str = json_object_to_json_string_ext(
                args_obj, JSON_C_TO_STRING_PLAIN);
            snprintf(out[count].arguments, RESP_MAX_ARGS, "%s",
                     args_str ? args_str : "{}");
        } else {
            snprintf(out[count].arguments, RESP_MAX_ARGS, "{}");
        }

        /* Generate call_id */
        resp_gen_id(out[count].call_id, "call_", 24);

        json_object_put(obj);
        free(content);
        count++;
        p = end + 12;
    }

    free(norm);
    return count;
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
        const char *start = strstr(p, "<tool_call>");
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
        const char *end = strstr(start, "</tool_call>");
        if (end) {
            p = end + 12;
        } else {
            /* No closing tag -- copy remainder */
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
