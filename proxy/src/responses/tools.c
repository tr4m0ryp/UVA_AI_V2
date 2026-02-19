#include "responses.h"
#include "responses_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
 * Convert OpenAI tool definitions to an XML system prompt.
 *
 * Input format (tools_json is the "tools" array):
 * [{"type":"function","function":{"name":"get_weather",
 *   "description":"Get weather","parameters":{...}}}]
 *
 * Output: XML prompt instructing the model how to call tools.
 */
char *resp_tools_to_xml(struct json_object *tools_json)
{
    if (!tools_json || !json_object_is_type(tools_json, json_type_array))
        return NULL;

    int count = (int)json_object_array_length(tools_json);
    if (count == 0) return NULL;

    /* Build into dynamic buffer */
    size_t cap = 4096;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    size_t pos = 0;

    /* Header */
    const char *header =
        "You have access to the following tools. "
        "To call a tool, emit exactly one XML block:\n"
        "<tool_call name=\"TOOL_NAME\">"
        "{\"arg1\": \"value1\"}</tool_call>\n\n"
        "You may emit multiple tool_call blocks if needed.\n\n"
        "Available tools:\n";
    size_t hlen = strlen(header);
    if (pos + hlen >= cap) {
        cap = pos + hlen + 2048;
        buf = realloc(buf, cap);
        if (!buf) return NULL;
    }
    memcpy(buf + pos, header, hlen);
    pos += hlen;

    for (int i = 0; i < count; i++) {
        struct json_object *tool = json_object_array_get_idx(tools_json,
                                                              (size_t)i);
        struct json_object *func_obj;
        if (!json_object_object_get_ex(tool, "function", &func_obj))
            continue;

        struct json_object *name_obj, *desc_obj, *params_obj;
        const char *name = "unknown";
        const char *desc = "";
        const char *params = "{}";

        if (json_object_object_get_ex(func_obj, "name", &name_obj))
            name = json_object_get_string(name_obj);
        if (json_object_object_get_ex(func_obj, "description", &desc_obj))
            desc = json_object_get_string(desc_obj);
        if (json_object_object_get_ex(func_obj, "parameters", &params_obj))
            params = json_object_to_json_string_ext(params_obj,
                JSON_C_TO_STRING_PLAIN);

        /* <tool name="..."><description>...</description>
         *   <parameters>...</parameters></tool> */
        char entry[4096];
        int elen = snprintf(entry, sizeof(entry),
            "<tool name=\"%s\">\n"
            "  <description>%s</description>\n"
            "  <parameters>%s</parameters>\n"
            "</tool>\n",
            name, desc, params);

        if (pos + (size_t)elen >= cap) {
            cap = pos + (size_t)elen + 2048;
            buf = realloc(buf, cap);
            if (!buf) return NULL;
        }
        memcpy(buf + pos, entry, (size_t)elen);
        pos += (size_t)elen;
    }

    /* Footer */
    const char *footer =
        "\nIf you do not need to call a tool, "
        "reply normally without any XML blocks.\n";
    size_t flen = strlen(footer);
    if (pos + flen >= cap) {
        cap = pos + flen + 64;
        buf = realloc(buf, cap);
        if (!buf) return NULL;
    }
    memcpy(buf + pos, footer, flen);
    pos += flen;
    buf[pos] = '\0';

    return buf;
}

/*
 * Parse <tool_call name="...">...</tool_call> blocks from model output.
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
    const char *open_tag = "<tool_call";
    const char *close_tag = "</tool_call>";
    size_t close_len = strlen(close_tag);

    while (count < max_calls) {
        /* Find opening tag */
        const char *start = strstr(p, open_tag);
        if (!start) break;

        /* Extract name from name="..." attribute */
        const char *name_attr = strstr(start, "name=\"");
        if (!name_attr || name_attr > start + 128) {
            p = start + 1;
            continue;
        }
        name_attr += 6; /* skip name=" */
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

        /* Extract arguments (content between tags) */
        size_t args_len = (size_t)(end - content_start);

        /* Skip leading/trailing whitespace in arguments */
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

        /* Generate call_id: call_ + 24 hex chars */
        resp_gen_id(out[count].call_id, "call_", 24);

        count++;
        p = end + close_len;
    }

    return count;
}

/*
 * Strip tool_call XML blocks from text, returning cleaned text.
 * Caller frees the returned string.
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
        const char *start = strstr(p, "<tool_call");
        if (!start) {
            /* Copy remaining text */
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
            p = end + 12; /* strlen("</tool_call>") */
        } else {
            /* No closing tag found, copy rest as-is */
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
