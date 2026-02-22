#include "server.h"
#include "upstream.h"
#include "translator.h"
#include "think_parser.h"
#include "stream.h"
#include "platform.h"
#include "apikey.h"
#include "database.h"
#include "idgen.h"
#include "thread_state.h"
#include "chat_thread.h"
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

/* chat_overrides.c */
char *apply_apikey_overrides(const char *body, const db_api_key_t *ak,
                              char *model_buf, size_t model_size);

typedef struct {
    int              client_fd;
    const char      *model;
    char            *completion_id;
    stream_parser_t *parser;
    think_parser_t  *think;
    buffer_t         accum;
    int              streaming;
    int              completion_chars;
} chat_stream_ctx_t;

static void on_think_emit(const char *text, int is_reasoning, void *userdata)
{
    chat_stream_ctx_t *ctx = (chat_stream_ctx_t *)userdata;
    ctx->completion_chars += (int)strlen(text);

    if (is_reasoning) {
        char *json = translate_stream_chunk_ex(text, "reasoning_content",
                         ctx->model, ctx->completion_id, 0);
        if (json) {
            char line[8192];
            int n = snprintf(line, sizeof(line), "data: %s\n\n", json);
            platform_send(ctx->client_fd, line, (size_t)n);
            free(json);
        }
    } else {
        stream_emit_chunk(ctx->client_fd, ctx->model,
                          ctx->completion_id, text, 0);
    }
}

static void on_token(const char *token, void *userdata)
{
    chat_stream_ctx_t *ctx = (chat_stream_ctx_t *)userdata;
    if (ctx->streaming) {
        think_parser_feed(ctx->think, token);
    } else {
        ctx->completion_chars += (int)strlen(token);
        buffer_append(&ctx->accum, token, strlen(token));
    }
}

static size_t upstream_stream_cb(const char *data, size_t len, void *userdata)
{
    chat_stream_ctx_t *ctx = (chat_stream_ctx_t *)userdata;
    stream_parser_feed(ctx->parser, data, len);
    return len;
}

/* Resolve thread state: reuse existing thread or create new one. */
static int resolve_thread(const char *effective_body,
                           struct json_object *parsed,
                           const db_api_key_t *ak,
                           const char *model,
                           char **uva_body, int *is_incremental,
                           char *conv_key_out)
{
    *is_incremental = 0;
    conv_key_out[0] = '\0';
    struct json_object *msgs;
    if (!json_object_object_get_ex(parsed, "messages", &msgs)) {
        char *tid = actions_get_or_create_thread();
        *uva_body = translate_request(effective_body, tid);
        free(tid);
        return *uva_body ? 0 : -1;
    }

    const char *first_user = chat_first_user_content(msgs);
    const char *last_user = chat_last_user_content(msgs);
    int msg_count = chat_count_messages(msgs);

    char conv_key[65];
    if (!first_user || chat_make_conv_key(ak->id, model, first_user,
                                           conv_key, sizeof(conv_key)) != 0) {
        char *tid = actions_get_or_create_thread();
        *uva_body = translate_request(effective_body, tid);
        free(tid);
        return *uva_body ? 0 : -1;
    }

    snprintf(conv_key_out, 65, "%s", conv_key);
    thread_state_t ts;
    if (thread_state_find(conv_key, &ts) == 0 && msg_count > ts.message_count) {
        /* Incremental mode: reuse existing UvA thread */
        fprintf(stderr, "  [chat] incremental mode: thread=%s msgs=%d->%d\n",
                ts.uva_thread_id, ts.message_count, msg_count);
        *uva_body = translate_request_incremental(last_user, "user",
                                                    ts.uva_thread_id, NULL);
        if (*uva_body) {
            *is_incremental = 1;
            return 0;
        }
    }

    char *tid = actions_get_or_create_thread();
    *uva_body = translate_request(effective_body, tid);
    fprintf(stderr, "  [chat] full mode: new thread=%s msgs=%d\n",
            tid, msg_count);
    thread_state_t new_ts;
    memset(&new_ts, 0, sizeof(new_ts));
    snprintf(new_ts.conv_key, sizeof(new_ts.conv_key), "%s", conv_key);
    snprintf(new_ts.uva_thread_id, sizeof(new_ts.uva_thread_id), "%s", tid);
    snprintf(new_ts.model, sizeof(new_ts.model), "%s", model);
    new_ts.message_count = msg_count;
    thread_state_save(&new_ts);

    free(tid);
    return *uva_body ? 0 : -1;
}

