#ifndef UVA_PROXY_RESPONSES_H
#define UVA_PROXY_RESPONSES_H

#include "server.h"
#include <json-c/json.h>

#define RESP_ID_LEN          22   /* "resp_" + 16 hex + null */
#define RESP_MSG_ID_LEN      17   /* "msg_" + 12 hex + null */
#define RESP_CALL_ID_LEN     30   /* "call_" + 24 hex + null */
#define RESP_MAX_TOOL_CALLS  16
#define RESP_MAX_NAME        128
#define RESP_MAX_ARGS        32768
#define RESP_MAX_MODEL       128
#define RESP_MAX_INSTRUCTIONS 8192

/* tool_choice values */
#define RESP_TC_AUTO     0
#define RESP_TC_NONE     1
#define RESP_TC_REQUIRED 2

typedef struct {
    char name[RESP_MAX_NAME];
    char arguments[RESP_MAX_ARGS];
    char call_id[RESP_CALL_ID_LEN];
} resp_tool_call_t;

typedef struct {
    char  response_id[RESP_ID_LEN];
    char  msg_id[RESP_MSG_ID_LEN];
    char *full_text;            /* accumulated text (caller frees) */
    int   tool_call_count;
    resp_tool_call_t tool_calls[RESP_MAX_TOOL_CALLS];
    int   seq;                  /* SSE sequence_number counter */
} resp_result_t;

typedef struct {
    char               model[RESP_MAX_MODEL];
    char               instructions[RESP_MAX_INSTRUCTIONS];
    int                has_tools;
    struct json_object *tools_json;     /* borrowed, do not free */
    int                stream;
    int                tool_choice;     /* RESP_TC_AUTO/NONE/REQUIRED */
    char               previous_response_id[RESP_ID_LEN];
    struct json_object *input_json;     /* borrowed, do not free */
    double             temperature;     /* -1 = not set */
    int                max_tokens;      /* 0  = not set */
    double             top_p;           /* -1 = not set */
} resp_request_t;

/* Main entry point for /v1/responses */
void handle_responses(http_request_t *req);

/* Initialize response_state table. Call after db_init(). */
int resp_state_init_schema(void);

/* State persistence */
int  resp_state_save(const char *response_id, const char *messages_json);
char *resp_state_load(const char *response_id);
int  resp_state_delete(const char *response_id);

/* Input normalization */
struct json_object *resp_input_to_messages(struct json_object *input,
                                           const char *instructions);
void resp_fold_tool_results(struct json_object *messages);

/* Tool prompt building (Hermes-style) */
char *resp_build_tool_prompt(struct json_object *tools_json);

/* Tool call parsing */
int   resp_parse_tool_calls(const char *text, resp_tool_call_t *out,
                            int max_calls);

/* Handler paths */
int resp_handle_text_stream(int fd, const char *uva_body,
                            const char *cookie, const char *model,
                            resp_result_t *result);

/* Buffered (non-streaming) handler */
int resp_handle_text_buffered(const char *uva_body,
                              const char *cookie, const char *model,
                              resp_result_t *result);

/* SSE emitters */
void resp_emit_created(int fd, const resp_result_t *r, const char *model);
void resp_emit_output_item_added(int fd, const resp_result_t *r,
                                 const char *model);
void resp_emit_content_part_added(int fd, const resp_result_t *r);
void resp_emit_text_delta(int fd, const resp_result_t *r,
                          const char *delta);
void resp_emit_text_done(int fd, const resp_result_t *r);
void resp_emit_content_part_done(int fd, const resp_result_t *r);
void resp_emit_output_item_done(int fd, const resp_result_t *r,
                                const char *model);
void resp_emit_completed(int fd, const resp_result_t *r,
                         const char *model);
void resp_emit_func_call_added(int fd, const resp_result_t *r,
                               int idx, const char *model);
void resp_emit_func_call_args_delta(int fd, const resp_result_t *r,
                                    int idx);
void resp_emit_func_call_args_done(int fd, const resp_result_t *r,
                                   int idx);
void resp_emit_func_call_item_done(int fd, const resp_result_t *r,
                                   int idx, const char *model);

#endif /* UVA_PROXY_RESPONSES_H */
