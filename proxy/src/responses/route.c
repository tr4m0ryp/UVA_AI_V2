#include "responses.h"
#include "responses_internal.h"
#include "server.h"
#include "upstream.h"
#include "translator.h"
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
void response_end_sse(int fd);

/* actions.c */
char *actions_get_or_create_thread(void);

static const char *RETRY_FORCE_MSG =
    "Your previous response did not contain any <function_call> "
    "blocks. You MUST respond ONLY with <function_call> blocks. "
    "Convert your intended actions into actual tool calls now. "
    "Output ONLY <function_call name=\"...\">...</function_call> "
    "blocks. No other text, no explanations, no markdown.";

/*
 * Buffer a tool-bearing request through upstream and parse for tool calls.
 * If the model returns 0 tool calls, retry once with a forcing prompt.
 * Modifies messages on retry (appends assistant + forcing messages).
 * Returns handler rc. Result is populated on success.
 */
static int tool_request_with_retry(struct json_object *messages,
                                    const char *model, const char *cookie,
                                    const resp_request_t *rr,
                                    resp_result_t *result)
{
    char *body = resp_build_openai_body(messages, model, rr);
    char *tid = actions_get_or_create_thread();
    char *uva = translate_request(body, tid);
    free(body);
    free(tid);
    if (!uva) return -1;

    int rc = resp_handle_tool_buffered(uva, cookie, model, result);
    free(uva);
    if (rc != 0) return rc;

    /* If tool calls found or empty response, no retry needed */
    if (result->tool_call_count > 0) return 0;
    if (!result->full_text || !result->full_text[0]) return 0;

    /* No tool calls detected -- retry with forcing prompt */
    fprintf(stderr, "  [responses] 0 tool calls detected, retrying "
            "with forcing prompt (model: %s)\n", model);

    struct json_object *asst = json_object_new_object();
    json_object_object_add(asst, "role",
        json_object_new_string("assistant"));
    json_object_object_add(asst, "content",
        json_object_new_string(result->full_text));
    json_object_array_add(messages, asst);

    struct json_object *force = json_object_new_object();
    json_object_object_add(force, "role",
        json_object_new_string("user"));
    json_object_object_add(force, "content",
        json_object_new_string(RETRY_FORCE_MSG));
    json_object_array_add(messages, force);

    free(result->full_text);
    result->full_text = NULL;
    result->tool_call_count = 0;

    body = resp_build_openai_body(messages, model, rr);
    tid = actions_get_or_create_thread();
    uva = translate_request(body, tid);
    free(body);
    free(tid);
    if (!uva) return -1;

    rc = resp_handle_tool_buffered(uva, cookie, model, result);
    free(uva);
    return rc;
}

/*
 * Emit all SSE events for a completed tool result.
 * Handles both tool-call and text-fallback cases.
 */
static void emit_tool_result_sse(int fd, resp_result_t *result,
                                  const char *model)
{
    resp_emit_created(fd, result, model);

    if (result->tool_call_count > 0) {
        for (int i = 0; i < result->tool_call_count; i++) {
            resp_emit_func_call_added(fd, result, i, model);
            resp_emit_func_call_args_delta(fd, result, i);
            resp_emit_func_call_args_done(fd, result, i);
            resp_emit_func_call_item_done(fd, result, i, model);
        }
    } else {
        resp_emit_output_item_added(fd, result, model);
        resp_emit_content_part_added(fd, result);
        resp_emit_text_done(fd, result);
        resp_emit_content_part_done(fd, result);
        resp_emit_output_item_done(fd, result, model);
    }

    resp_emit_completed(fd, result, model);
}

