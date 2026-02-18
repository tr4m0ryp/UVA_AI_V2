/*
 * chat_tools.c - Tool call handling path for handle_chat_completions().
 * Called when the client request includes a "tools" array.
 */
#include "server.h"
#include "upstream.h"
#include "translator.h"
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

/*
 * Build a modified request body with API key overrides applied.
 * Injects system prompt and overrides model/parameters.
 * Caller frees the returned string.
 */
char *apply_apikey_overrides(const char *body, const db_api_key_t *ak,
                              char *model_buf, size_t model_size)
{
    struct json_object *parsed = json_tokener_parse(body);
    if (!parsed) return NULL;

    snprintf(model_buf, model_size, "%s", ak->model);
    json_object_object_del(parsed, "model");
    json_object_object_add(parsed, "model",
        json_object_new_string(ak->model));

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
            int mlen = (int)json_object_array_length(msgs);
            for (int i = mlen - 1; i > 0; i--)
                json_object_array_put_idx(msgs, (size_t)i,
                    json_object_get(json_object_array_get_idx(msgs,
                        (size_t)(i - 1))));
            json_object_array_put_idx(msgs, 0, sys_msg);
        }
    }

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

/* actions.c */
char *actions_get_or_create_thread(void);

/* Accumulate raw SSE bytes into a buffer */
static size_t tool_accum_cb(const char *data, size_t len, void *userdata)
{
    buffer_t *buf = (buffer_t *)userdata;
    buffer_append(buf, data, len);
    return len;
}

/*
 * Check if tool_name is in a CSV allowlist.
 * Returns 1 if allowed (allowlist empty = all allowed, or name in list).
 */
int tool_in_allowlist(const char *tool_name, const char *allowlist)
{
    if (!allowlist || allowlist[0] == '\0') return 1;
    const char *p = allowlist;
    size_t nlen = strlen(tool_name);
    while (*p) {
        const char *comma = strchr(p, ',');
        size_t seg_len = comma ? (size_t)(comma - p) : strlen(p);
        if (seg_len == nlen && memcmp(p, tool_name, nlen) == 0)
            return 1;
        p += seg_len;
        if (*p == ',') p++;
    }
    return 0;
}

/*
 * handle_tool_path - Execute the full tool-call translation path.
 *
 * Parameters:
 *   client_fd    - response socket
 *   parsed       - parsed OpenAI request JSON (will be put/freed here)
 *   tools_arr    - the "tools" array from parsed (borrowed, do not free)
 *   effective_body - serialised effective_body (freed here)
 *   has_apikey   - 1 if an API key was resolved
 *   resolved_key - the resolved key (only valid if has_apikey)
 *   model_buf    - model name string
 *   cid          - completion ID string
 *   streaming    - 1 if client wants SSE
 */
