#ifndef UVA_PROXY_TRANSLATOR_H
#define UVA_PROXY_TRANSLATOR_H

#include <json-c/json.h>

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

#endif /* UVA_PROXY_TRANSLATOR_H */
