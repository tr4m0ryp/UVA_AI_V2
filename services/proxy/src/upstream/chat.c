#include "server.h"
#include "upstream.h"
#include "translator.h"
#include "stream.h"
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

/* chat_tools.c */
void handle_tool_path(int client_fd,
                      struct json_object *parsed,
                      struct json_object *tools_arr,
                      char *effective_body,
                      int has_apikey,
                      const db_api_key_t *resolved_key,
                      const char *model_buf,
                      const char *cid,
                      int streaming);

/*
 * Streaming context: receives upstream data, parses SSE tokens,
 * and forwards them as OpenAI-format SSE to the client fd.
 */
typedef struct {
    int              client_fd;
    const char      *model;
    char            *completion_id;
    int              chunk_index;
    stream_parser_t *parser;
    buffer_t         accum; /* for non-streaming: accumulate full response */
    int              streaming;
    size_t           output_chars; /* accumulates output byte count */
} chat_stream_ctx_t;

static void on_token(const char *token, void *userdata)
{
    chat_stream_ctx_t *ctx = (chat_stream_ctx_t *)userdata;
    ctx->output_chars += strlen(token);

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

static size_t upstream_stream_cb(const char *data, size_t len, void *userdata)
{
    chat_stream_ctx_t *ctx = (chat_stream_ctx_t *)userdata;
    stream_parser_feed(ctx->parser, data, len);
    return len;
}

/* chat_tools.c */
char *apply_apikey_overrides(const char *body, const db_api_key_t *ak,
                              char *model_buf, size_t model_size);

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

    /* Check for API key in Authorization header */
    db_api_key_t resolved_key;
    int has_apikey = 0;
    const char *auth = request_get_header(req, "Authorization");
    if (auth) {
        const char *key = apikey_extract(auth);
        if (key) {
            if (apikey_resolve(key, &resolved_key) == 0) {
                has_apikey = 1;
                fprintf(stderr, "  Using API key: %s (model: %s)\n",
                        resolved_key.key_prefix, resolved_key.model);
            } else {
                response_send_error(req->client_fd, 401,
                    "Invalid or inactive API key");
                return;
            }
        }
    }

    /* Prepare request body: apply API key overrides if present */
    char *effective_body;
    char model_buf[DB_MAX_MODEL];
    if (has_apikey) {
        effective_body = apply_apikey_overrides(req->body, &resolved_key,
                                                model_buf, sizeof(model_buf));
        if (!effective_body) {
            response_send_error(req->client_fd, 400, "Invalid JSON");
            return;
        }
    } else {
        effective_body = strdup(req->body);
        snprintf(model_buf, sizeof(model_buf), "unknown");
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

    if (!has_apikey) {
        struct json_object *model_obj;
        if (json_object_object_get_ex(parsed, "model", &model_obj))
            snprintf(model_buf, sizeof(model_buf), "%s",
                     json_object_get_string(model_obj));
    }

    /* Generate completion ID (used by both tool and normal paths) */
    char cid[64];
    snprintf(cid, sizeof(cid), "chatcmpl-%lx%04x",
             (unsigned long)time(NULL), rand() & 0xFFFF);

    /* --- Tool call path: delegate to chat_tools.c ------------------------ */
    struct json_object *tools_arr;
    int has_tools = json_object_object_get_ex(parsed, "tools", &tools_arr)
                    && json_object_array_length(tools_arr) > 0;

    if (has_tools) {
        /* parsed, effective_body ownership transferred to handle_tool_path */
        handle_tool_path(req->client_fd, parsed, tools_arr,
                         effective_body, has_apikey, &resolved_key,
                         model_buf, cid, streaming);
        return;
    }
    /* --- End tool call path ----------------------------------------------- */

    json_object_put(parsed);

    /* Get or create a thread ID */
    char *thread_id = actions_get_or_create_thread();
    if (!thread_id) {
        free(effective_body);
        response_send_error(req->client_fd, 502,
            "Failed to obtain chat thread from UvA");
        return;
    }

    /* Capture input size before freeing effective_body */
    size_t input_chars = has_apikey ? strlen(effective_body) : 0;

    /* Translate to UvA format */
    char *uva_body = translate_request(effective_body, thread_id);
    free(thread_id);
    free(effective_body);

    if (!uva_body) {
        response_send_error(req->client_fd, 500,
            "Failed to translate request");
        return;
    }

    /* Set up streaming context */
    chat_stream_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.client_fd = req->client_fd;
    ctx.model = model_buf;
    ctx.completion_id = cid;
    ctx.chunk_index = 0;
    ctx.streaming = streaming;
    buffer_init(&ctx.accum);
    ctx.parser = stream_parser_new(on_token, &ctx);

    if (!ctx.parser) {
        free(uva_body);
        response_send_error(req->client_fd, 500, "Parser init failed");
        return;
    }

    if (streaming)
        response_start_sse(req->client_fd);

    /* Send to upstream: use user's cookie if API key, else global */
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    buffer_t resp_buf;
    int http_code;
    if (has_apikey) {
        http_code = upstream_chat_with_cookie(uva_body, strlen(uva_body),
            resolved_key.user_session, upstream_stream_cb, &ctx, &resp_buf);
    } else {
        http_code = upstream_chat(uva_body, strlen(uva_body),
                                  upstream_stream_cb, &ctx, &resp_buf);
    }
    free(uva_body);

    /* Flush parser */
    stream_parser_finish(ctx.parser);
    stream_parser_free(ctx.parser);

    clock_gettime(CLOCK_MONOTONIC, &t1);
    long latency_ms = (t1.tv_sec - t0.tv_sec) * 1000
                    + (t1.tv_nsec - t0.tv_nsec) / 1000000;

    if (has_apikey)
        db_log_request(resolved_key.id, model_buf, input_chars,
            ctx.output_chars, http_code >= 200 && http_code < 500, latency_ms);

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
        char *stop = translate_stream_chunk(NULL, model_buf, cid, 0);
        if (stop) {
            response_send_sse_data(req->client_fd, stop);
            free(stop);
        }
        response_end_sse(req->client_fd);
    } else {
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
