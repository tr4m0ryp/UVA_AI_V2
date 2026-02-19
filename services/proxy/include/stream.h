#ifndef UVA_PROXY_STREAM_H
#define UVA_PROXY_STREAM_H

#include <stddef.h>

/* Opaque stream parser state. */
typedef struct stream_parser stream_parser_t;

/* Callback invoked for each text token extracted from upstream. */
typedef void (*token_callback_t)(const char *token, void *userdata);

/* Create a new stream parser. cb is called for each extracted token. */
stream_parser_t *stream_parser_new(token_callback_t cb, void *userdata);

/* Feed raw upstream data into the parser. */
void stream_parser_feed(stream_parser_t *p, const char *data, size_t len);

/* Signal end of upstream data. Flushes any remaining buffered content. */
void stream_parser_finish(stream_parser_t *p);

/* Free the parser. */
void stream_parser_free(stream_parser_t *p);

/* Write an OpenAI SSE chunk to a file descriptor.
 * Returns bytes written, or -1 on error. */
int stream_emit_chunk(int fd, const char *model, const char *completion_id,
                      const char *delta_content, int chunk_index);

/* Write the final SSE [DONE] message. Returns bytes written, or -1. */
int stream_emit_done(int fd);

/* Write SSE headers to a file descriptor. Returns 0 on success. */
int stream_emit_headers(int fd);

#endif /* UVA_PROXY_STREAM_H */