void handle_responses(http_request_t *req)
{
    if (strcmp(req->method, "POST") != 0) {
        response_send_error(req->client_fd, 405, "Method not allowed");
        return;
    }

    if (!req->body || req->body_len == 0) {
        response_send_error(req->client_fd, 400, "Request body required");
        return;
    }

    /* Authenticate via API key */
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

    fprintf(stderr, "  [responses] API key: %s (model: %s)\n",
            resolved_key.key_prefix, resolved_key.model);

    /* Parse request body */
    struct json_object *parsed = json_tokener_parse(req->body);
    if (!parsed) {
        response_send_error(req->client_fd, 400, "Invalid JSON");
        return;
    }

    resp_request_t rr;
    resp_parse_request(parsed, &rr);

    fprintf(stderr, "  [responses] model=%s stream=%d tools=%d prev=%s\n",
            rr.model, rr.stream, rr.has_tools, rr.previous_response_id);

    /* Use API key model override if request model is empty */
    if (!rr.model[0])
        snprintf(rr.model, sizeof(rr.model), "%s", resolved_key.model);

    /* Initialize result with generated IDs */
    resp_result_t result;
    memset(&result, 0, sizeof(result));
    srand((unsigned)time(NULL) ^ (unsigned)(uintptr_t)req);
    resp_gen_id(result.response_id, "resp_", 16);
    resp_gen_id(result.msg_id, "msg_", 12);

    /* Load prior conversation if chaining */
    struct json_object *prior_messages = NULL;
    if (rr.previous_response_id[0]) {
        char *prior_json = resp_state_load(rr.previous_response_id);
        if (prior_json) {
            prior_messages = json_tokener_parse(prior_json);
            free(prior_json);
            resp_state_delete(rr.previous_response_id);
        }
    }

    /* Normalize input to messages array */
    struct json_object *messages = resp_input_to_messages(
        rr.input_json, rr.instructions);

    /* Fold tool results if present */
    resp_fold_tool_results(messages);

    /* Prepend prior messages if chaining */
    if (prior_messages &&
        json_object_is_type(prior_messages, json_type_array)) {
        struct json_object *combined = json_object_new_array();
        int plen = (int)json_object_array_length(prior_messages);
        for (int i = 0; i < plen; i++)
            json_object_array_add(combined,
                json_object_get(json_object_array_get_idx(
                    prior_messages, (size_t)i)));
        int mlen = (int)json_object_array_length(messages);
        for (int i = 0; i < mlen; i++)
            json_object_array_add(combined,
                json_object_get(json_object_array_get_idx(
                    messages, (size_t)i)));
        json_object_put(messages);
        messages = combined;
    }

    /* Inject tool XML as system message if tools present */
    if (rr.has_tools) {
        char *tool_xml = resp_tools_to_xml(rr.tools_json, rr.model);
        if (tool_xml) {
            struct json_object *sys = json_object_new_object();
            json_object_object_add(sys, "role",
                json_object_new_string("system"));
            json_object_object_add(sys, "content",
                json_object_new_string(tool_xml));

            int len = (int)json_object_array_length(messages);
            struct json_object *new_msgs = json_object_new_array();
            json_object_array_add(new_msgs, sys);
            for (int i = 0; i < len; i++)
                json_object_array_add(new_msgs,
                    json_object_get(json_object_array_get_idx(
                        messages, (size_t)i)));
            json_object_put(messages);
            messages = new_msgs;
            free(tool_xml);
        }
    }

    int rc;

    if (rr.has_tools) {
        /* Tool path: buffer + parse with retry on failure.
         * SSE events are emitted only after the final result. */
        rc = tool_request_with_retry(messages, rr.model,
            resolved_key.user_session, &rr, &result);

        if (rc != 0) {
            if (!rr.stream)
                response_send_error(req->client_fd, 502, "Upstream error");
            goto cleanup;
        }

        if (rr.stream) {
            response_start_sse(req->client_fd);
            emit_tool_result_sse(req->client_fd, &result, rr.model);
            response_end_sse(req->client_fd);
        } else {
            char *resp_json = resp_build_nonstream_response(&result,
                                                             rr.model);
            if (resp_json) {
                response_send_json(req->client_fd, 200, resp_json);
                free(resp_json);
            } else {
                response_send_error(req->client_fd, 500,
                    "Failed to build response");
            }
        }
    } else {
        /* Non-tool path: build body, translate, dispatch directly */
        char *openai_body = resp_build_openai_body(messages, rr.model, &rr);
        char *thread_id = actions_get_or_create_thread();
        char *uva_body = translate_request(openai_body, thread_id);
        free(openai_body);
        free(thread_id);

        if (!uva_body) {
            json_object_put(messages);
            if (prior_messages) json_object_put(prior_messages);
            json_object_put(parsed);
            response_send_error(req->client_fd, 500,
                "Failed to translate request");
            return;
        }

        if (rr.stream)
            response_start_sse(req->client_fd);

        if (rr.stream)
            rc = resp_handle_text_stream(req->client_fd, uva_body,
                resolved_key.user_session, rr.model, &result);
        else
            rc = resp_handle_text_buffered(uva_body,
                resolved_key.user_session, rr.model, &result);
        free(uva_body);

        if (rc != 0) {
            if (!rr.stream)
                response_send_error(req->client_fd, 502, "Upstream error");
            goto cleanup;
        }

        if (rr.stream) {
            resp_emit_completed(req->client_fd, &result, rr.model);
            response_end_sse(req->client_fd);
        } else {
            char *resp_json = resp_build_nonstream_response(&result,
                                                             rr.model);
            if (resp_json) {
                response_send_json(req->client_fd, 200, resp_json);
                free(resp_json);
            } else {
                response_send_error(req->client_fd, 500,
                    "Failed to build response");
            }
        }
    }

    /* Append assistant's response to messages before saving state */
    if (result.tool_call_count > 0) {
        buffer_t tc_buf;
        buffer_init(&tc_buf);
        for (int i = 0; i < result.tool_call_count; i++) {
            char tc_line[RESP_MAX_ARGS + 256];
            snprintf(tc_line, sizeof(tc_line),
                "[Called tool \"%s\" (call_id: %s) with arguments: %s]\n",
                result.tool_calls[i].name,
                result.tool_calls[i].call_id,
                result.tool_calls[i].arguments);
            buffer_append(&tc_buf, tc_line, strlen(tc_line));
        }
        struct json_object *asst = json_object_new_object();
        json_object_object_add(asst, "role",
            json_object_new_string("assistant"));
        json_object_object_add(asst, "content",
            json_object_new_string(tc_buf.data ? tc_buf.data : ""));
        json_object_array_add(messages, asst);
        buffer_free(&tc_buf);
    } else if (result.full_text && result.full_text[0]) {
        struct json_object *asst = json_object_new_object();
        json_object_object_add(asst, "role",
            json_object_new_string("assistant"));
        json_object_object_add(asst, "content",
            json_object_new_string(result.full_text));
        json_object_array_add(messages, asst);
    }

    /* Save state for potential chaining */
    const char *msgs_str = json_object_to_json_string_ext(messages,
        JSON_C_TO_STRING_PLAIN);
    resp_state_save(result.response_id, msgs_str);

cleanup:
    free(result.full_text);
    json_object_put(messages);
    if (prior_messages) json_object_put(prior_messages);
    json_object_put(parsed);
}
