#include "api/responses/responses.h"
#include "api/responses/responses_internal.h"
#include "api/upstream/upstream.h"
#include "api/stream/stream.h"
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
