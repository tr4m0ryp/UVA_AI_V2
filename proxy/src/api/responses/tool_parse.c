#include "api/responses/responses.h"
#include "api/responses/responses_internal.h"
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
        /* Accept both <tool_call> and <function_call> tags */
        const char *start_tc = strstr(p, "<tool_call>");
        const char *start_fc = strstr(p, "<function_call>");
        const char *start = NULL;
        const char *close_tag = NULL;
        int open_len = 0;

        if (start_tc && (!start_fc || start_tc <= start_fc)) {
            start = start_tc;
            open_len = 11;  /* strlen("<tool_call>") */
            close_tag = "</tool_call>";
        } else if (start_fc) {
            start = start_fc;
            open_len = 15;  /* strlen("<function_call>") */
            close_tag = "</function_call>";
        }
        if (!start) break;

        const char *content_start = start + open_len;
        const char *end = strstr(content_start, close_tag);
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

        /* Parse as JSON, with repair fallback */
        struct json_object *obj = json_tokener_parse(trimmed);
        if (!obj) {
            char *repaired = resp_repair_json(trimmed, tlen);
            if (repaired) {
                obj = json_tokener_parse(repaired);
                free(repaired);
            }
        }
        if (!obj) {
            free(content);
            p = end + strlen(close_tag);
            continue;
        }

        /* Extract "name" */
        struct json_object *name_obj;
        if (!json_object_object_get_ex(obj, "name", &name_obj)) {
            json_object_put(obj);
            free(content);
            p = end + strlen(close_tag);
            continue;
        }
        const char *name = json_object_get_string(name_obj);
        if (!name) name = "unknown";
        snprintf(out[count].name, RESP_MAX_NAME, "%s", name);

        /* Extract "arguments" and re-serialize as string */
        struct json_object *args_obj;
        if (json_object_object_get_ex(obj, "arguments", &args_obj)) {
            /* Handle double-serialized arguments (string not object) */
            if (json_object_is_type(args_obj, json_type_string)) {
                const char *astr = json_object_get_string(args_obj);
                struct json_object *re = json_tokener_parse(astr);
                if (re) {
                    const char *rs = json_object_to_json_string_ext(
                        re, JSON_C_TO_STRING_PLAIN);
                    snprintf(out[count].arguments, RESP_MAX_ARGS,
                             "%s", rs ? rs : "{}");
                    json_object_put(re);
                } else {
                    snprintf(out[count].arguments, RESP_MAX_ARGS,
                             "%s", astr);
                }
            } else {
                const char *args_str = json_object_to_json_string_ext(
                    args_obj, JSON_C_TO_STRING_PLAIN);
                snprintf(out[count].arguments, RESP_MAX_ARGS, "%s",
                         args_str ? args_str : "{}");
            }
        } else {
            snprintf(out[count].arguments, RESP_MAX_ARGS, "{}");
        }

        /* Generate call_id */
        resp_gen_id(out[count].call_id, "call_", 24);

        json_object_put(obj);
        free(content);
        count++;
        p = end + strlen(close_tag);
    }

    free(norm);
    return count;
}
