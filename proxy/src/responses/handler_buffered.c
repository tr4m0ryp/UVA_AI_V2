#include "responses.h"
#include "responses_internal.h"
#include "upstream.h"
#include "stream.h"
#include <stdlib.h>
#include <string.h>

/*
 * Text buffered path (non-streaming): accumulates all tokens silently.
 * No SSE events are emitted. Sets result->full_text when done.
 */
int resp_handle_text_buffered(const char *uva_body,
                              const char *cookie, const char *model,
                              resp_result_t *result)
{
    (void)model;

    buffer_t accum;
    buffer_init(&accum);

    stream_parser_t *parser = stream_parser_new(resp_on_buffer_token,
                                                 &accum);
    if (!parser) {
        buffer_free(&accum);
        return -1;
    }

    resp_buffer_cb_ctx_t bcctx;
    bcctx.parser = parser;

    buffer_t resp_buf;
    buffer_init(&resp_buf);

    int http_code = upstream_chat_with_cookie(
        uva_body, strlen(uva_body),
        cookie, resp_buffer_write_cb, &bcctx, &resp_buf);

    stream_parser_finish(parser);
    stream_parser_free(parser);
    buffer_free(&resp_buf);

    if (http_code < 0 || http_code >= 500) {
        buffer_free(&accum);
        return -1;
    }

    result->full_text = accum.data ? accum.data : strdup("");
    if (accum.data) accum.data = NULL;
    buffer_free(&accum);
    return 0;
}

/*
 * Tool buffered path (non-streaming): buffers response, parses tool calls.
 * No SSE events are emitted. Populates result->tool_calls or
 * result->full_text.
 */
int resp_handle_tool_buffered(const char *uva_body,
                              const char *cookie, const char *model,
                              resp_result_t *result)
{
    (void)model;

    buffer_t accum;
    buffer_init(&accum);

    stream_parser_t *parser = stream_parser_new(resp_on_buffer_token,
                                                 &accum);
    if (!parser) {
        buffer_free(&accum);
        return -1;
    }

    resp_buffer_cb_ctx_t bcctx;
    bcctx.parser = parser;

    buffer_t resp_buf;
    buffer_init(&resp_buf);

    int http_code = upstream_chat_with_cookie(
        uva_body, strlen(uva_body),
        cookie, resp_buffer_write_cb, &bcctx, &resp_buf);

    stream_parser_finish(parser);
    stream_parser_free(parser);
    buffer_free(&resp_buf);

    if (http_code < 0 || http_code >= 500) {
        buffer_free(&accum);
        return -1;
    }

    const char *full_text = accum.data ? accum.data : "";

    resp_tool_call_t calls[RESP_MAX_TOOL_CALLS];
    int n = resp_parse_tool_calls(full_text, calls, RESP_MAX_TOOL_CALLS);

    if (n > 0) {
        result->tool_call_count = n;
        memcpy(result->tool_calls, calls,
               (size_t)n * sizeof(resp_tool_call_t));
        result->full_text = resp_strip_tool_calls(full_text);
    } else {
        result->full_text = accum.data ? strdup(accum.data) : strdup("");
        result->tool_call_count = 0;
    }

    buffer_free(&accum);
    return 0;
}
