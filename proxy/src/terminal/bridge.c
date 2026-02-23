#include "terminal/terminal.h"
#include "websocket/websocket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef PLATFORM_WINDOWS

int terminal_bridge(terminal_session_t *s)
{
    (void)s;
    return -1;
}

#else /* POSIX */

#include <unistd.h>
#include <poll.h>
#include <sys/wait.h>
#include <json-c/json.h>

/* Check if the child process is still alive.
 * Returns 1 if alive, 0 if exited. */
static int child_alive(terminal_session_t *s)
{
    if (s->child_pid <= 0) return 0;
    int status;
    pid_t ret = waitpid(s->child_pid, &status, WNOHANG);
    if (ret == 0) return 1;   /* still running */
    s->child_pid = -1;
    return 0;
}

/* Try to parse a resize message from the WebSocket payload.
 * Format: {"type":"resize","cols":N,"rows":N}
 * Returns 1 if it was a resize message (handled), 0 otherwise. */
static int try_handle_resize(terminal_session_t *s,
                              const uint8_t *data, size_t len)
{
    if (len < 10 || data[0] != '{') return 0;

    /* Parse as JSON -- must be null-terminated */
    char *tmp = malloc(len + 1);
    if (!tmp) return 0;
    memcpy(tmp, data, len);
    tmp[len] = '\0';

    struct json_object *obj = json_tokener_parse(tmp);
    free(tmp);
    if (!obj) return 0;

    struct json_object *type_val;
    if (!json_object_object_get_ex(obj, "type", &type_val) ||
        strcmp(json_object_get_string(type_val), "resize") != 0) {
        json_object_put(obj);
        return 0;
    }

    struct json_object *cols_val, *rows_val;
    int cols = 80, rows = 24;
    if (json_object_object_get_ex(obj, "cols", &cols_val))
        cols = json_object_get_int(cols_val);
    if (json_object_object_get_ex(obj, "rows", &rows_val))
        rows = json_object_get_int(rows_val);
    json_object_put(obj);

    terminal_resize(s, cols, rows);
    return 1;
}

int terminal_bridge(terminal_session_t *s)
{
    if (!s || !s->active) return -1;

    struct pollfd fds[2];
    fds[0].fd = s->ws_fd;
    fds[0].events = POLLIN;
    fds[1].fd = s->master_fd;
    fds[1].events = POLLIN;

    char pty_buf[4096];

    while (s->active) {
        int ret = poll(fds, 2, 500);  /* 500ms timeout for child checks */
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }

        /* Check child alive on timeout */
        if (ret == 0) {
            if (!child_alive(s)) {
                /* Send remaining PTY output before closing */
                ssize_t n = read(s->master_fd, pty_buf, sizeof(pty_buf));
                if (n > 0)
                    ws_send_frame(s->ws_fd, WS_OPCODE_BINARY, pty_buf,
                                  (size_t)n);
                break;
            }
            continue;
        }

        /* WebSocket input -> PTY */
        if (fds[0].revents & POLLIN) {
            ws_frame_t frame;
            if (ws_read_frame(s->ws_fd, &frame) != 0) {
                /* Client disconnected or sent close */
                break;
            }

            if (frame.payload && frame.payload_len > 0) {
                if (!try_handle_resize(s, frame.payload, frame.payload_len)) {
                    /* Forward user input to PTY */
                    write(s->master_fd, frame.payload, frame.payload_len);
                }
                free(frame.payload);
            }
        }

        if (fds[0].revents & (POLLHUP | POLLERR)) {
            break;
        }

        /* PTY output -> WebSocket */
        if (fds[1].revents & POLLIN) {
            ssize_t n = read(s->master_fd, pty_buf, sizeof(pty_buf));
            if (n <= 0) {
                /* PTY closed (child exited) */
                break;
            }
            if (ws_send_frame(s->ws_fd, WS_OPCODE_BINARY, pty_buf,
                              (size_t)n) != 0) {
                break;
            }
        }

        if (fds[1].revents & (POLLHUP | POLLERR)) {
            /* Child exited -- drain remaining output */
            for (;;) {
                ssize_t n = read(s->master_fd, pty_buf, sizeof(pty_buf));
                if (n <= 0) break;
                ws_send_frame(s->ws_fd, WS_OPCODE_BINARY, pty_buf,
                              (size_t)n);
            }
            break;
        }
    }

    terminal_kill(s);
    ws_send_close(s->ws_fd, 1000);
    return 0;
}

#endif /* PLATFORM_WINDOWS */
