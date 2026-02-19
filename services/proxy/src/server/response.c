#include "server.h"
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void response_send(int fd, int status, const char *content_type,
                   const char *body, size_t body_len)
{
    const char *status_text;
    switch (status) {
    case 200: status_text = "OK"; break;
    case 400: status_text = "Bad Request"; break;
    case 401: status_text = "Unauthorized"; break;
    case 404: status_text = "Not Found"; break;
    case 405: status_text = "Method Not Allowed"; break;
    case 500: status_text = "Internal Server Error"; break;
    case 502: status_text = "Bad Gateway"; break;
    default:  status_text = "Unknown"; break;
    }

    char header[1024];
    int hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
        "Connection: close\r\n"
        "\r\n",
        status, status_text, content_type, body_len);

    platform_send(fd, header, (size_t)hlen);
    if (body && body_len > 0)
        platform_send(fd, body, body_len);
}

void response_send_json(int fd, int status, const char *json)
{
    response_send(fd, status, "application/json", json, strlen(json));
}

void response_send_error(int fd, int status, const char *message)
{
    char buf[512];
    int len = snprintf(buf, sizeof(buf),
        "{\"error\":{\"message\":\"%s\",\"type\":\"proxy_error\",\"code\":%d}}",
        message, status);
    response_send(fd, status, "application/json", buf, (size_t)len);
}

void response_start_sse(int fd)
{
    const char *header =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: keep-alive\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n";
    platform_send(fd, header, strlen(header));
}

void response_send_sse_data(int fd, const char *data)
{
    /* chunked transfer encoding */
    char line[8192];
    int len = snprintf(line, sizeof(line), "data: %s\n\n", data);
    char chunk[8320];
    int clen = snprintf(chunk, sizeof(chunk), "%x\r\n%s\r\n", len, line);
    platform_send(fd, chunk, (size_t)clen);
}

void response_end_sse(int fd)
{
    /* Send [DONE] and final chunk */
    const char *done = "data: [DONE]\n\n";
    char chunk[128];
    int clen = snprintf(chunk, sizeof(chunk), "%x\r\n%s\r\n",
                        (int)strlen(done), done);
    platform_send(fd, chunk, (size_t)clen);

    /* Terminating chunk */
    platform_send(fd, "0\r\n\r\n", 5);
}

/* Declaration for external use */
void response_send_options(int fd)
{
    const char *header =
        "HTTP/1.1 204 No Content\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
        "Access-Control-Max-Age: 86400\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n"
        "\r\n";
    platform_send(fd, header, strlen(header));
}
