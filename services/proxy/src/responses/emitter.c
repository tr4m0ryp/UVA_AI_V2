#include "responses.h"
#include "responses_internal.h"
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Generate hex IDs: "prefix" + N hex chars */
void resp_gen_id(char *buf, const char *prefix, int hex_len)
{
    static const char hex[] = "0123456789abcdef";
    int pos = 0;
    size_t plen = strlen(prefix);
    memcpy(buf, prefix, plen);
    pos = (int)plen;
    for (int i = 0; i < hex_len; i++)
        buf[pos++] = hex[rand() & 0xf];
    buf[pos] = '\0';
}

/* Send one Responses API SSE event inside chunked transfer encoding.
 * Format: "event: TYPE\ndata: JSON\n\n" */
void resp_send_sse_event(int fd, const char *event_type,
                         const char *json_data)
{
    char line[16384];
    int len = snprintf(line, sizeof(line),
        "event: %s\ndata: %s\n\n", event_type, json_data);
    char chunk[16512];
    int clen = snprintf(chunk, sizeof(chunk), "%x\r\n%s\r\n", len, line);
    platform_send(fd, chunk, (size_t)clen);
}

struct json_object *resp_build_skeleton(const resp_result_t *r,
                                        const char *model,
                                        const char *status)
{
    struct json_object *obj = json_object_new_object();
    json_object_object_add(obj, "id",
        json_object_new_string(r->response_id));
    json_object_object_add(obj, "object",
        json_object_new_string("response"));
    json_object_object_add(obj, "created_at",
        json_object_new_int64((int64_t)time(NULL)));
    json_object_object_add(obj, "status",
        json_object_new_string(status));
    json_object_object_add(obj, "model",
        json_object_new_string(model));
    json_object_object_add(obj, "output",
        json_object_new_array());
    return obj;
}

struct json_object *resp_build_message_item(const resp_result_t *r,
                                            const char *model,
                                            const char *text,
                                            const char *status)
{
    (void)model;
    struct json_object *item = json_object_new_object();
    json_object_object_add(item, "type",
        json_object_new_string("message"));
    json_object_object_add(item, "id",
        json_object_new_string(r->msg_id));
    json_object_object_add(item, "status",
        json_object_new_string(status));
    json_object_object_add(item, "role",
        json_object_new_string("assistant"));

    struct json_object *content = json_object_new_array();
    if (text) {
        struct json_object *part = json_object_new_object();
        json_object_object_add(part, "type",
            json_object_new_string("output_text"));
        json_object_object_add(part, "text",
            json_object_new_string(text));
        json_object_array_add(content, part);
    }
    json_object_object_add(item, "content", content);
    return item;
}

struct json_object *resp_build_func_call_item(const resp_tool_call_t *tc,
                                              const char *model,
                                              const char *status)
{
    (void)model;
    struct json_object *item = json_object_new_object();
    json_object_object_add(item, "type",
        json_object_new_string("function_call"));
    json_object_object_add(item, "id",
        json_object_new_string(tc->call_id));
    json_object_object_add(item, "status",
        json_object_new_string(status));
    json_object_object_add(item, "name",
        json_object_new_string(tc->name));
    json_object_object_add(item, "call_id",
        json_object_new_string(tc->call_id));
    json_object_object_add(item, "arguments",
        json_object_new_string(tc->arguments));
    return item;
}

void resp_emit_created(int fd, const resp_result_t *r, const char *model)
{
    struct json_object *obj = resp_build_skeleton(r, model, "in_progress");
    const char *s = json_object_to_json_string_ext(obj,
        JSON_C_TO_STRING_PLAIN);
    resp_send_sse_event(fd, "response.created", s);
    json_object_put(obj);
}

void resp_emit_output_item_added(int fd, const resp_result_t *r,
                                 const char *model)
{
    struct json_object *item = resp_build_message_item(r, model,
                                                       NULL, "in_progress");
    struct json_object *ev = json_object_new_object();
    json_object_object_add(ev, "output_index", json_object_new_int(0));
    json_object_object_add(ev, "item", item);
    const char *s = json_object_to_json_string_ext(ev,
        JSON_C_TO_STRING_PLAIN);
    resp_send_sse_event(fd, "response.output_item.added", s);
    json_object_put(ev);
}

void resp_emit_content_part_added(int fd, const resp_result_t *r)
{
    (void)r;
    struct json_object *ev = json_object_new_object();
    json_object_object_add(ev, "output_index", json_object_new_int(0));
    json_object_object_add(ev, "content_index", json_object_new_int(0));
    struct json_object *part = json_object_new_object();
    json_object_object_add(part, "type",
        json_object_new_string("output_text"));
    json_object_object_add(part, "text", json_object_new_string(""));
    json_object_object_add(ev, "part", part);
    const char *s = json_object_to_json_string_ext(ev,
        JSON_C_TO_STRING_PLAIN);
    resp_send_sse_event(fd, "response.content_part.added", s);
    json_object_put(ev);
}

