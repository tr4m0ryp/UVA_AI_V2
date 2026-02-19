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

/*
 * Parse the incoming Responses API request body.
 * Returns 0 on success and fills req_out.
 */
static int parse_request(struct json_object *parsed, resp_request_t *req)
{
    memset(req, 0, sizeof(*req));
    req->stream = 1; /* default: streaming */

    struct json_object *model_obj;
    if (json_object_object_get_ex(parsed, "model", &model_obj))
        snprintf(req->model, sizeof(req->model), "%s",
                 json_object_get_string(model_obj));

    struct json_object *inst_obj;
    if (json_object_object_get_ex(parsed, "instructions", &inst_obj))
        snprintf(req->instructions, sizeof(req->instructions), "%s",
                 json_object_get_string(inst_obj));

    struct json_object *stream_obj;
    if (json_object_object_get_ex(parsed, "stream", &stream_obj))
        req->stream = json_object_get_boolean(stream_obj);

    /* tools (optional) */
    struct json_object *tools_obj;
    if (json_object_object_get_ex(parsed, "tools", &tools_obj)) {
        if (json_object_is_type(tools_obj, json_type_array) &&
            json_object_array_length(tools_obj) > 0) {
            req->has_tools = 1;
            req->tools_json = tools_obj; /* borrowed */
        }
    }

    /* previous_response_id (optional) */
    struct json_object *prev_obj;
    if (json_object_object_get_ex(parsed, "previous_response_id", &prev_obj)) {
        const char *prev = json_object_get_string(prev_obj);
        if (prev)
            snprintf(req->previous_response_id,
                     sizeof(req->previous_response_id), "%s", prev);
    }

    /* input (required) */
    struct json_object *input_obj;
    if (json_object_object_get_ex(parsed, "input", &input_obj))
        req->input_json = input_obj; /* borrowed */

    return 0;
}

/*
 * Build an OpenAI messages-format request body from normalized messages.
 * This will be passed to translate_request() for UvA conversion.
 */
static char *build_openai_body(struct json_object *messages,
                                const char *model)
{
    struct json_object *body = json_object_new_object();
    json_object_object_add(body, "model",
        json_object_new_string(model));
    json_object_object_add(body, "messages", json_object_get(messages));
    json_object_object_add(body, "stream",
        json_object_new_boolean(1));

    const char *s = json_object_to_json_string_ext(body,
        JSON_C_TO_STRING_PLAIN);
    char *result = strdup(s);
    json_object_put(body);
    return result;
}

/*
 * Build a non-streaming JSON response for the Responses API.
 */
static char *build_nonstream_response(const resp_result_t *r,
                                       const char *model)
{
    struct json_object *obj = resp_build_skeleton(r, model, "completed");

    struct json_object *output;
    json_object_object_get_ex(obj, "output", &output);

    if (r->tool_call_count > 0) {
        for (int i = 0; i < r->tool_call_count; i++) {
            struct json_object *item = resp_build_func_call_item(
                &r->tool_calls[i], model, "completed");
            json_object_array_add(output, item);
        }
    } else {
        struct json_object *item = resp_build_message_item(r, model,
            r->full_text ? r->full_text : "", "completed");
        json_object_array_add(output, item);
    }

    const char *s = json_object_to_json_string_ext(obj,
        JSON_C_TO_STRING_PLAIN);
    char *result = strdup(s);
    json_object_put(obj);
    return result;
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
    parse_request(parsed, &rr);

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
            /* Delete old state entry */
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
        char *tool_xml = resp_tools_to_xml(rr.tools_json);
        if (tool_xml) {
            struct json_object *sys = json_object_new_object();
            json_object_object_add(sys, "role",
                json_object_new_string("system"));
            json_object_object_add(sys, "content",
                json_object_new_string(tool_xml));

            /* Prepend to messages array */
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

    /* Build OpenAI-format body and translate to UvA format */
    char *openai_body = build_openai_body(messages, rr.model);
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

    int rc;
    if (rr.stream)
        response_start_sse(req->client_fd);

    /* Dispatch to text stream or tool buffered path */
    if (rr.has_tools) {
        rc = resp_handle_tool_path(req->client_fd, uva_body,
            resolved_key.user_session, rr.model, &result);
    } else {
        rc = resp_handle_text_stream(req->client_fd, uva_body,
            resolved_key.user_session, rr.model, &result);
    }

    free(uva_body);

    if (rc != 0) {
        if (!rr.stream)
            response_send_error(req->client_fd, 502, "Upstream error");
        goto cleanup;
    }

    if (rr.stream) {
        /* Emit response.completed and end stream */
        resp_emit_completed(req->client_fd, &result, rr.model);
        response_end_sse(req->client_fd);
    } else {
        /* Non-streaming: send full JSON response */
        char *resp_json = build_nonstream_response(&result, rr.model);
        if (resp_json) {
            response_send_json(req->client_fd, 200, resp_json);
            free(resp_json);
        } else {
            response_send_error(req->client_fd, 500,
                "Failed to build response");
        }
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
