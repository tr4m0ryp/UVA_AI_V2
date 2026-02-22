#include "responses.h"
#include "responses_internal.h"
#include "translator.h"
#include "thread_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>

/* actions.c */
char *actions_get_or_create_thread(void);

/* Parse the incoming Responses API request body.
 * Returns 0 on success and fills req. */
int resp_parse_request(struct json_object *parsed, resp_request_t *req)
{
    memset(req, 0, sizeof(*req));
    req->stream = 1;
    req->temperature = -1;
    req->top_p = -1;
    req->tool_choice = RESP_TC_AUTO;

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

    struct json_object *tools_obj;
    if (json_object_object_get_ex(parsed, "tools", &tools_obj)) {
        if (json_object_is_type(tools_obj, json_type_array) &&
            json_object_array_length(tools_obj) > 0) {
            req->has_tools = 1;
            req->tools_json = tools_obj;
        }
    }

    struct json_object *prev_obj;
    if (json_object_object_get_ex(parsed, "previous_response_id",
                                   &prev_obj)) {
        const char *prev = json_object_get_string(prev_obj);
        if (prev)
            snprintf(req->previous_response_id,
                     sizeof(req->previous_response_id), "%s", prev);
    }

    struct json_object *input_obj;
    if (json_object_object_get_ex(parsed, "input", &input_obj))
        req->input_json = input_obj;

    struct json_object *temp_obj;
    if (json_object_object_get_ex(parsed, "temperature", &temp_obj))
        req->temperature = json_object_get_double(temp_obj);

    struct json_object *maxtoken_obj;
    if (json_object_object_get_ex(parsed, "max_tokens", &maxtoken_obj))
        req->max_tokens = json_object_get_int(maxtoken_obj);
    if (!req->max_tokens &&
        json_object_object_get_ex(parsed, "max_output_tokens",
                                   &maxtoken_obj))
        req->max_tokens = json_object_get_int(maxtoken_obj);

    struct json_object *topp_obj;
    if (json_object_object_get_ex(parsed, "top_p", &topp_obj))
        req->top_p = json_object_get_double(topp_obj);

    /* Parse tool_choice: "none", "auto", "required", or object */
    struct json_object *tc_obj;
    if (json_object_object_get_ex(parsed, "tool_choice", &tc_obj)) {
        if (json_object_is_type(tc_obj, json_type_string)) {
            const char *tc = json_object_get_string(tc_obj);
            if (tc && strcmp(tc, "none") == 0)
                req->tool_choice = RESP_TC_NONE;
            else if (tc && strcmp(tc, "required") == 0)
                req->tool_choice = RESP_TC_REQUIRED;
            /* "auto" is the default */
        }
        fprintf(stderr, "  [responses] tool_choice: %s (parsed: %d)\n",
                json_object_get_string(tc_obj), req->tool_choice);
    }

    return 0;
}

/* Build an OpenAI messages-format request body from normalized messages.
 * Passed to translate_request() for UvA conversion. Caller frees. */
char *resp_build_openai_body(struct json_object *messages,
                              const char *model,
                              const resp_request_t *rr)
{
    struct json_object *body = json_object_new_object();
    json_object_object_add(body, "model",
        json_object_new_string(model));
    json_object_object_add(body, "messages", json_object_get(messages));
    json_object_object_add(body, "stream",
        json_object_new_boolean(1));

    if (rr->temperature >= 0)
        json_object_object_add(body, "temperature",
            json_object_new_double(rr->temperature));
    if (rr->max_tokens > 0)
        json_object_object_add(body, "max_tokens",
            json_object_new_int(rr->max_tokens));
    if (rr->top_p >= 0)
        json_object_object_add(body, "top_p",
            json_object_new_double(rr->top_p));

    const char *s = json_object_to_json_string_ext(body,
        JSON_C_TO_STRING_PLAIN);
    char *result = strdup(s);
    json_object_put(body);
    return result;
}

/* Build a non-streaming JSON response for the Responses API. Caller frees. */
char *resp_build_nonstream_response(const resp_result_t *r,
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

/* Parse tool calls from buffered text, populate result. Returns count. */
int resp_try_parse_tools(resp_result_t *result)
{
    if (!result->full_text || !result->full_text[0])
        return 0;
    resp_tool_call_t calls[RESP_MAX_TOOL_CALLS];
    int n = resp_parse_tool_calls(result->full_text, calls,
                                   RESP_MAX_TOOL_CALLS);
    if (n > 0) {
        result->tool_call_count = n;
        memcpy(result->tool_calls, calls,
               (size_t)n * sizeof(resp_tool_call_t));
        char *stripped = resp_strip_tool_calls(result->full_text);
        free(result->full_text);
        result->full_text = stripped ? stripped : strdup("");
    }
    return n;
}

/* Translate messages and build UvA body, trying incremental mode first. */
char *resp_build_and_translate(struct json_object *messages,
                                const char *model,
                                const resp_request_t *rr,
                                const char *stored_tid,
                                char *used_tid, size_t tid_sz)
{
    char *openai_body = resp_build_openai_body(messages, model, rr);
    char *uva_body = NULL;
    if (stored_tid && stored_tid[0]) {
        int mlen = (int)json_object_array_length(messages);
        const char *last_user = NULL;
        for (int i = mlen - 1; i >= 0; i--) {
            struct json_object *m = json_object_array_get_idx(
                messages, (size_t)i);
            struct json_object *role_o, *content_o;
            if (json_object_object_get_ex(m, "role", &role_o) &&
                strcmp(json_object_get_string(role_o), "user") == 0 &&
                json_object_object_get_ex(m, "content", &content_o)) {
                last_user = json_object_get_string(content_o);
                break;
            }
        }
        if (last_user) {
            uva_body = translate_request_incremental(
                last_user, "user", stored_tid, NULL);
            if (uva_body) {
                snprintf(used_tid, tid_sz, "%s", stored_tid);
                fprintf(stderr, "  [responses] incremental mode: "
                        "thread=%s\n", stored_tid);
            }
        }
    }
    if (!uva_body) {
        char *tid = actions_get_or_create_thread();
        uva_body = translate_request(openai_body, tid);
        snprintf(used_tid, tid_sz, "%s", tid);
        fprintf(stderr, "  [responses] full mode: thread=%s\n", tid);
        free(tid);
    }
    free(openai_body);
    return uva_body;
}

/*
 * Emit all SSE events for a completed result.
 * Handles both tool-call and text-fallback cases.
 */
void resp_emit_result_sse(int fd, resp_result_t *result,
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
