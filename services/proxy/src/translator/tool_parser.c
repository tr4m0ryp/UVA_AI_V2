/*
 * tool_parser.c - Parse <tool_call> XML tags from model text,
 * build OpenAI tool_calls response or streaming SSE sequence.
 */
#include "translator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* response.c forward declaration */
void response_send_sse_data(int fd, const char *data);

/* -----------------------------------------------------------------------
 * Random hex ID generation
 * ----------------------------------------------------------------------- */
static void gen_hex(char *buf, int len)
{
    static const char hex[] = "0123456789abcdef";
    srand((unsigned)time(NULL) ^ (unsigned)(uintptr_t)buf);
    for (int i = 0; i < len; i++)
        buf[i] = hex[rand() & 0xf];
    buf[len] = '\0';
}

/* -----------------------------------------------------------------------
 * parse_tool_calls_from_text
 * Scan text for <tool_call name="X">JSON</tool_call> blocks.
 * Returns count found. Fills calls[].
 * ----------------------------------------------------------------------- */
int parse_tool_calls_from_text(const char *text,
                                parsed_tool_call_t *calls, int max_calls)
{
    if (!text || !calls || max_calls <= 0) return 0;

    int count = 0;
    const char *p = text;
    const char *tag_open = "<tool_call name=\"";
    size_t tag_open_len = strlen(tag_open);
    const char *tag_close = "</tool_call>";
    size_t tag_close_len = strlen(tag_close);

    while (count < max_calls) {
        /* Find opening tag */
        const char *start = strstr(p, tag_open);
        if (!start) break;
        start += tag_open_len;

        /* Extract name until closing quote + > */
        const char *name_end = strchr(start, '"');
        if (!name_end) break;
        size_t name_len = (size_t)(name_end - start);
        if (name_len == 0 || name_len >= sizeof(calls[count].name)) {
            p = name_end + 1;
            continue;
        }

        /* Expect "> after name quote */
        const char *after_name = name_end + 1;
        if (*after_name != '>') {
            p = after_name;
            continue;
        }
        const char *body_start = after_name + 1;

        /* Find closing tag */
        const char *body_end = strstr(body_start, tag_close);
        if (!body_end) break;

        size_t body_len = (size_t)(body_end - body_start);

        /* Trim whitespace from body */
        while (body_len > 0 && (body_start[0] == ' ' || body_start[0] == '\n'
                                  || body_start[0] == '\r' || body_start[0] == '\t')) {
            body_start++;
            body_len--;
        }
        while (body_len > 0 && (body_start[body_len - 1] == ' '
                                  || body_start[body_len - 1] == '\n'
                                  || body_start[body_len - 1] == '\r'
                                  || body_start[body_len - 1] == '\t')) {
            body_len--;
        }

        if (body_len >= sizeof(calls[count].args_json)) {
            p = body_end + tag_close_len;
            continue;
        }

        /* Fill the struct */
        memcpy(calls[count].name, start, name_len);
        calls[count].name[name_len] = '\0';
        memcpy(calls[count].args_json, body_start, body_len);
        calls[count].args_json[body_len] = '\0';

        /* Generate call_id */
        char hex_part[17];
        gen_hex(hex_part, 16);
        snprintf(calls[count].call_id, sizeof(calls[count].call_id),
                 "call_%s", hex_part);

        count++;
        p = body_end + tag_close_len;
    }

    return count;
}

/* -----------------------------------------------------------------------
 * build_tool_calls_response
 * Build a non-streaming OpenAI response with finish_reason:"tool_calls".
 * Caller frees the returned string.
 * ----------------------------------------------------------------------- */
