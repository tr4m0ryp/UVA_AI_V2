#include "responses/responses.h"
#include "responses/responses_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>

/*
 * Repair common JSON issues in model output.
 * - Strip wrapping markdown code fences (```json ... ```)
 * - Remove trailing commas before } or ]
 * Returns malloc'd cleaned string; caller frees.
 */
char *resp_repair_json(const char *raw, size_t len)
{
    const char *start = raw;
    const char *end = raw + len;

    /* Strip leading whitespace */
    while (start < end && (*start == ' ' || *start == '\n' ||
           *start == '\r' || *start == '\t'))
        start++;

    /* Strip ```json or ``` prefix */
    if (end - start >= 3 && strncmp(start, "```", 3) == 0) {
        start += 3;
        while (start < end && *start != '\n' && *start != '{')
            start++;
        if (start < end && *start == '\n') start++;
    }

    /* Strip trailing ``` */
    const char *te = end;
    while (te > start && (te[-1] == ' ' || te[-1] == '\n' ||
           te[-1] == '\r' || te[-1] == '\t'))
        te--;
    if (te - start >= 3 && strncmp(te - 3, "```", 3) == 0)
        te -= 3;
    while (te > start && (te[-1] == ' ' || te[-1] == '\n' ||
           te[-1] == '\r' || te[-1] == '\t'))
        te--;

    size_t slen = (size_t)(te - start);
    char *out = malloc(slen + 1);
    if (!out) return NULL;

    /* Copy while removing trailing commas before } or ] */
    size_t j = 0;
    int in_string = 0;
    char prev_non_ws = 0;
    for (size_t i = 0; i < slen; i++) {
        char c = start[i];
        if (in_string) {
            out[j++] = c;
            if (c == '"' && (i == 0 || start[i-1] != '\\'))
                in_string = 0;
            continue;
        }
        if (c == '"') in_string = 1;
        if ((c == '}' || c == ']') && prev_non_ws == ',') {
            size_t k = j;
            while (k > 0 && (out[k-1] == ' ' || out[k-1] == '\n' ||
                   out[k-1] == '\r' || out[k-1] == '\t'))
                k--;
            if (k > 0 && out[k-1] == ',') j = k - 1;
        }
        out[j++] = c;
        if (c != ' ' && c != '\n' && c != '\r' && c != '\t')
            prev_non_ws = c;
    }
    out[j] = '\0';
    return out;
}

/*
 * Extract tool calls from loose JSON (no XML tags).
 * Scans for {"name":"...","arguments":...} patterns in the text.
 * Returns number of calls found (up to max_calls).
 */
int resp_extract_loose_json(const char *text, resp_tool_call_t *out,
                             int max_calls)
{
    if (!text || !out || max_calls <= 0) return 0;
    int count = 0;
    const char *p = text;

    while (count < max_calls && *p) {
        /* Look for "name" key as anchor */
        const char *anchor = strstr(p, "\"name\"");
        if (!anchor) break;

        /* Backtrack to nearest { */
        const char *brace = anchor;
        while (brace > text && *brace != '{') brace--;
        if (*brace != '{') { p = anchor + 6; continue; }

        /* Brace-depth counting to find matching } */
        int depth = 0;
        int in_str = 0;
        const char *scan = brace;
        const char *match_end = NULL;
        while (*scan) {
            char c = *scan;
            if (in_str) {
                if (c == '\\') { scan++; if (*scan) scan++; continue; }
                if (c == '"') in_str = 0;
                scan++;
                continue;
            }
            if (c == '"') { in_str = 1; scan++; continue; }
            if (c == '{') depth++;
            else if (c == '}') {
                depth--;
                if (depth == 0) { match_end = scan + 1; break; }
            }
            scan++;
        }
        if (!match_end) { p = anchor + 6; continue; }

        /* Extract candidate */
        size_t clen = (size_t)(match_end - brace);
        char *repaired = resp_repair_json(brace, clen);
        if (!repaired) { p = match_end; continue; }

        struct json_object *obj = json_tokener_parse(repaired);
        free(repaired);
        if (!obj) { p = match_end; continue; }

        /* Validate: must have "name" (string) */
        struct json_object *name_obj;
        if (!json_object_object_get_ex(obj, "name", &name_obj) ||
            !json_object_is_type(name_obj, json_type_string)) {
            json_object_put(obj);
            p = match_end;
            continue;
        }

        const char *name = json_object_get_string(name_obj);
        snprintf(out[count].name, RESP_MAX_NAME, "%s",
                 name ? name : "unknown");

        /* Extract arguments */
        struct json_object *args_obj;
        if (json_object_object_get_ex(obj, "arguments", &args_obj)) {
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
                const char *as = json_object_to_json_string_ext(
                    args_obj, JSON_C_TO_STRING_PLAIN);
                snprintf(out[count].arguments, RESP_MAX_ARGS,
                         "%s", as ? as : "{}");
            }
        } else {
            snprintf(out[count].arguments, RESP_MAX_ARGS, "{}");
        }

        resp_gen_id(out[count].call_id, "call_", 24);
        fprintf(stderr, "  [responses] loose JSON extraction: %s\n",
                out[count].name);
        json_object_put(obj);
        count++;
        p = match_end;
    }

    return count;
}
