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
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n";
    return write_all(fd, header, strlen(header));
}

int stream_emit_chunk(int fd, const char *model, const char *completion_id,
                      const char *delta_content, int chunk_index)
{
    char *json = translate_stream_chunk(delta_content, model,
                                         completion_id, chunk_index);
    if (!json) return -1;

    char line[8192];
    int line_len = snprintf(line, sizeof(line), "data: %s\n\n", json);
    free(json);

    return write_all(fd, line, (size_t)line_len);
}

int stream_emit_done(int fd)
{
    const char *done_line = "data: [DONE]\n\n";
    return write_all(fd, done_line, strlen(done_line));
}
