#include "stream.h"
#include "translator.h"
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int write_all(int fd, const char *data, size_t len)
{
    return platform_send(fd, data, len) == 0 ? (int)len : -1;
}

int stream_emit_headers(int fd)
{
    const char *header =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: keep-alive\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n";
    return write_all(fd, header, strlen(header));
}

int stream_emit_chunk(int fd, const char *model, const char *completion_id,
                      const char *delta_content, int chunk_index)
{
    char *json = translate_stream_chunk(delta_content, model,
                                         completion_id, chunk_index);
    if (!json) return -1;

    /* Format as SSE data line inside chunked transfer encoding */
    char line[8192];
    int line_len = snprintf(line, sizeof(line), "data: %s\n\n", json);
    free(json);

    char chunk[8320];
    int chunk_len = snprintf(chunk, sizeof(chunk), "%x\r\n%.*s\r\n",
                             line_len, line_len, line);
    return write_all(fd, chunk, (size_t)chunk_len);
}

int stream_emit_done(int fd)
{
    const char *done_line = "data: [DONE]\n\n";
    size_t done_len = strlen(done_line);

    char chunk[128];
    int chunk_len = snprintf(chunk, sizeof(chunk), "%x\r\n%s\r\n",
                             (int)done_len, done_line);
    int ret = write_all(fd, chunk, (size_t)chunk_len);
    if (ret < 0) return -1;

    /* Terminating zero-length chunk */
    return write_all(fd, "0\r\n\r\n", 5);
}
