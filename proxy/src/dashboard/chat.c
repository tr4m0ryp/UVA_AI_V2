#include "dashboard/dashboard.h"
#include "database/database.h"
#include "server/server.h"
#include "upstream/upstream.h"
#include "translator/translator.h"
#include "stream/stream.h"
#include "platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <json-c/json.h>

/* response.c */
void response_send_json(int fd, int status, const char *json);
void response_send_error(int fd, int status, const char *message);
void response_start_sse(int fd);
void response_send_sse_data(int fd, const char *data);
void response_end_sse(int fd);

/* actions.c */
char *actions_get_or_create_thread(void);

/*
 * Streaming context for dashboard chat.
 * Same pattern as upstream/chat.c but without API key resolution.
 */
typedef struct {
    int              client_fd;
    const char      *model;
    char            *completion_id;
    stream_parser_t *parser;
    buffer_t         accum;     /* accumulates tokens in non-streaming mode */
    int              streaming;
} dash_chat_ctx_t;

static void on_token(const char *token, void *userdata)
{
    dash_chat_ctx_t *ctx = (dash_chat_ctx_t *)userdata;
    if (ctx->streaming) {
        char *chunk = translate_stream_chunk(token, ctx->model,
                                             ctx->completion_id, 0);
        if (chunk) {
            response_send_sse_data(ctx->client_fd, chunk);
            free(chunk);
        }
    } else {
        buffer_append(&ctx->accum, token, strlen(token));
    }
}

static size_t on_upstream_data(const char *data, size_t len, void *userdata)
{
    dash_chat_ctx_t *ctx = (dash_chat_ctx_t *)userdata;
    stream_parser_feed(ctx->parser, data, len);
    return len;
}

/*
 * POST /api/dashboard/chat
 *
 * Accepts OpenAI-compatible body: { model, messages, stream? }
 * Authenticated via dashboard token (resolved by router before calling us).
 * Uses the user's uva_session cookie for upstream requests.
 */
void dashboard_chat_handle(http_request_t *req, const db_user_t *user)
{
    if (strcmp(req->method, "POST") != 0) {
        response_send_error(req->client_fd, 405, "Method not allowed");
        return;
    }

    if (!req->body || req->body_len == 0) {
        response_send_error(req->client_fd, 400, "Request body required");
        return;
    }

    if (!user->uva_session[0]) {
        response_send_error(req->client_fd, 400,
            "No UvA session available. Please re-login.");
        return;
    }

    /* Parse request body */
    struct json_object *parsed = json_tokener_parse(req->body);
    if (!parsed) {
        response_send_error(req->client_fd, 400, "Invalid JSON");
        return;
    }

    /* Extract model */
    char model_buf[DB_MAX_MODEL];
    struct json_object *model_obj;
    if (json_object_object_get_ex(parsed, "model", &model_obj)) {
        snprintf(model_buf, sizeof(model_buf), "%s",
                 json_object_get_string(model_obj));
    } else {
        snprintf(model_buf, sizeof(model_buf), "gpt-4.1");
    }

    /* Check stream flag (default true for dashboard chat) */
    int streaming = 1;
    struct json_object *stream_obj;
    if (json_object_object_get_ex(parsed, "stream", &stream_obj))
        streaming = json_object_get_boolean(stream_obj);

    json_object_put(parsed);

    fprintf(stderr, "  [dashboard-chat] user=%s model=%s stream=%d\n",
            user->email, model_buf, streaming);

    /* Get a fresh thread ID */
    char *thread_id = actions_get_or_create_thread();
    if (!thread_id) {
        response_send_error(req->client_fd, 502,
            "Failed to obtain chat thread from UvA");
        return;
    }

    /* Translate OpenAI request to UvA format */
    char *uva_body = translate_request(req->body, thread_id);
    free(thread_id);

    if (!uva_body) {
        response_send_error(req->client_fd, 500,
            "Failed to translate request");
        return;
    }

    /* Generate completion ID */
    char cid[64];
    snprintf(cid, sizeof(cid), "chatcmpl-%lx%04x",
             (unsigned long)time(NULL), rand() & 0xFFFF);

    /* Set up streaming context */
    dash_chat_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.client_fd = req->client_fd;
    ctx.model = model_buf;
    ctx.completion_id = cid;
    ctx.streaming = streaming;
    buffer_init(&ctx.accum);
    ctx.parser = stream_parser_new(on_token, &ctx);

    if (!ctx.parser) {
        free(uva_body);
        response_send_error(req->client_fd, 500, "Parser init failed");
        return;
    }

    /* Start SSE response */
    if (streaming)
        response_start_sse(req->client_fd);

    /* Send to upstream using the user's session cookie */
    buffer_t resp_buf;
    int http_code = upstream_chat_with_cookie(uva_body, strlen(uva_body),
        user->uva_session, on_upstream_data, &ctx, &resp_buf);
    free(uva_body);

    /* Flush parser */
    stream_parser_finish(ctx.parser);
    stream_parser_free(ctx.parser);

    if (http_code < 0 || http_code >= 500) {
        if (!streaming) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Upstream error (HTTP %d)", http_code);
            response_send_error(req->client_fd, 502, msg);
        }
        buffer_free(&ctx.accum);
        return;
    }

    if (streaming) {
        /* Send stop chunk and end SSE */
        char *stop = translate_stream_chunk(NULL, model_buf, cid, 0);
        if (stop) {
            response_send_sse_data(req->client_fd, stop);
            free(stop);
        }
        response_end_sse(req->client_fd);
    } else {
        /* Non-streaming: build response from accumulated tokens */
        char *resp_json = translate_response(
            ctx.accum.data ? ctx.accum.data : "", model_buf, cid);
        if (resp_json) {
            response_send_json(req->client_fd, 200, resp_json);
            free(resp_json);
        } else {
            response_send_error(req->client_fd, 500,
                "Failed to build response");
        }
    }

    buffer_free(&ctx.accum);
}
