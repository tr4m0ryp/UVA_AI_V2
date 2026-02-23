#include "stream/stream.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>

/*
 * Parse upstream SSE stream from UvA.
 *
 * Expected upstream format (to be confirmed by Phase 1 traffic capture):
 *
 * Format A - Standard SSE with JSON:
 *   data: {"content":"token text","done":false}
 *   data: {"content":"","done":true}
 *
 * Format B - Plain text SSE:
 *   data: token text here
 *   data: [DONE]
 *
 * Format C - OpenAI-compatible SSE (if UvA wraps the provider):
 *   data: {"choices":[{"delta":{"content":"token"}}]}
 *
 * We handle all three formats.
 */

#define PARSER_BUF_SIZE (64 * 1024)

struct stream_parser {
    token_callback_t  callback;
    void             *userdata;
    char             *buffer;
    size_t            buf_len;
    size_t            buf_cap;
};

stream_parser_t *stream_parser_new(token_callback_t cb, void *userdata)
{
    stream_parser_t *p = calloc(1, sizeof(*p));
    if (!p) return NULL;

    p->callback = cb;
    p->userdata = userdata;
    p->buffer = malloc(PARSER_BUF_SIZE);
    if (!p->buffer) {
        free(p);
        return NULL;
    }
    p->buf_len = 0;
    p->buf_cap = PARSER_BUF_SIZE;
    return p;
}

static void process_sse_data(stream_parser_t *p, const char *data)
{
    /* Skip empty lines */
    if (!data || !*data) return;

    /* Check for [DONE] marker */
    if (strcmp(data, "[DONE]") == 0)
        return;

    /* Try parsing as JSON */
    struct json_object *obj = json_tokener_parse(data);
    if (obj) {
        struct json_object *type_obj;
        if (json_object_object_get_ex(obj, "type", &type_obj)) {
            const char *type = json_object_get_string(type_obj);

            /* Vercel AI SDK Data Stream: {"type":"text-delta","delta":"..."} */
            if (type && strcmp(type, "text-delta") == 0) {
                struct json_object *delta;
                if (json_object_object_get_ex(obj, "delta", &delta)) {
                    const char *text = json_object_get_string(delta);
                    if (text && *text)
                        p->callback(text, p->userdata);
                }
                json_object_put(obj);
                return;
            }

            /* Vercel AI SDK error: {"type":"error","errorText":"..."} */
            if (type && strcmp(type, "error") == 0) {
                struct json_object *err;
                if (json_object_object_get_ex(obj, "errorText", &err)) {
                    const char *text = json_object_get_string(err);
                    if (text)
                        fprintf(stderr, "UvA stream error: %s\n", text);
                }
                json_object_put(obj);
                return;
            }

            /* Skip other Vercel types: start, start-step, text-start,
             * text-end, finish-step, finish */
            json_object_put(obj);
            return;
        }

        /* OpenAI-style: {"choices":[{"delta":{"content":"..."}}]} */
        struct json_object *choices;
        if (json_object_object_get_ex(obj, "choices", &choices)) {
            struct json_object *choice =
                json_object_array_get_idx(choices, 0);
            if (choice) {
                struct json_object *delta;
                if (json_object_object_get_ex(choice, "delta", &delta)) {
                    struct json_object *content;
                    if (json_object_object_get_ex(delta, "content",
                                                   &content)) {
                        const char *text =
                            json_object_get_string(content);
                        if (text && *text)
                            p->callback(text, p->userdata);
                    }
                }
            }
            json_object_put(obj);
            return;
        }

        /* Fallback: {"content":"..."} or {"delta":"..."} */
        struct json_object *content;
        if (json_object_object_get_ex(obj, "content", &content) ||
            json_object_object_get_ex(obj, "delta", &content)) {
            const char *text = json_object_get_string(content);
            if (text && *text)
                p->callback(text, p->userdata);
        }
        json_object_put(obj);
        return;
    }
}

static void process_line(stream_parser_t *p, const char *line, size_t len)
{
    /* SSE lines starting with "data: " */
    if (len > 6 && strncmp(line, "data: ", 6) == 0) {
        /* Extract the data portion */
        char *data = strndup(line + 6, len - 6);
        if (data) {
            /* Trim trailing whitespace */
            size_t dlen = strlen(data);
            while (dlen > 0 && (data[dlen-1] == '\n' ||
                                data[dlen-1] == '\r' ||
                                data[dlen-1] == ' '))
                data[--dlen] = '\0';
            process_sse_data(p, data);
            free(data);
        }
        return;
    }

    /* Also handle "data:" without space */
    if (len > 5 && strncmp(line, "data:", 5) == 0) {
        char *data = strndup(line + 5, len - 5);
        if (data) {
            size_t dlen = strlen(data);
            while (dlen > 0 && (data[dlen-1] == '\n' ||
                                data[dlen-1] == '\r' ||
                                data[dlen-1] == ' '))
                data[--dlen] = '\0';
            process_sse_data(p, data);
            free(data);
        }
    }

    /*
     * If the upstream does not use SSE at all but returns raw text or
     * raw JSON, we handle that in stream_parser_finish by treating
     * the entire buffer as content.
     */
}

void stream_parser_feed(stream_parser_t *p, const char *data, size_t len)
{
    /* Append to buffer */
    if (p->buf_len + len >= p->buf_cap) {
        size_t new_cap = p->buf_cap * 2;
        while (new_cap < p->buf_len + len + 1)
            new_cap *= 2;
        char *tmp = realloc(p->buffer, new_cap);
        if (!tmp) return;
        p->buffer = tmp;
        p->buf_cap = new_cap;
    }
    memcpy(p->buffer + p->buf_len, data, len);
    p->buf_len += len;
    p->buffer[p->buf_len] = '\0';

    /* Process complete lines */
    char *start = p->buffer;
    char *newline;
    while ((newline = strchr(start, '\n')) != NULL) {
        size_t line_len = (size_t)(newline - start);
        process_line(p, start, line_len);
        start = newline + 1;
    }

    /* Move remaining data to start of buffer */
    size_t remaining = (size_t)(p->buffer + p->buf_len - start);
    if (remaining > 0 && start != p->buffer)
        memmove(p->buffer, start, remaining);
    p->buf_len = remaining;
}

void stream_parser_finish(stream_parser_t *p)
{
    /* Process any remaining data */
    if (p->buf_len > 0) {
        p->buffer[p->buf_len] = '\0';
        process_line(p, p->buffer, p->buf_len);
        p->buf_len = 0;
    }
}

void stream_parser_free(stream_parser_t *p)
{
    if (!p) return;
    free(p->buffer);
    free(p);
}
