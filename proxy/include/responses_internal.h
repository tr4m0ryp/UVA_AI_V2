#ifndef UVA_PROXY_RESPONSES_INTERNAL_H
#define UVA_PROXY_RESPONSES_INTERNAL_H

#include "responses.h"
#include "upstream.h"
#include "stream.h"
#include <json-c/json.h>

/* Generate hex IDs for response/message/call identifiers */
void resp_gen_id(char *buf, const char *prefix, int hex_len);

/* Send a Responses API SSE event: "event: TYPE\ndata: JSON\n\n"
 * inside chunked transfer encoding. */
void resp_send_sse_event(int fd, const char *event_type,
                         const char *json_data);

/* Build a response skeleton JSON object (shared by created/completed) */
struct json_object *resp_build_skeleton(const resp_result_t *r,
                                        const char *model,
                                        const char *status);

/* Build a message output item JSON */
struct json_object *resp_build_message_item(const resp_result_t *r,
                                            const char *model,
                                            const char *text,
                                            const char *status);

/* Build a function_call output item JSON */
struct json_object *resp_build_func_call_item(const resp_tool_call_t *tc,
                                              const char *model,
                                              const char *status);

/* Streaming context for text path */
typedef struct {
    int              client_fd;
    resp_result_t   *result;
    const char      *model;
    stream_parser_t *parser;
    buffer_t         accum;
    int              first_token;
} resp_stream_ctx_t;

#endif /* UVA_PROXY_RESPONSES_INTERNAL_H */
