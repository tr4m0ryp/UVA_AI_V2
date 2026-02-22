#include "responses.h"
#include "responses_internal.h"
#include "server.h"
#include "upstream.h"
#include "apikey.h"
#include "database.h"
#include "thread_state.h"
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

static const char *REQUIRED_RETRY_MSG =
    "Please use one of the available tools to complete this request.";

/* route_helpers.c */
int   resp_try_parse_tools(resp_result_t *result);
char *resp_build_and_translate(struct json_object *messages,
                                const char *model,
                                const resp_request_t *rr,
                                const char *stored_tid,
                                char *used_tid, size_t tid_sz);

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

    struct json_object *parsed = json_tokener_parse(req->body);
    if (!parsed) {
        response_send_error(req->client_fd, 400, "Invalid JSON");
        return;
    }

    resp_request_t rr;
    resp_parse_request(parsed, &rr);
    fprintf(stderr, "  [responses] model=%s stream=%d tools=%d prev=%s\n",
            rr.model, rr.stream, rr.has_tools, rr.previous_response_id);
    if (!rr.model[0])
        snprintf(rr.model, sizeof(rr.model), "%s", resolved_key.model);

    resp_result_t result;
    memset(&result, 0, sizeof(result));
    srand((unsigned)time(NULL) ^ (unsigned)(uintptr_t)req);
    resp_gen_id(result.response_id, "resp_", 16);
    resp_gen_id(result.msg_id, "msg_", 12);
    struct json_object *prior_messages = NULL;
    char stored_thread_id[THREAD_STATE_MAX_THREAD] = "";
    if (rr.previous_response_id[0]) {
        char *prior_json = resp_state_load(rr.previous_response_id);
        if (prior_json) {
            prior_messages = json_tokener_parse(prior_json);
            free(prior_json);
            resp_state_delete(rr.previous_response_id);
        }
        thread_state_t ts;
        if (thread_state_find(rr.previous_response_id, &ts) == 0) {
            snprintf(stored_thread_id, sizeof(stored_thread_id),
                     "%s", ts.uva_thread_id);
            fprintf(stderr, "  [responses] found thread state: %s\n",
                    stored_thread_id);
        }
    }

    struct json_object *messages = resp_input_to_messages(
        rr.input_json, rr.instructions);
    resp_fold_tool_results(messages);
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

    int inject_tools = rr.has_tools && rr.tool_choice != RESP_TC_NONE;
    if (inject_tools) {
        char *tool_prompt = resp_build_tool_prompt(rr.tools_json);
        if (tool_prompt) {
            struct json_object *sys = json_object_new_object();
            json_object_object_add(sys, "role",
                json_object_new_string("system"));
            json_object_object_add(sys, "content",
                json_object_new_string(tool_prompt));
            json_object_array_add(messages, sys);
            free(tool_prompt);
        }
    }

    int rc;
    char used_thread_id[THREAD_STATE_MAX_THREAD] = "";
    if (inject_tools) {
        char *uva_body = resp_build_and_translate(messages, rr.model, &rr,
            stored_thread_id, used_thread_id, sizeof(used_thread_id));
        if (!uva_body) {
            json_object_put(messages);
            if (prior_messages) json_object_put(prior_messages);
            json_object_put(parsed);
            response_send_error(req->client_fd, 500,
                "Failed to translate request");
            return;
        }

        rc = resp_handle_text_buffered(uva_body,
            resolved_key.user_session, rr.model, &result);
        free(uva_body);

        if (rc != 0) {
            if (!rr.stream)
                response_send_error(req->client_fd, 502, "Upstream error");
            goto cleanup;
        }

        resp_try_parse_tools(&result);
        if (rr.tool_choice == RESP_TC_REQUIRED &&
            result.tool_call_count == 0) {
            fprintf(stderr, "  [responses] tool_choice=required, "
                    "no tool calls -- retrying\n");

            struct json_object *asst = json_object_new_object();
            json_object_object_add(asst, "role",
                json_object_new_string("assistant"));
            json_object_object_add(asst, "content",
                json_object_new_string(
                    result.full_text ? result.full_text : ""));
            json_object_array_add(messages, asst);

            struct json_object *nudge = json_object_new_object();
            json_object_object_add(nudge, "role",
                json_object_new_string("user"));
            json_object_object_add(nudge, "content",
                json_object_new_string(REQUIRED_RETRY_MSG));
            json_object_array_add(messages, nudge);

            free(result.full_text);
            result.full_text = NULL;
            result.tool_call_count = 0;

            uva_body = resp_build_and_translate(messages, rr.model, &rr,
                stored_thread_id, used_thread_id,
                sizeof(used_thread_id));
            if (uva_body) {
                rc = resp_handle_text_buffered(uva_body,
                    resolved_key.user_session, rr.model, &result);
                free(uva_body);
                if (rc == 0) resp_try_parse_tools(&result);
            }
        }
        if (rr.stream) {
            response_start_sse(req->client_fd);
            resp_emit_result_sse(req->client_fd, &result, rr.model);
            response_end_sse(req->client_fd);
        } else {
            char *rj = resp_build_nonstream_response(&result, rr.model);
            if (rj) {
                response_send_json(req->client_fd, 200, rj);
                free(rj);
            } else {
                response_send_error(req->client_fd, 500,
                    "Failed to build response");
            }
        }
    } else {
        char *uva_body = resp_build_and_translate(messages, rr.model, &rr,
            stored_thread_id, used_thread_id, sizeof(used_thread_id));
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
            char *rj = resp_build_nonstream_response(&result, rr.model);
            if (rj) {
                response_send_json(req->client_fd, 200, rj);
                free(rj);
            } else {
                response_send_error(req->client_fd, 500,
                    "Failed to build response");
            }
        }
    }

    if (result.tool_call_count > 0) {
        buffer_t tc_buf;
        buffer_init(&tc_buf);
        for (int i = 0; i < result.tool_call_count; i++) {
            char tc_line[RESP_MAX_ARGS + 256];
            snprintf(tc_line, sizeof(tc_line),
                "<tool_call>\n"
                "{\"name\": \"%s\", \"arguments\": %s}\n"
                "</tool_call>\n",
                result.tool_calls[i].name,
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

    const char *msgs_str = json_object_to_json_string_ext(messages,
        JSON_C_TO_STRING_PLAIN);
    resp_state_save(result.response_id, msgs_str);

    if (used_thread_id[0]) {
        thread_state_t ts;
        memset(&ts, 0, sizeof(ts));
        snprintf(ts.conv_key, sizeof(ts.conv_key), "%s",
                 result.response_id);
        snprintf(ts.uva_thread_id, sizeof(ts.uva_thread_id), "%s",
                 used_thread_id);
        snprintf(ts.model, sizeof(ts.model), "%s", rr.model);
        ts.message_count = (int)json_object_array_length(messages);
        thread_state_save(&ts);
    }

    if (rr.previous_response_id[0] && stored_thread_id[0])
        thread_state_delete(rr.previous_response_id);

cleanup:
    free(result.full_text);
    json_object_put(messages);
    if (prior_messages) json_object_put(prior_messages);
    json_object_put(parsed);
}
