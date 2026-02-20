#include "server.h"
#include "upstream.h"
#include "translator.h"
#include "think_parser.h"
#include "stream.h"
#include "platform.h"
#include "apikey.h"
#include "database.h"
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
 * Streaming context: receives upstream data, parses SSE tokens,
 * and forwards them as OpenAI-format SSE to the client fd.
 */
typedef struct {
    int              client_fd;
    const char      *model;
    char            *completion_id;
    stream_parser_t *parser;
    think_parser_t  *think;
    buffer_t         accum; /* for non-streaming: accumulate full response */
    int              streaming;
    int              completion_chars;
} chat_stream_ctx_t;

/* Called by think_parser when it has classified a piece of text. */
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

/* Build a modified request body with API key overrides applied.
 * Injects system prompt and overrides model if API key provides them. */
static char *apply_apikey_overrides(const char *body,
                                    const db_api_key_t *ak,
                                    char *model_buf, size_t model_size)
{
    struct json_object *parsed = json_tokener_parse(body);
    if (!parsed) return NULL;

    /* Use client's model if present, fall back to key's model */
    struct json_object *existing_model;
    if (json_object_object_get_ex(parsed, "model", &existing_model)) {
        snprintf(model_buf, model_size, "%s",
            json_object_get_string(existing_model));
    } else {
        snprintf(model_buf, model_size, "%s", ak->model);
        json_object_object_add(parsed, "model",
            json_object_new_string(ak->model));
    }

    /* Inject system prompt at front of messages */
    if (ak->system_prompt[0]) {
        struct json_object *msgs;
        if (json_object_object_get_ex(parsed, "messages", &msgs)) {
            struct json_object *sys_msg = json_object_new_object();
            json_object_object_add(sys_msg, "role",
                json_object_new_string("system"));
            json_object_object_add(sys_msg, "content",
                json_object_new_string(ak->system_prompt));
            json_object_array_put_idx(msgs,
                (size_t)json_object_array_length(msgs), NULL);
            /* Shift all elements right by 1 */
            int len = (int)json_object_array_length(msgs);
            for (int i = len - 1; i > 0; i--)
                json_object_array_put_idx(msgs, (size_t)i,
                    json_object_get(json_object_array_get_idx(msgs,
                        (size_t)(i - 1))));
            json_object_array_put_idx(msgs, 0, sys_msg);
        }
    }

    /* Pass through parameters for translate_request to pick up */
    if (ak->temperature >= 0)
        json_object_object_add(parsed, "temperature",
            json_object_new_double(ak->temperature));
    if (ak->top_p >= 0)
        json_object_object_add(parsed, "top_p",
            json_object_new_double(ak->top_p));
    if (ak->max_tokens > 0)
        json_object_object_add(parsed, "max_tokens",
            json_object_new_int(ak->max_tokens));
    if (ak->frequency_penalty != 0.0)
        json_object_object_add(parsed, "frequency_penalty",
            json_object_new_double(ak->frequency_penalty));
    if (ak->presence_penalty != 0.0)
        json_object_object_add(parsed, "presence_penalty",
            json_object_new_double(ak->presence_penalty));
    if (ak->reasoning_effort[0])
        json_object_object_add(parsed, "reasoning_effort",
            json_object_new_string(ak->reasoning_effort));

    const char *str = json_object_to_json_string_ext(parsed,
        JSON_C_TO_STRING_PLAIN);
    char *result = strdup(str);
    json_object_put(parsed);
    return result;
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

    /* Authenticate via API key (mandatory) */
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

    /* Prepare request body: apply API key overrides */
    char model_buf[DB_MAX_MODEL];
    char *effective_body = apply_apikey_overrides(req->body, &resolved_key,
                                                  model_buf, sizeof(model_buf));
    if (!effective_body) {
        response_send_error(req->client_fd, 400, "Invalid JSON");
        return;
    }

    /* Parse to check for stream flag and model */
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

    json_object_put(parsed);

    /* Get or create a thread ID */
    char *thread_id = actions_get_or_create_thread();
    if (!thread_id) {
        free(effective_body);
        response_send_error(req->client_fd, 502,
            "Failed to obtain chat thread from UvA");
        return;
    }

    /* Translate to UvA format */
    char *uva_body = translate_request(effective_body, thread_id);
    free(thread_id);
    free(effective_body);

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

    /* Send to upstream using the API key's session cookie */
    buffer_t resp_buf;
    int http_code = upstream_chat_with_cookie(uva_body, strlen(uva_body),
        resolved_key.user_session, upstream_stream_cb, &ctx, &resp_buf);
    free(uva_body);

    /* Flush parsers */
    stream_parser_finish(ctx.parser);
    stream_parser_free(ctx.parser);
    if (ctx.think) {
        think_parser_finish(ctx.think);
        think_parser_free(ctx.think);
    }

    fprintf(stderr, "  [chat] upstream HTTP %d (%d chars)\n",
            http_code, ctx.completion_chars);

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

    /* Log usage: estimate tokens from character count (~4 chars/token) */
    if (http_code >= 0 && http_code < 500) {
        int est_prompt = (int)(req->body_len / 4);
        int est_completion = ctx.completion_chars / 4;
        db_log_request(resolved_key.id, resolved_key.user_id,
                       model_buf, est_prompt, est_completion, http_code);
    }

    buffer_free(&ctx.accum);
}