void resp_emit_text_delta(int fd, const resp_result_t *r,
                          const char *delta)
{
    (void)r;
    struct json_object *ev = json_object_new_object();
    json_object_object_add(ev, "output_index", json_object_new_int(0));
    json_object_object_add(ev, "content_index", json_object_new_int(0));
    json_object_object_add(ev, "delta",
        json_object_new_string(delta));
    const char *s = json_object_to_json_string_ext(ev,
        JSON_C_TO_STRING_PLAIN);
    resp_send_sse_event(fd, "response.output_text.delta", s);
    json_object_put(ev);
}

void resp_emit_text_done(int fd, const resp_result_t *r)
{
    struct json_object *ev = json_object_new_object();
    json_object_object_add(ev, "output_index", json_object_new_int(0));
    json_object_object_add(ev, "content_index", json_object_new_int(0));
    json_object_object_add(ev, "text",
        json_object_new_string(r->full_text ? r->full_text : ""));
    const char *s = json_object_to_json_string_ext(ev,
        JSON_C_TO_STRING_PLAIN);
    resp_send_sse_event(fd, "response.output_text.done", s);
    json_object_put(ev);
}

void resp_emit_content_part_done(int fd, const resp_result_t *r)
{
    (void)r;
    struct json_object *ev = json_object_new_object();
    json_object_object_add(ev, "output_index", json_object_new_int(0));
    json_object_object_add(ev, "content_index", json_object_new_int(0));
    struct json_object *part = json_object_new_object();
    json_object_object_add(part, "type",
        json_object_new_string("output_text"));
    json_object_object_add(part, "text",
        json_object_new_string(r->full_text ? r->full_text : ""));
    json_object_object_add(ev, "part", part);
    const char *s = json_object_to_json_string_ext(ev,
        JSON_C_TO_STRING_PLAIN);
    resp_send_sse_event(fd, "response.content_part.done", s);
    json_object_put(ev);
}

void resp_emit_output_item_done(int fd, const resp_result_t *r,
                                const char *model)
{
    struct json_object *item = resp_build_message_item(r, model,
        r->full_text ? r->full_text : "", "completed");
    struct json_object *ev = json_object_new_object();
    json_object_object_add(ev, "output_index", json_object_new_int(0));
    json_object_object_add(ev, "item", item);
    const char *s = json_object_to_json_string_ext(ev,
        JSON_C_TO_STRING_PLAIN);
    resp_send_sse_event(fd, "response.output_item.done", s);
    json_object_put(ev);
}

void resp_emit_completed(int fd, const resp_result_t *r,
                         const char *model)
{
    struct json_object *obj = resp_build_skeleton(r, model, "completed");

    /* Fill output array based on result type */
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
    resp_send_sse_event(fd, "response.completed", s);
    json_object_put(obj);
}

void resp_emit_func_call_added(int fd, const resp_result_t *r,
                               int idx, const char *model)
{
    struct json_object *item = resp_build_func_call_item(
        &r->tool_calls[idx], model, "in_progress");
    struct json_object *ev = json_object_new_object();
    json_object_object_add(ev, "output_index",
        json_object_new_int(idx));
    json_object_object_add(ev, "item", item);
    const char *s = json_object_to_json_string_ext(ev,
        JSON_C_TO_STRING_PLAIN);
    resp_send_sse_event(fd, "response.output_item.added", s);
    json_object_put(ev);
}

void resp_emit_func_call_args_delta(int fd, const resp_result_t *r,
                                    int idx)
{
    struct json_object *ev = json_object_new_object();
    json_object_object_add(ev, "output_index",
        json_object_new_int(idx));
    json_object_object_add(ev, "delta",
        json_object_new_string(r->tool_calls[idx].arguments));
    const char *s = json_object_to_json_string_ext(ev,
        JSON_C_TO_STRING_PLAIN);
    resp_send_sse_event(fd, "response.function_call_arguments.delta", s);
    json_object_put(ev);
}

void resp_emit_func_call_args_done(int fd, const resp_result_t *r,
                                   int idx)
{
    struct json_object *ev = json_object_new_object();
    json_object_object_add(ev, "output_index",
        json_object_new_int(idx));
    json_object_object_add(ev, "arguments",
        json_object_new_string(r->tool_calls[idx].arguments));
    const char *s = json_object_to_json_string_ext(ev,
        JSON_C_TO_STRING_PLAIN);
    resp_send_sse_event(fd, "response.function_call_arguments.done", s);
    json_object_put(ev);
}

void resp_emit_func_call_item_done(int fd, const resp_result_t *r,
                                   int idx, const char *model)
{
    struct json_object *item = resp_build_func_call_item(
        &r->tool_calls[idx], model, "completed");
    struct json_object *ev = json_object_new_object();
    json_object_object_add(ev, "output_index",
        json_object_new_int(idx));
    json_object_object_add(ev, "item", item);
    const char *s = json_object_to_json_string_ext(ev,
        JSON_C_TO_STRING_PLAIN);
    resp_send_sse_event(fd, "response.output_item.done", s);
    json_object_put(ev);
}
