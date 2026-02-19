#ifndef UVA_PROXY_WEBSOCKET_H
#define UVA_PROXY_WEBSOCKET_H

#include "server.h"
#include <stddef.h>
#include <stdint.h>

/* WebSocket opcodes (RFC 6455 Section 5.2) */
#define WS_OPCODE_TEXT   0x1
#define WS_OPCODE_BINARY 0x2
#define WS_OPCODE_CLOSE  0x8
#define WS_OPCODE_PING   0x9
#define WS_OPCODE_PONG   0xA

/* Maximum frame payload we will accept (64 KB) */
#define WS_MAX_PAYLOAD   (64 * 1024)

typedef struct {
    uint8_t  opcode;
    uint8_t *payload;
    size_t   payload_len;
    int      fin;
} ws_frame_t;

/* Check if this HTTP request is a WebSocket upgrade. Returns 1 if yes. */
int ws_is_upgrade(const http_request_t *req);

/* Perform the RFC 6455 handshake. Sends 101 response on success.
 * Returns 0 on success, -1 on error. */
int ws_handshake(int fd, const http_request_t *req);

/* Read one WebSocket frame from fd. Caller must free frame->payload.
 * Auto-replies PONG to PING. Returns 0 on success, -1 on error/close. */
int ws_read_frame(int fd, ws_frame_t *frame);

/* Send a WebSocket frame (server-to-client, no mask).
 * Returns 0 on success, -1 on error. */
int ws_send_frame(int fd, uint8_t opcode, const void *data, size_t len);

/* Send a close frame with the given status code.
 * Returns 0 on success. */
int ws_send_close(int fd, uint16_t code);

#endif /* UVA_PROXY_WEBSOCKET_H */
