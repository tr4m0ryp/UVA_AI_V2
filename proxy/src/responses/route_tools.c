#include "responses/responses.h"
#include "responses/responses_internal.h"
#include "translator/translator.h"
#include "utils/thread_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>

/* actions.c */
char *actions_get_or_create_thread(void);

static const char *CONTINUATION_CHECK_MSG =
    "Your previous response was text only -- no tool was called and "
    "no action was taken. The task requires tool use to make progress."
    "\n\nRespond with a <tool_call> block. Example:\n"
    "<tool_call>\n"
    "{\"name\": \"exec_command\", \"arguments\": "
    "{\"command\": [\"bash\", \"-lc\", \"ls\"]}}\n"
    "</tool_call>\n\n"
    "Respond ONLY with <tool_call> blocks. No other text.";

/*
 * Parse tool calls from buffered text, populate result. Returns count.
 * Tries XML tags first, then falls back to loose JSON extraction.
 */
int resp_try_parse_tools(resp_result_t *result)
{
    if (!result->full_text || !result->full_text[0])
        return 0;
    resp_tool_call_t calls[RESP_MAX_TOOL_CALLS];

    /* Primary: XML tag parsing */
    int n = resp_parse_tool_calls(result->full_text, calls,
                                   RESP_MAX_TOOL_CALLS);
    if (n > 0) {
        result->tool_call_count = n;
        memcpy(result->tool_calls, calls,
               (size_t)n * sizeof(resp_tool_call_t));
        char *stripped = resp_strip_tool_calls(result->full_text);
        free(result->full_text);
        result->full_text = stripped ? stripped : strdup("");
        return n;
    }

    /* Fallback: loose JSON extraction */
    n = resp_extract_loose_json(result->full_text, calls,
                                 RESP_MAX_TOOL_CALLS);
    if (n > 0) {
        result->tool_call_count = n;
        memcpy(result->tool_calls, calls,
               (size_t)n * sizeof(resp_tool_call_t));
        free(result->full_text);
        result->full_text = strdup("");
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
 * Continuation check: when model returned text without tool calls,
 * re-prompt with a nudge + tool reminder to elicit tool use.
 *
 * Returns 1 if tool calls were found on retry, 0 if confirmed complete.
 * On success, result->tool_calls and result->full_text are updated.
 */
int resp_continuation_check(struct json_object *messages,
                             const resp_request_t *rr,
                             const char *stored_tid,
                             char *used_tid, size_t tid_sz,
                             const char *cookie,
                             resp_result_t *result)
{
    fprintf(stderr, "  [responses] continuation check: "
            "no tool calls, verifying task completion\n");

    char *original_text = result->full_text;
    result->full_text = NULL;

    /* Append model's text as assistant message */
    struct json_object *asst = json_object_new_object();
    json_object_object_add(asst, "role",
        json_object_new_string("assistant"));
    json_object_object_add(asst, "content",
        json_object_new_string(original_text ? original_text : ""));
    json_object_array_add(messages, asst);

    /* Append nudge as user message */
    struct json_object *nudge = json_object_new_object();
    json_object_object_add(nudge, "role",
        json_object_new_string("user"));
    json_object_object_add(nudge, "content",
        json_object_new_string(CONTINUATION_CHECK_MSG));
    json_object_array_add(messages, nudge);

    /* Re-inject compact tool reminder as system message */
    if (rr->tools_json) {
        char *reminder = resp_build_tool_reminder(rr->tools_json);
        if (reminder) {
            struct json_object *sys = json_object_new_object();
            json_object_object_add(sys, "role",
                json_object_new_string("system"));
            json_object_object_add(sys, "content",
                json_object_new_string(reminder));
            json_object_array_add(messages, sys);
            free(reminder);
        }
    }

    result->tool_call_count = 0;

    char *uva_body = resp_build_and_translate(messages, rr->model, rr,
        stored_tid, used_tid, tid_sz);
    if (uva_body) {
        int rc = resp_handle_text_buffered(uva_body, cookie,
            rr->model, result);
        free(uva_body);
        if (rc == 0) resp_try_parse_tools(result);
    }

    if (result->tool_call_count == 0) {
        /* Model confirmed task is done; use original text */
        fprintf(stderr, "  [responses] continuation check: "
                "task confirmed complete\n");
        free(result->full_text);
        result->full_text = original_text;
        return 0;
    }

    /* Model continued with tool calls */
    fprintf(stderr, "  [responses] continuation check: "
            "model continued with %d tool call(s)\n",
            result->tool_call_count);
    free(original_text);
    return 1;
}
