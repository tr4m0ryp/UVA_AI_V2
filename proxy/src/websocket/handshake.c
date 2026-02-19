#include "websocket.h"
#include "platform.h"
#include <stdio.h>
#include <string.h>
#include <openssl/sha.h>
#include <openssl/evp.h>

/* RFC 6455 magic GUID */
static const char *WS_MAGIC = "258EAFA5-E914-47DA-95CA-5B8B13085502";

int ws_is_upgrade(const http_request_t *req)
{
    const char *upgrade = request_get_header(req, "Upgrade");
    if (!upgrade) return 0;
    return (strcasecmp(upgrade, "websocket") == 0);
}

int ws_handshake(int fd, const http_request_t *req)
{
    const char *key = request_get_header(req, "Sec-WebSocket-Key");
    if (!key || strlen(key) == 0) {
        fprintf(stderr, "ws_handshake: missing Sec-WebSocket-Key\n");
        return -1;
    }

    /* Concatenate key + magic GUID */
    char concat[256];
    int n = snprintf(concat, sizeof(concat), "%s%s", key, WS_MAGIC);
    if (n < 0 || (size_t)n >= sizeof(concat)) return -1;

    /* SHA-1 hash */
    unsigned char sha1_hash[SHA_DIGEST_LENGTH];
    SHA1((const unsigned char *)concat, (size_t)n, sha1_hash);

    /* Base64 encode */
    char accept_key[64];
    int b64_len = EVP_EncodeBlock((unsigned char *)accept_key,
                                   sha1_hash, SHA_DIGEST_LENGTH);
    if (b64_len <= 0) return -1;
    accept_key[b64_len] = '\0';

    /* Build 101 Switching Protocols response */
    char response[512];
    int rlen = snprintf(response, sizeof(response),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "\r\n",
        accept_key);

    if (rlen < 0 || (size_t)rlen >= sizeof(response)) return -1;

    /* Send response */
    if (platform_send(fd, response, (size_t)rlen) != 0)
        return -1;

    return 0;
}