void handle_tool_path(int client_fd,
                      struct json_object *parsed,
                      struct json_object *tools_arr,
                      char *effective_body,
                      int has_apikey,
                      const db_api_key_t *resolved_key,
                      const char *model_buf,
                      const char *cid,
                      int streaming)
{
    /* Permission check */
    if (has_apikey && !resolved_key->allow_tools) {
        json_object_put(parsed);
        free(effective_body);
        response_send_error(client_fd, 403,
            "Tool use not permitted for this API key");
        return;
    }

    /* Per-tool allowlist check */
    if (has_apikey && resolved_key->tool_allowlist[0] != '\0') {
        int n_tools = (int)json_object_array_length(tools_arr);
        for (int ti = 0; ti < n_tools; ti++) {
            struct json_object *t = json_object_array_get_idx(tools_arr,
                                                               (size_t)ti);
            struct json_object *fn_o;
            const char *tname = NULL;
            if (json_object_object_get_ex(t, "function", &fn_o)) {
                struct json_object *nm;
                if (json_object_object_get_ex(fn_o, "name", &nm))
                    tname = json_object_get_string(nm);
            }
            if (tname && !tool_in_allowlist(tname, resolved_key->tool_allowlist)) {
                json_object_put(parsed);
                free(effective_body);
                response_send_error(client_fd, 403,
                    "Tool not permitted for this API key");
                return;
            }
        }
    }

    /* 1. Inject tool system prompt */
    char *sysprompt = tool_definitions_to_sysprompt(tools_arr);
    struct json_object *req_with_tools = inject_tool_sysprompt(parsed, sysprompt);
    free(sysprompt);
    json_object_put(parsed);

    if (!req_with_tools) {
        free(effective_body);
        response_send_error(client_fd, 500, "Tool injection failed");
        return;
    }

    /* 2. Fold tool_result messages -> plain user messages */
    struct json_object *msgs_obj;
    if (json_object_object_get_ex(req_with_tools, "messages", &msgs_obj)) {
        struct json_object *folded = fold_tool_results(msgs_obj);
        json_object_object_del(req_with_tools, "messages");
        json_object_object_add(req_with_tools, "messages", folded);
    }

    /* 3. Translate to UvA format */
    const char *req_str = json_object_to_json_string_ext(req_with_tools,
                                                           JSON_C_TO_STRING_PLAIN);
    char *thread_id = actions_get_or_create_thread();
    size_t input_chars = has_apikey ? strlen(req_str) : 0;
    char *uva_body = translate_request(req_str,
        thread_id ? thread_id :
        "00000000-0000-4000-8000-000000000000");
    free(thread_id);
    json_object_put(req_with_tools);
    free(effective_body);

    if (!uva_body) {
        response_send_error(client_fd, 500, "Translation failed");
        return;
    }

    /* 4. Send to UvA and accumulate raw SSE */
    buffer_t raw_buf;
    buffer_init(&raw_buf);
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    int http_code;
    if (has_apikey) {
        http_code = upstream_chat_with_cookie(uva_body, strlen(uva_body),
            resolved_key->user_session, tool_accum_cb, &raw_buf, NULL);
    } else {
        http_code = upstream_chat(uva_body, strlen(uva_body),
                                   tool_accum_cb, &raw_buf, NULL);
    }
    free(uva_body);

    clock_gettime(CLOCK_MONOTONIC, &t1);
    long latency_ms = (t1.tv_sec - t0.tv_sec) * 1000
                    + (t1.tv_nsec - t0.tv_nsec) / 1000000;

    if (http_code < 0 || http_code >= 500) {
        if (has_apikey)
            db_log_request(resolved_key->id, model_buf,
                           input_chars, 0, 0, latency_ms);
        buffer_free(&raw_buf);
        response_send_error(client_fd, 502, "Upstream error");
        return;
    }

    /* 5. Extract plain text */
    char *response_text = extract_text_from_sse(raw_buf.data, raw_buf.size);
    size_t output_chars = response_text ? strlen(response_text) : 0;
    buffer_free(&raw_buf);

    if (has_apikey)
        db_log_request(resolved_key->id, model_buf,
                       input_chars, output_chars, 1, latency_ms);

    /* 6. Parse tool calls */
    parsed_tool_call_t calls[MAX_TOOL_CALLS];
    int n_calls = parse_tool_calls_from_text(
        response_text ? response_text : "", calls, MAX_TOOL_CALLS);

    if (n_calls > 0) {
        /* 7a. Return tool_calls response */
        if (streaming) {
            response_start_sse(client_fd);
            emit_tool_calls_sse(client_fd, calls, n_calls, model_buf, cid);
            response_end_sse(client_fd);
        } else {
            char *resp = build_tool_calls_response(calls, n_calls,
                                                    model_buf, cid);
            if (resp) {
                response_send_json(client_fd, 200, resp);
                free(resp);
            } else {
                response_send_error(client_fd, 500,
                    "Failed to build tool_calls response");
            }
        }
    } else {
        /* 7b. Final answer - no tool calls */
        if (streaming) {
            response_start_sse(client_fd);
            char *chunk = translate_stream_chunk(
                response_text ? response_text : "", model_buf, cid, 0);
            if (chunk) {
                response_send_sse_data(client_fd, chunk);
                free(chunk);
            }
            char *stop = translate_stream_chunk(NULL, model_buf, cid, 0);
            if (stop) {
                response_send_sse_data(client_fd, stop);
                free(stop);
            }
            response_end_sse(client_fd);
        } else {
            char *resp = translate_response(
                response_text ? response_text : "", model_buf, cid);
            if (resp) {
                response_send_json(client_fd, 200, resp);
                free(resp);
            } else {
                response_send_error(client_fd, 500,
                    "Failed to build response");
            }
        }
    }
    free(response_text);
}
