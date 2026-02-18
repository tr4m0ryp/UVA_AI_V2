#ifndef UVA_PROXY_TRANSLATOR_H
#define UVA_PROXY_TRANSLATOR_H

#include <json-c/json.h>
#include <stddef.h>

#define MAX_MODELS 32

typedef struct {
    const char *openai_id;    /* e.g. "claude-sonnet-4.5" */
    const char *uva_id;       /* e.g. "claude-sonnet-4.5" (may differ) */
    const char *display_name; /* e.g. "Claude Sonnet 4.5" */
} model_mapping_t;

/* Get the full model mapping table. Returns count. */
int models_get_all(const model_mapping_t **out);

/* Translate an OpenAI model ID to UvA model ID. Returns NULL if unknown. */
const char *models_to_uva(const char *openai_id);

/* Translate a UvA model ID to OpenAI model ID. Returns NULL if unknown. */
const char *models_to_openai(const char *uva_id);

/* Build OpenAI /v1/models JSON response. Caller frees returned string. */
char *models_list_json(void);

/* Translate OpenAI chat completion request to UvA request body.
 * thread_id is the UvA chat thread to use.
 * Returns a new JSON string (caller frees), or NULL on error. */
char *translate_request(const char *openai_body, const char *thread_id);

/* Build an OpenAI chat completion response (non-streaming).
 * content is the full assistant message text.
 * model is the model name used. Caller frees returned string. */
char *translate_response(const char *content, const char *model,
                         const char *completion_id);

/* Build an OpenAI SSE chunk for streaming. Caller frees returned string. */
char *translate_stream_chunk(const char *delta_content, const char *model,
                             const char *completion_id, int index);

/* Build the final [DONE] SSE line. Returns static string. */
const char *translate_stream_done(void);

/* ----------------------------------------------------------------------- */
/* Tool call translation                                                     */
/* ----------------------------------------------------------------------- */

#define MAX_TOOL_CALLS 16

typedef struct {
    char name[64];          /* tool name from name="..." attribute */
    char args_json[4096];   /* JSON body between the tags */
    char call_id[40];       /* generated: "call_" + random hex */
} parsed_tool_call_t;

/*
 * Build the tool-calling system prompt from an OpenAI tools[] JSON array.
 * Returns heap-allocated string, caller frees. Returns NULL on error.
 */
char *tool_definitions_to_sysprompt(struct json_object *tools_array);

/*
 * Inject the tool system prompt into the messages array.
 * Prepends to an existing "system" role message or inserts one at index 0.
 * Returns a new JSON object (caller owns it); input req is not modified.
 */
struct json_object *inject_tool_sysprompt(struct json_object *req,
                                           const char *tool_sysprompt);

/*
 * Convert tool_result messages in messages[] to plain user messages.
 * {"role":"tool","tool_call_id":"call_xxx","content":"result text"}
 * becomes: {"role":"user","content":"Tool result (call_xxx):\nresult text"}
 * Returns a new JSON array; caller owns it.
 */
struct json_object *fold_tool_results(struct json_object *messages_array);

/*
 * Parse a buffered UvA SSE response and extract plain text content.
 * Returns heap-allocated string (caller frees), or NULL on error.
 */
char *extract_text_from_sse(const char *data, size_t len);

/*
 * Scan text for <tool_call name="X">JSON</tool_call> blocks.
 * Returns count found (0..MAX_TOOL_CALLS). Fills calls[].
 */
int parse_tool_calls_from_text(const char *text,
                                parsed_tool_call_t *calls, int max_calls);

/*
 * Build an OpenAI tool_calls response JSON string (non-streaming).
 * Caller frees the returned string.
 */
char *build_tool_calls_response(const parsed_tool_call_t *calls, int count,
                                 const char *model, const char *completion_id);

/*
 * Write OpenAI streaming SSE chunks for tool calls to client_fd.
 */
void emit_tool_calls_sse(int client_fd, const parsed_tool_call_t *calls,
                          int count, const char *model,
                          const char *completion_id);

#endif /* UVA_PROXY_TRANSLATOR_H */
