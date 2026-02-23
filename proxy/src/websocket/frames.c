#include "websocket/websocket.h"
#include "platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifndef PLATFORM_WINDOWS
  #include <sys/socket.h>
#endif

/* Read exactly n bytes from fd. Returns 0 on success, -1 on error. */
static int recv_exact(int fd, void *buf, size_t n)
{
    size_t total = 0;
    while (total < n) {
        int r = recv(fd, (char *)buf + total, (int)(n - total), 0);
        if (r <= 0) return -1;
        total += (size_t)r;
    }
    return 0;
}

int ws_read_frame(int fd, ws_frame_t *frame)
{
    memset(frame, 0, sizeof(*frame));

retry:
    ;
    /* Read 2-byte header */
    uint8_t hdr[2];
    if (recv_exact(fd, hdr, 2) != 0) return -1;

    frame->fin = (hdr[0] >> 7) & 1;
    frame->opcode = hdr[0] & 0x0F;
    int masked = (hdr[1] >> 7) & 1;
    uint64_t payload_len = hdr[1] & 0x7F;

    /* Extended payload length */
    if (payload_len == 126) {
        uint8_t ext[2];
        if (recv_exact(fd, ext, 2) != 0) return -1;
        payload_len = ((uint64_t)ext[0] << 8) | ext[1];
    } else if (payload_len == 127) {
        uint8_t ext[8];
        if (recv_exact(fd, ext, 8) != 0) return -1;
        payload_len = 0;
        for (int i = 0; i < 8; i++)
            payload_len = (payload_len << 8) | ext[i];
    }

    if (payload_len > WS_MAX_PAYLOAD) {
        fprintf(stderr, "ws_read_frame: payload too large (%lu)\n",
                (unsigned long)payload_len);
        return -1;
    }

    /* Read mask key (client frames must be masked) */
    uint8_t mask[4] = {0};
    if (masked) {
        if (recv_exact(fd, mask, 4) != 0) return -1;
    }

    /* Read payload */
    frame->payload_len = (size_t)payload_len;
    frame->payload = NULL;
    if (payload_len > 0) {
        frame->payload = malloc((size_t)payload_len);
        if (!frame->payload) return -1;
        if (recv_exact(fd, frame->payload, (size_t)payload_len) != 0) {
            free(frame->payload);
            frame->payload = NULL;
            return -1;
        }
        /* Unmask */
        if (masked) {
            for (size_t i = 0; i < frame->payload_len; i++)
                frame->payload[i] ^= mask[i & 3];
        }
    }

    /* Handle control frames */
    if (frame->opcode == WS_OPCODE_PING) {
        /* Auto-reply with PONG */
        ws_send_frame(fd, WS_OPCODE_PONG, frame->payload, frame->payload_len);
        free(frame->payload);
        frame->payload = NULL;
        goto retry;
    }

    if (frame->opcode == WS_OPCODE_CLOSE) {
        /* Echo close frame back */
        ws_send_frame(fd, WS_OPCODE_CLOSE, frame->payload, frame->payload_len);
        free(frame->payload);
        frame->payload = NULL;
        return -1;
    }

    return 0;
}

int ws_send_frame(int fd, uint8_t opcode, const void *data, size_t len)
{
    /* Build frame header (server frames are not masked) */
    uint8_t hdr[10];
    size_t hdr_len = 0;

    hdr[0] = 0x80 | (opcode & 0x0F);  /* FIN=1, opcode */
    hdr_len = 1;

    if (len < 126) {
        hdr[1] = (uint8_t)len;
        hdr_len = 2;
    } else if (len <= 0xFFFF) {
        hdr[1] = 126;
        hdr[2] = (uint8_t)(len >> 8);
        hdr[3] = (uint8_t)(len & 0xFF);
        hdr_len = 4;
    } else {
        hdr[1] = 127;
        for (int i = 0; i < 8; i++)
            hdr[2 + i] = (uint8_t)(len >> (56 - 8 * i));
        hdr_len = 10;
    }

    /* Send header */
    if (platform_send(fd, (const char *)hdr, hdr_len) != 0)
        return -1;

    /* Send payload */
    if (len > 0 && data) {
        if (platform_send(fd, (const char *)data, len) != 0)
            return -1;
    }

    return 0;
}

int ws_send_close(int fd, uint16_t code)
{
    uint8_t payload[2];
    payload[0] = (uint8_t)(code >> 8);
    payload[1] = (uint8_t)(code & 0xFF);
    return ws_send_frame(fd, WS_OPCODE_CLOSE, payload, 2);
}
