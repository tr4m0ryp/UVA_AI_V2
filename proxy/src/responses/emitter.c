#include "responses/responses.h"
#include "responses/responses_internal.h"
#include "platform/platform.h"
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

/* Send one Responses API SSE event.
 * Format: "event: TYPE\ndata: JSON\n\n" */
void resp_send_sse_event(int fd, const char *event_type,
                         const char *json_data)
{
    char line[16384];
    int len = snprintf(line, sizeof(line),
        "event: %s\ndata: %s\n\n", event_type, json_data);
    platform_send(fd, line, (size_t)len);
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
    json_object_object_add(obj, "parallel_tool_calls",
        json_object_new_boolean(1));
    json_object_object_add(obj, "usage", NULL);
    json_object_object_add(obj, "metadata",
        json_object_new_object());
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
        json_object_object_add(part, "annotations",
            json_object_new_array());
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

/* Helper: wrap data with type + sequence_number and emit */
static void emit_event(int fd, resp_result_t *r, const char *event_type,
                        struct json_object *data)
{
    r->seq++;
    json_object_object_add(data, "type",
        json_object_new_string(event_type));
    json_object_object_add(data, "sequence_number",
        json_object_new_int(r->seq));
    const char *s = json_object_to_json_string_ext(data,
        JSON_C_TO_STRING_PLAIN);
    resp_send_sse_event(fd, event_type, s);
    json_object_put(data);
}

void resp_emit_created(int fd, const resp_result_t *r, const char *model)
{
    resp_result_t *mr = (resp_result_t *)r; /* for seq increment */
    struct json_object *resp = resp_build_skeleton(r, model, "in_progress");
    struct json_object *ev = json_object_new_object();
    json_object_object_add(ev, "response", resp);
    emit_event(fd, mr, "response.created", ev);
}

void resp_emit_output_item_added(int fd, const resp_result_t *r,
                                 const char *model)
{
    resp_result_t *mr = (resp_result_t *)r;
    struct json_object *item = resp_build_message_item(r, model,
                                                       NULL, "in_progress");
    struct json_object *ev = json_object_new_object();
    json_object_object_add(ev, "output_index", json_object_new_int(0));
    json_object_object_add(ev, "item", item);
    emit_event(fd, mr, "response.output_item.added", ev);
}

void resp_emit_content_part_added(int fd, const resp_result_t *r)
{
    resp_result_t *mr = (resp_result_t *)r;
    struct json_object *ev = json_object_new_object();
    json_object_object_add(ev, "item_id",
        json_object_new_string(r->msg_id));
    json_object_object_add(ev, "output_index", json_object_new_int(0));
    json_object_object_add(ev, "content_index", json_object_new_int(0));
    struct json_object *part = json_object_new_object();
    json_object_object_add(part, "type",
        json_object_new_string("output_text"));
    json_object_object_add(part, "text", json_object_new_string(""));
    json_object_object_add(part, "annotations",
        json_object_new_array());
    json_object_object_add(ev, "part", part);
    emit_event(fd, mr, "response.content_part.added", ev);
}

void resp_emit_text_delta(int fd, const resp_result_t *r,
                          const char *delta)
{
    resp_result_t *mr = (resp_result_t *)r;
    struct json_object *ev = json_object_new_object();
    json_object_object_add(ev, "item_id",
        json_object_new_string(r->msg_id));
    json_object_object_add(ev, "output_index", json_object_new_int(0));
    json_object_object_add(ev, "content_index", json_object_new_int(0));
    json_object_object_add(ev, "delta",
        json_object_new_string(delta));
    emit_event(fd, mr, "response.output_text.delta", ev);
}

void resp_emit_text_done(int fd, const resp_result_t *r)
{
    resp_result_t *mr = (resp_result_t *)r;
    struct json_object *ev = json_object_new_object();
    json_object_object_add(ev, "item_id",
        json_object_new_string(r->msg_id));
    json_object_object_add(ev, "output_index", json_object_new_int(0));
    json_object_object_add(ev, "content_index", json_object_new_int(0));
    json_object_object_add(ev, "text",
        json_object_new_string(r->full_text ? r->full_text : ""));
    emit_event(fd, mr, "response.output_text.done", ev);
}

void resp_emit_content_part_done(int fd, const resp_result_t *r)
{
    resp_result_t *mr = (resp_result_t *)r;
    struct json_object *ev = json_object_new_object();
    json_object_object_add(ev, "item_id",
        json_object_new_string(r->msg_id));
    json_object_object_add(ev, "output_index", json_object_new_int(0));
    json_object_object_add(ev, "content_index", json_object_new_int(0));
    struct json_object *part = json_object_new_object();
    json_object_object_add(part, "type",
        json_object_new_string("output_text"));
    json_object_object_add(part, "text",
        json_object_new_string(r->full_text ? r->full_text : ""));
    json_object_object_add(part, "annotations",
        json_object_new_array());
    json_object_object_add(ev, "part", part);
    emit_event(fd, mr, "response.content_part.done", ev);
}

void resp_emit_output_item_done(int fd, const resp_result_t *r,
                                const char *model)
{
    resp_result_t *mr = (resp_result_t *)r;
    struct json_object *item = resp_build_message_item(r, model,
        r->full_text ? r->full_text : "", "completed");
    struct json_object *ev = json_object_new_object();
    json_object_object_add(ev, "output_index", json_object_new_int(0));
    json_object_object_add(ev, "item", item);
    emit_event(fd, mr, "response.output_item.done", ev);
}

void resp_emit_completed(int fd, const resp_result_t *r,
                         const char *model)
{
    resp_result_t *mr = (resp_result_t *)r;
    struct json_object *resp = resp_build_skeleton(r, model, "completed");

    /* Fill output array based on result type */
    struct json_object *output;
    json_object_object_get_ex(resp, "output", &output);

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

    /* Add usage */
    struct json_object *usage = json_object_new_object();
    json_object_object_add(usage, "input_tokens",
        json_object_new_int(0));
    json_object_object_add(usage, "output_tokens",
        json_object_new_int(0));
    json_object_object_add(usage, "total_tokens",
        json_object_new_int(0));
    json_object_object_del(resp, "usage");
    json_object_object_add(resp, "usage", usage);

    struct json_object *ev = json_object_new_object();
    json_object_object_add(ev, "response", resp);
    emit_event(fd, mr, "response.completed", ev);
}

void resp_emit_func_call_added(int fd, const resp_result_t *r,
                               int idx, const char *model)
{
    resp_result_t *mr = (resp_result_t *)r;
    struct json_object *item = resp_build_func_call_item(
        &r->tool_calls[idx], model, "in_progress");
    struct json_object *ev = json_object_new_object();
    json_object_object_add(ev, "output_index",
        json_object_new_int(idx));
    json_object_object_add(ev, "item", item);
    emit_event(fd, mr, "response.output_item.added", ev);
}

void resp_emit_func_call_args_delta(int fd, const resp_result_t *r,
                                    int idx)
{
    resp_result_t *mr = (resp_result_t *)r;
    struct json_object *ev = json_object_new_object();
    json_object_object_add(ev, "item_id",
        json_object_new_string(r->tool_calls[idx].call_id));
    json_object_object_add(ev, "output_index",
        json_object_new_int(idx));
    json_object_object_add(ev, "delta",
        json_object_new_string(r->tool_calls[idx].arguments));
    emit_event(fd, mr, "response.function_call_arguments.delta", ev);
}

void resp_emit_func_call_args_done(int fd, const resp_result_t *r,
                                   int idx)
{
    resp_result_t *mr = (resp_result_t *)r;
    struct json_object *ev = json_object_new_object();
    json_object_object_add(ev, "item_id",
        json_object_new_string(r->tool_calls[idx].call_id));
    json_object_object_add(ev, "output_index",
        json_object_new_int(idx));
    json_object_object_add(ev, "arguments",
        json_object_new_string(r->tool_calls[idx].arguments));
    emit_event(fd, mr, "response.function_call_arguments.done", ev);
}

void resp_emit_func_call_item_done(int fd, const resp_result_t *r,
                                   int idx, const char *model)
{
    resp_result_t *mr = (resp_result_t *)r;
    struct json_object *item = resp_build_func_call_item(
        &r->tool_calls[idx], model, "completed");
    struct json_object *ev = json_object_new_object();
    json_object_object_add(ev, "output_index",
        json_object_new_int(idx));
    json_object_object_add(ev, "item", item);
    emit_event(fd, mr, "response.output_item.done", ev);
}
