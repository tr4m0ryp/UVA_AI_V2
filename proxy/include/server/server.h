#ifndef UVA_PROXY_SERVER_H
#define UVA_PROXY_SERVER_H

#include "config/config.h"
#include <stddef.h>

#define MAX_HEADERS       64
#define MAX_HEADER_NAME   256
#define MAX_HEADER_VALUE  2048
#define MAX_PATH_LEN      512
#define MAX_METHOD_LEN    16
#define MAX_BODY_LEN      (1024 * 1024)  /* 1 MB */

typedef struct {
    char name[MAX_HEADER_NAME];
    char value[MAX_HEADER_VALUE];
} http_header_t;

typedef struct {
    char          method[MAX_METHOD_LEN];
    char          path[MAX_PATH_LEN];
    http_header_t headers[MAX_HEADERS];
    int           header_count;
    char         *body;
    size_t        body_len;
    int           client_fd;
    int           fd_claimed;   /* set to 1 if handler takes ownership of fd */
} http_request_t;

/* Start the HTTP server. Blocks until shutdown. Returns 0 on success. */
int server_start(const proxy_config_t *cfg);

/* Bind + listen only (non-blocking). Returns 0 on success. */
int server_listen(const proxy_config_t *cfg);

/* Accept loop (blocks until server_stop). Returns 0 on success. */
int server_accept_loop(void);

/* Signal the server to stop. */
void server_stop(void);

/* Get a header value by name (case-insensitive). Returns NULL if not found. */
const char *request_get_header(const http_request_t *req, const char *name);

#endif /* UVA_PROXY_SERVER_H */
