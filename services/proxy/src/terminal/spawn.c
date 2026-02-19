#include "terminal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef PLATFORM_WINDOWS

const char *terminal_find_codex(void) { return NULL; }

int terminal_spawn(terminal_session_t *s, const char *api_key,
                   const char *model, const char *work_dir,
                   int cols, int rows)
{
    (void)s; (void)api_key; (void)model;
    (void)work_dir; (void)cols; (void)rows;
    fprintf(stderr, "terminal_spawn: not supported on Windows\n");
    return -1;
}

int terminal_resize(terminal_session_t *s, int cols, int rows)
{
    (void)s; (void)cols; (void)rows;
    return -1;
}

void terminal_kill(terminal_session_t *s) { (void)s; }

#else /* POSIX */

#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/ioctl.h>

#if defined(__linux__)
  #include <pty.h>
#elif defined(__APPLE__)
  #include <util.h>
#endif

static const char *codex_paths[] = {
    "/usr/local/bin/codex",
    NULL  /* sentinel, $HOME path added at runtime */
};

const char *terminal_find_codex(void)
{
    /* Check well-known paths */
    for (int i = 0; codex_paths[i]; i++) {
        if (access(codex_paths[i], X_OK) == 0)
            return codex_paths[i];
    }

    /* Check $HOME/.local/bin/codex */
    static char home_path[512];
    const char *home = getenv("HOME");
    if (home) {
        snprintf(home_path, sizeof(home_path), "%s/.local/bin/codex", home);
        if (access(home_path, X_OK) == 0)
            return home_path;
    }

    /* Check $HOME/.npm-global/bin/codex */
    if (home) {
        snprintf(home_path, sizeof(home_path),
                 "%s/.npm-global/bin/codex", home);
        if (access(home_path, X_OK) == 0)
            return home_path;
    }

    /* Fall back to PATH lookup */
    static char which_buf[512];
    FILE *fp = popen("which codex 2>/dev/null", "r");
    if (fp) {
        if (fgets(which_buf, sizeof(which_buf), fp)) {
            size_t len = strlen(which_buf);
            if (len > 0 && which_buf[len - 1] == '\n')
                which_buf[len - 1] = '\0';
            pclose(fp);
            if (access(which_buf, X_OK) == 0)
                return which_buf;
        } else {
            pclose(fp);
        }
    }

    return NULL;
}

int terminal_spawn(terminal_session_t *s, const char *api_key,
                   const char *model, const char *work_dir,
                   int cols, int rows)
{
    const char *codex = terminal_find_codex();
    if (!codex) {
        fprintf(stderr, "terminal_spawn: codex CLI not found\n");
        return -1;
    }

    struct winsize ws;
    memset(&ws, 0, sizeof(ws));
    ws.ws_col = (unsigned short)cols;
    ws.ws_row = (unsigned short)rows;

    int master_fd;
    pid_t pid = forkpty(&master_fd, NULL, NULL, &ws);

    if (pid < 0) {
        perror("forkpty");
        return -1;
    }

    if (pid == 0) {
        /* Child process */
        setenv("UVA_PROXY_API_KEY", api_key, 1);
        setenv("OPENAI_API_KEY", api_key, 1);
        setenv("OPENAI_BASE_URL", "http://127.0.0.1:8787/v1", 1);
        setenv("TERM", "xterm-256color", 1);

        if (work_dir && work_dir[0])
            chdir(work_dir);

        execlp(codex, "codex", "--model", model, "--full-auto", NULL);
        perror("execlp codex");
        _exit(127);
    }

    /* Parent */
    s->master_fd = master_fd;
    s->child_pid = pid;
    s->active = 1;

    fprintf(stderr, "terminal_spawn: started codex (pid=%d, model=%s)\n",
            pid, model);
    return 0;
}

int terminal_resize(terminal_session_t *s, int cols, int rows)
{
    if (!s || s->master_fd < 0) return -1;
    struct winsize ws;
    memset(&ws, 0, sizeof(ws));
    ws.ws_col = (unsigned short)cols;
    ws.ws_row = (unsigned short)rows;
    return ioctl(s->master_fd, TIOCSWINSZ, &ws);
}

void terminal_kill(terminal_session_t *s)
{
    if (!s || !s->active) return;
    s->active = 0;

    if (s->child_pid > 0) {
        kill(s->child_pid, SIGTERM);

        /* Brief wait for graceful exit */
        int status;
        for (int i = 0; i < 10; i++) {
            if (waitpid(s->child_pid, &status, WNOHANG) != 0)
                goto done;
            usleep(50000);  /* 50ms */
        }

        /* Force kill */
        kill(s->child_pid, SIGKILL);
        waitpid(s->child_pid, &status, 0);
    }

done:
    if (s->master_fd >= 0) {
        close(s->master_fd);
        s->master_fd = -1;
    }
    s->child_pid = -1;
    fprintf(stderr, "terminal_kill: session cleaned up\n");
}

#endif /* PLATFORM_WINDOWS */