void handle_chat_completions(http_request_t *req)
{
    if (strcmp(req->method, "POST") != 0) {
        response_send_error(req->client_fd, 405, "Method not allowed");
        return;
    }

    if (!req->body || req->body_len == 0) {
        response_send_error(req->client_fd, 400, "Request body required");
        return;
    }

    db_api_key_t resolved_key;
    const char *auth = request_get_header(req, "Authorization");
    if (!auth) {
        response_send_error(req->client_fd, 401, "Authorization required");
        return;
    }

    const char *key = apikey_extract(auth);
    if (!key || apikey_resolve(key, &resolved_key) != 0) {
        response_send_error(req->client_fd, 401,
            "Invalid or inactive API key");
        return;
    }

    fprintf(stderr, "  [chat] API key: %s (model: %s)\n",
            resolved_key.key_prefix, resolved_key.model);

    char model_buf[DB_MAX_MODEL];
    char *effective_body = apply_apikey_overrides(req->body, &resolved_key,
                                                  model_buf, sizeof(model_buf));
    if (!effective_body) {
        response_send_error(req->client_fd, 400, "Invalid JSON");
        return;
    }

    struct json_object *parsed = json_tokener_parse(effective_body);
    if (!parsed) {
        free(effective_body);
        response_send_error(req->client_fd, 400, "Invalid JSON");
        return;
    }

    int streaming = 0;
    struct json_object *stream_obj;
    if (json_object_object_get_ex(parsed, "stream", &stream_obj))
        streaming = json_object_get_boolean(stream_obj);

    char *uva_body = NULL;
    int is_incremental = 0;
    char conv_key[65];
    if (resolve_thread(effective_body, parsed, &resolved_key,
                        model_buf, &uva_body, &is_incremental,
                        conv_key) != 0) {
        json_object_put(parsed);
        free(effective_body);
        response_send_error(req->client_fd, 500,
            "Failed to translate request");
        return;
    }

    json_object_put(parsed);
    free(effective_body);

    char cid[64];
    snprintf(cid, sizeof(cid), "chatcmpl-%lx%04x",
             (unsigned long)time(NULL), rand() & 0xFFFF);

    chat_stream_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.client_fd = req->client_fd;
    ctx.model = model_buf;
    ctx.completion_id = cid;
    ctx.streaming = streaming;
    buffer_init(&ctx.accum);
    ctx.parser = stream_parser_new(on_token, &ctx);
    ctx.think = streaming ? think_parser_new(on_think_emit, &ctx) : NULL;

    if (!ctx.parser) {
        free(uva_body);
        response_send_error(req->client_fd, 500, "Parser init failed");
        return;
    }

    fprintf(stderr, "  [chat] effective model: %s\n", model_buf);

    if (streaming)
        stream_emit_headers(req->client_fd);

    buffer_t resp_buf;
    int http_code = upstream_chat_with_cookie(uva_body, strlen(uva_body),
        resolved_key.user_session, upstream_stream_cb, &ctx, &resp_buf);
    free(uva_body);

    stream_parser_finish(ctx.parser);
    stream_parser_free(ctx.parser);
    if (ctx.think) {
        think_parser_finish(ctx.think);
        think_parser_free(ctx.think);
    }

    fprintf(stderr, "  [chat] upstream HTTP %d (%d chars)\n",
            http_code, ctx.completion_chars);

    if (is_incremental && conv_key[0]) {
        if (http_code < 0 || http_code >= 400) {
            fprintf(stderr, "  [chat] incremental failed (HTTP %d), "
                    "clearing thread state\n", http_code);
            thread_state_delete(conv_key);
        } else if (http_code >= 200 && http_code < 300) {
            thread_state_incr(conv_key);
        }
    }

    if (http_code < 0 || http_code >= 500) {
        if (!streaming) {
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "Upstream error (HTTP %d)", http_code);
            response_send_error(req->client_fd, 502, msg);
        }
        buffer_free(&ctx.accum);
        return;
    }

    if (streaming) {
        stream_emit_chunk(req->client_fd, model_buf, cid, NULL, 0);
        stream_emit_done(req->client_fd);
    } else {
        char *reasoning = NULL;
        char *clean = think_strip(
            ctx.accum.data ? ctx.accum.data : "", &reasoning);
        char *json_resp = translate_response_ex(clean, reasoning,
                                                 model_buf, cid);
        response_send_json(req->client_fd, 200, json_resp);
        free(json_resp);
        free(clean);
        free(reasoning);
    }

    if (http_code >= 0 && http_code < 500) {
        int est_prompt = (int)(req->body_len / 4);
        int est_completion = ctx.completion_chars / 4;
        db_log_request(resolved_key.id, resolved_key.user_id,
                       model_buf, est_prompt, est_completion, http_code);
    }

    buffer_free(&ctx.accum);
}
