#include "vps.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#if defined(__linux__)
  #include <pty.h>
#elif defined(__APPLE__)
  #include <util.h>
#endif
#endif

extern const proxy_config_t *g_config;

/* Shell-escape a string by wrapping in single quotes.
 * Caller must free the result. */
static char *sh_escape(const char *s)
{
    size_t len = strlen(s);
    /* Worst case: every char is a single quote -> '\'' */
    char *out = malloc(len * 4 + 3);
    if (!out) return NULL;

    char *p = out;
    *p++ = '\'';
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '\'') {
            *p++ = '\'';
            *p++ = '\\';
            *p++ = '\'';
            *p++ = '\'';
        } else {
            *p++ = s[i];
        }
    }
    *p++ = '\'';
    *p = '\0';
    return out;
}

int vps_create_container(const char *repo_full_name,
                         const char *github_token,
                         const char *branch, const char *model,
                         const char *instructions,
                         const char *api_key,
                         char *cid_out, size_t cid_size)
{
    if (cid_out && cid_size > 0) cid_out[0] = '\0';

    const char *image = g_config->vps_docker_image[0]
        ? g_config->vps_docker_image : "uva-codex:latest";

    /* Build proxy base URL for the container to call back */
    char proxy_url[256];
    snprintf(proxy_url, sizeof(proxy_url),
        "http://host.docker.internal:%d", g_config->listen_port);

    /* Shell-escape user-provided values */
    char *esc_instructions = sh_escape(instructions);
    char *esc_repo = sh_escape(repo_full_name);
    char *esc_branch = sh_escape(branch);
    if (!esc_instructions || !esc_repo || !esc_branch) {
        free(esc_instructions);
        free(esc_repo);
        free(esc_branch);
        return -1;
    }

    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
        "docker run -d --add-host=host.docker.internal:host-gateway "
        "-e REPO_FULL_NAME=%s "
        "-e GITHUB_TOKEN='%s' "
        "-e BRANCH_NAME=%s "
        "-e MODEL='%s' "
        "-e INSTRUCTIONS=%s "
        "-e OPENAI_API_KEY='%s' "
        "-e PROXY_BASE_URL='%s' "
        "%s",
        esc_repo, github_token, esc_branch, model,
        esc_instructions, api_key, proxy_url, image);

    free(esc_instructions);
    free(esc_repo);
    free(esc_branch);

    char out[256] = {0};
    int rc = vps_exec(cmd, out, sizeof(out));
    if (rc != 0) {
        fprintf(stderr, "vps_create_container: docker run failed: %s\n", out);
        return -1;
    }

    /* Output is the container ID (first 12 chars or full SHA) */
    if (cid_out && cid_size > 0)
        snprintf(cid_out, cid_size, "%s", out);

    fprintf(stderr, "VPS container created: %.12s\n", out);
    return 0;
}

int vps_attach_terminal(const char *container_id, pid_t *child_pid,
                        int cols, int rows)
{
#ifdef _WIN32
    (void)container_id; (void)child_pid; (void)cols; (void)rows;
    fprintf(stderr, "vps_attach_terminal: not supported on Windows\n");
    return -1;
#else
    if (!g_config || !g_config->vps_host[0]) return -1;

    struct winsize ws;
    memset(&ws, 0, sizeof(ws));
    ws.ws_col = (unsigned short)cols;
    ws.ws_row = (unsigned short)rows;

    int master_fd;
    pid_t pid = forkpty(&master_fd, NULL, NULL, &ws);
    if (pid < 0) {
        perror("vps_attach_terminal: forkpty");
        return -1;
    }

    if (pid == 0) {
        /* Child: exec ssh -t VPS docker exec -it CID /bin/bash */
        if (g_config->vps_ssh_key[0]) {
            execlp("ssh", "ssh",
                "-i", g_config->vps_ssh_key,
                "-o", "StrictHostKeyChecking=no",
                "-t", g_config->vps_host,
                "docker", "exec", "-it", container_id, "/bin/bash",
                (char *)NULL);
        } else {
            execlp("ssh", "ssh",
                "-o", "StrictHostKeyChecking=no",
                "-t", g_config->vps_host,
                "docker", "exec", "-it", container_id, "/bin/bash",
                (char *)NULL);
        }
        perror("execlp");
        _exit(127);
    }

    *child_pid = pid;
    return master_fd;
#endif
}

int vps_container_status(const char *container_id,
                         char *status_out, size_t status_size)
{
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
        "docker inspect -f '{{.State.Status}}' '%s'", container_id);
    return vps_exec(cmd, status_out, status_size);
}

int vps_container_logs(const char *container_id, char **out)
{
    *out = NULL;

    char cmd[256];
    snprintf(cmd, sizeof(cmd),
        "docker logs --tail 200 '%s' 2>&1", container_id);

    /* Use a large buffer */
    char *buf = malloc(65536);
    if (!buf) return -1;

    int rc = vps_exec(cmd, buf, 65536);
    if (rc != 0) {
        /* Container might not exist; return whatever we got */
    }
    *out = buf;
    return 0;
}

int vps_remove_container(const char *container_id)
{
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "docker rm -f '%s'", container_id);
    return vps_exec(cmd, NULL, 0);
}
