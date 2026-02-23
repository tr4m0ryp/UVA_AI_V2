#include "api/responses/responses.h"
#include "api/responses/responses_internal.h"
#include "api/upstream/upstream.h"
#include "api/stream/stream.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Token callback for text streaming path.
 * Emits SSE events as tokens arrive from upstream.
 */
static void on_text_token(const char *token, void *userdata)
{
    resp_stream_ctx_t *ctx = (resp_stream_ctx_t *)userdata;

    /* On first token, emit the preamble events */
    if (ctx->first_token) {
        ctx->first_token = 0;
        resp_emit_created(ctx->client_fd, ctx->result, ctx->model);
        resp_emit_output_item_added(ctx->client_fd, ctx->result,
                                     ctx->model);
        resp_emit_content_part_added(ctx->client_fd, ctx->result);
    }

    resp_emit_text_delta(ctx->client_fd, ctx->result, token);
    buffer_append(&ctx->accum, token, strlen(token));
}

/* curl write callback: feeds raw upstream data to the stream parser. */
static size_t resp_stream_cb(const char *data, size_t len, void *userdata)
{
    resp_stream_ctx_t *ctx = (resp_stream_ctx_t *)userdata;
    stream_parser_feed(ctx->parser, data, len);
    return len;
}

/* Shared token callback for buffered paths. */
void resp_on_buffer_token(const char *token, void *userdata)
{
    buffer_t *accum = (buffer_t *)userdata;
    buffer_append(accum, token, strlen(token));
}

/* Shared curl write callback for buffered paths. */
size_t resp_buffer_write_cb(const char *data, size_t len, void *userdata)
{
    resp_buffer_cb_ctx_t *ctx = (resp_buffer_cb_ctx_t *)userdata;
    stream_parser_feed(ctx->parser, data, len);
    return len;
}

/* Text stream path: tokens are emitted as they arrive. */
int resp_handle_text_stream(int fd, const char *uva_body,
                            const char *cookie, const char *model,
                            resp_result_t *result)
{
    resp_stream_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.client_fd = fd;
    ctx.result = result;
    ctx.model = model;
    ctx.first_token = 1;
    buffer_init(&ctx.accum);
    ctx.parser = stream_parser_new(on_text_token, &ctx);
    if (!ctx.parser) return -1;

    buffer_t resp_buf;
    buffer_init(&resp_buf);

    int http_code = upstream_chat_with_cookie(
        uva_body, strlen(uva_body),
        cookie, resp_stream_cb, &ctx, &resp_buf);

    stream_parser_finish(ctx.parser);
    stream_parser_free(ctx.parser);
    buffer_free(&resp_buf);

    if (http_code < 0 || http_code >= 500) {
        buffer_free(&ctx.accum);
        return -1;
    }

    if (ctx.accum.data) {
        result->full_text = ctx.accum.data;
        ctx.accum.data = NULL;
    } else {
        result->full_text = strdup("");
    }
    buffer_free(&ctx.accum);

    if (!ctx.first_token) {
        resp_emit_text_done(fd, result);
        resp_emit_content_part_done(fd, result);
        resp_emit_output_item_done(fd, result, model);
    }

    return 0;
}
