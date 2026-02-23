#ifndef UVA_PROXY_THINK_PARSER_H
#define UVA_PROXY_THINK_PARSER_H

/*
 * Streaming parser for <think>...</think> blocks.
 *
 * Separates model output into reasoning (inside think tags) and
 * content (outside), emitting each piece via callback as it arrives.
 */

/* Callback: text is the parsed piece, is_reasoning = 1 for think blocks. */
typedef void (*think_emit_fn)(const char *text, int is_reasoning,
                              void *userdata);

typedef struct think_parser think_parser_t;

think_parser_t *think_parser_new(think_emit_fn emit, void *userdata);
void think_parser_feed(think_parser_t *tp, const char *text);
void think_parser_finish(think_parser_t *tp);
void think_parser_free(think_parser_t *tp);

/* Strip <think>...</think> from text for non-streaming responses.
 * If reasoning_out is non-NULL, extracted reasoning text is stored there.
 * Caller frees both returned strings. */
char *think_strip(const char *text, char **reasoning_out);

#endif /* UVA_PROXY_THINK_PARSER_H */
