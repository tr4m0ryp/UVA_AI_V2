#ifndef UVA_PROXY_UPSTREAM_H
#define UVA_PROXY_UPSTREAM_H

#include "config.h"
#include <stddef.h>
#include <curl/curl.h>

#define MAX_THREAD_ID_LEN  64

typedef struct {
    char   *data;
    size_t  size;
    size_t  capacity;
} buffer_t;

/* Callback for streaming data from upstream. Return bytes consumed. */
typedef size_t (*stream_callback_t)(const char *data, size_t len, void *userdata);

/* Initialize the upstream client (call once at startup). */
int upstream_init(const proxy_config_t *cfg);

/* Cleanup the upstream client. */
void upstream_cleanup(void);

/* Send a chat request to UvA. If stream_cb is non-NULL, data is streamed
 * to the callback instead of buffered. Returns HTTP status code, or -1. */
int upstream_chat(const char *body, size_t body_len,
                  stream_callback_t stream_cb, void *cb_data,
                  buffer_t *response);

/* Call a Next.js server action. Returns HTTP status code, or -1. */
int upstream_server_action(const char *action_id,
                           const char *body, size_t body_len,
                           buffer_t *response);

/* Validate the current session cookie. Returns 1 if valid, 0 if not. */
int upstream_session_valid(void);

/* Send a chat request using a specific session cookie (for API key users).
 * Same interface as upstream_chat() but overrides the cookie. */
int upstream_chat_with_cookie(const char *body, size_t body_len,
                              const char *cookie,
                              stream_callback_t stream_cb, void *cb_data,
                              buffer_t *response);

/* Validate an arbitrary session cookie against UvA.
 * On success, writes email and name to out buffers. Returns 1 if valid. */
int upstream_validate_cookie(const char *cookie,
                             char *email_out, size_t email_size,
                             char *name_out, size_t name_size);

/* Buffer helpers. */
void buffer_init(buffer_t *buf);
void buffer_free(buffer_t *buf);
int  buffer_append(buffer_t *buf, const char *data, size_t len);

#endif /* UVA_PROXY_UPSTREAM_H */