char *build_tool_calls_response(const parsed_tool_call_t *calls, int count,
                                 const char *model, const char *completion_id)
{
    if (!calls || count <= 0) return NULL;

    /* Build tool_calls JSON array */
    char *tc_buf = NULL;
    size_t tc_cap = 256 + (size_t)count * 512;
    tc_buf = malloc(tc_cap);
    if (!tc_buf) return NULL;

    int pos = 0;
    pos += snprintf(tc_buf + pos, tc_cap - (size_t)pos, "[");
    for (int i = 0; i < count; i++) {
        if (i > 0) pos += snprintf(tc_buf + pos, tc_cap - (size_t)pos, ",");

        /* Escape args_json for embedding - it's already valid JSON, use directly */
        pos += snprintf(tc_buf + pos, tc_cap - (size_t)pos,
            "{\"id\":\"%s\",\"type\":\"function\","
            "\"function\":{\"name\":\"%s\",\"arguments\":%s}}",
            calls[i].call_id, calls[i].name,
            calls[i].args_json[0] ? calls[i].args_json : "{}");
    }
    pos += snprintf(tc_buf + pos, tc_cap - (size_t)pos, "]");

    /* Build full response */
    size_t resp_cap = 512 + (size_t)pos;
    char *resp = malloc(resp_cap);
    if (!resp) { free(tc_buf); return NULL; }

    snprintf(resp, resp_cap,
        "{\"id\":\"%s\",\"object\":\"chat.completion\","
        "\"created\":%lld,\"model\":\"%s\","
        "\"choices\":[{\"index\":0,\"message\":"
        "{\"role\":\"assistant\",\"content\":null,"
        "\"tool_calls\":%s},\"finish_reason\":\"tool_calls\"}],"
        "\"usage\":{\"prompt_tokens\":0,\"completion_tokens\":0,"
        "\"total_tokens\":0}}",
        completion_id ? completion_id : "chatcmpl-0",
        (long long)time(NULL),
        model ? model : "unknown",
        tc_buf);

    free(tc_buf);
    return resp;
}

/* -----------------------------------------------------------------------
 * emit_tool_calls_sse
 * Write streaming SSE chunks for tool calls to client_fd.
 * Follows OpenAI streaming tool_calls format.
 * ----------------------------------------------------------------------- */
void emit_tool_calls_sse(int client_fd, const parsed_tool_call_t *calls,
                          int count, const char *model,
                          const char *completion_id)
{
    if (!calls || count <= 0) return;

    long long ts = (long long)time(NULL);
    const char *cid = completion_id ? completion_id : "chatcmpl-0";
    const char *mod = model ? model : "unknown";

    for (int i = 0; i < count; i++) {
        /* First chunk for this tool: announce it */
        char buf[1024];
        snprintf(buf, sizeof(buf),
            "{\"id\":\"%s\",\"object\":\"chat.completion.chunk\","
            "\"created\":%lld,\"model\":\"%s\","
            "\"choices\":[{\"index\":0,\"delta\":"
            "{\"role\":\"assistant\",\"content\":null,"
            "\"tool_calls\":[{\"index\":%d,\"id\":\"%s\","
            "\"type\":\"function\",\"function\":{\"name\":\"%s\","
            "\"arguments\":\"\"}}]},"
            "\"finish_reason\":null}]}",
            cid, ts, mod, i, calls[i].call_id, calls[i].name);
        response_send_sse_data(client_fd, buf);

        /* Second chunk: the arguments */
        /* JSON-encode the args string for embedding in a JSON string value */
        const char *raw = calls[i].args_json;
        size_t rlen = strlen(raw);
        char *escaped = malloc(rlen * 2 + 4);
        if (!escaped) continue;
        size_t ep = 0;
        for (size_t j = 0; j < rlen; j++) {
            char c = raw[j];
            if (c == '"')      { escaped[ep++] = '\\'; escaped[ep++] = '"'; }
            else if (c == '\\') { escaped[ep++] = '\\'; escaped[ep++] = '\\'; }
            else if (c == '\n') { escaped[ep++] = '\\'; escaped[ep++] = 'n'; }
            else if (c == '\r') { escaped[ep++] = '\\'; escaped[ep++] = 'r'; }
            else if (c == '\t') { escaped[ep++] = '\\'; escaped[ep++] = 't'; }
            else { escaped[ep++] = c; }
        }
        escaped[ep] = '\0';

        size_t args_cap = ep + 256;
        char *args_chunk = malloc(args_cap);
        if (!args_chunk) { free(escaped); continue; }
        snprintf(args_chunk, args_cap,
            "{\"id\":\"%s\",\"object\":\"chat.completion.chunk\","
            "\"created\":%lld,\"model\":\"%s\","
            "\"choices\":[{\"index\":0,\"delta\":"
            "{\"tool_calls\":[{\"index\":%d,"
            "\"function\":{\"arguments\":\"%s\"}}]},"
            "\"finish_reason\":null}]}",
            cid, ts, mod, i, escaped);
        response_send_sse_data(client_fd, args_chunk);
        free(escaped);
        free(args_chunk);
    }

    /* Final chunk: finish_reason tool_calls */
    char final_buf[512];
    snprintf(final_buf, sizeof(final_buf),
        "{\"id\":\"%s\",\"object\":\"chat.completion.chunk\","
        "\"created\":%lld,\"model\":\"%s\","
        "\"choices\":[{\"index\":0,\"delta\":{},"
        "\"finish_reason\":\"tool_calls\"}]}",
        cid, ts, mod);
    response_send_sse_data(client_fd, final_buf);
}
