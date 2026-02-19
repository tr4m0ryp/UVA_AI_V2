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

/* Shared token callback for buffered paths (accumulates without SSE) */
void resp_on_buffer_token(const char *token, void *userdata);

/* Shared curl write callback for buffered paths */
typedef struct {
    stream_parser_t *parser;
} resp_buffer_cb_ctx_t;

size_t resp_buffer_write_cb(const char *data, size_t len, void *userdata);

/* Strip tool_call XML blocks from text (defined in tools_parse.c) */
char *resp_strip_tool_calls(const char *text);

/* Code block extraction fallback (defined in tools_parse.c) */
char *resp_extract_code_block_cmd(const char *text);
void  resp_synthesize_tool_call(resp_result_t *result, const char *cmd);

/* Route helpers (defined in route_helpers.c) */
int  resp_parse_request(struct json_object *parsed, resp_request_t *req);
char *resp_build_openai_body(struct json_object *messages,
                              const char *model, const resp_request_t *rr);
char *resp_build_nonstream_response(const resp_result_t *r,
                                     const char *model);

/* Tool retry + SSE emission helpers (defined in route_helpers.c) */
int  resp_tool_request_with_retry(struct json_object *messages,
                                   const char *model, const char *cookie,
                                   const resp_request_t *rr,
                                   resp_result_t *result);
void resp_emit_tool_result_sse(int fd, resp_result_t *result,
                                const char *model);

#endif /* UVA_PROXY_RESPONSES_INTERNAL_H */
