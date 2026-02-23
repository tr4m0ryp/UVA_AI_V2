#ifndef UVA_PROXY_TERMINAL_H
#define UVA_PROXY_TERMINAL_H

#include <sys/types.h>

typedef struct {
    int    master_fd;    /* PTY master file descriptor */
    pid_t  child_pid;    /* codex process PID */
    int    ws_fd;        /* WebSocket client fd */
    int    active;       /* 1 while session is running */
} terminal_session_t;

/* Spawn a codex CLI process in a PTY.
 * Sets env vars for API key, model, base URL.
 * Returns 0 on success, -1 on error (e.g. Windows). */
int terminal_spawn(terminal_session_t *s, const char *api_key,
                   const char *model, const char *work_dir,
                   int cols, int rows);

/* Resize the PTY to new dimensions.
 * Returns 0 on success. */
int terminal_resize(terminal_session_t *s, int cols, int rows);

/* Bridge WebSocket <-> PTY until session ends.
 * Blocks the calling thread. Returns 0 on clean exit. */
int terminal_bridge(terminal_session_t *s);

/* Kill the child process and clean up PTY resources. */
void terminal_kill(terminal_session_t *s);

/* Find the codex binary. Returns static path or NULL. */
const char *terminal_find_codex(void);

#endif /* UVA_PROXY_TERMINAL_H */
