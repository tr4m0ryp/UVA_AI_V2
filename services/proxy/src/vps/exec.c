#include "vps.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <sys/wait.h>
#endif

/* Global config pointer -- set by main.c */
extern const proxy_config_t *g_config;

int vps_exec(const char *cmd, char *out, size_t out_size)
{
    if (out && out_size > 0) out[0] = '\0';

    if (!g_config || !g_config->vps_host[0]) {
        fprintf(stderr, "vps_exec: VPS_HOST not configured\n");
        return -1;
    }

    /* Build SSH command:
     * ssh -i KEY -o StrictHostKeyChecking=no
     *     -o BatchMode=yes -o ConnectTimeout=10 HOST CMD */
    char ssh_cmd[4096];
    int n;

    if (g_config->vps_ssh_key[0]) {
        n = snprintf(ssh_cmd, sizeof(ssh_cmd),
            "ssh -i '%s' -o StrictHostKeyChecking=no "
            "-o BatchMode=yes -o ConnectTimeout=10 "
            "'%s' %s 2>/dev/null",
            g_config->vps_ssh_key, g_config->vps_host, cmd);
    } else {
        n = snprintf(ssh_cmd, sizeof(ssh_cmd),
            "ssh -o StrictHostKeyChecking=no "
            "-o BatchMode=yes -o ConnectTimeout=10 "
            "'%s' %s 2>/dev/null",
            g_config->vps_host, cmd);
    }

    if (n < 0 || (size_t)n >= sizeof(ssh_cmd)) {
        fprintf(stderr, "vps_exec: command too long\n");
        return -1;
    }

    FILE *fp = popen(ssh_cmd, "r");
    if (!fp) {
        perror("vps_exec: popen");
        return -1;
    }

    /* Read output */
    if (out && out_size > 0) {
        size_t total = 0;
        char buf[512];
        while (fgets(buf, sizeof(buf), fp)) {
            size_t len = strlen(buf);
            if (total + len >= out_size - 1) {
                size_t remain = out_size - 1 - total;
                if (remain > 0) {
                    memcpy(out + total, buf, remain);
                    total += remain;
                }
                break;
            }
            memcpy(out + total, buf, len);
            total += len;
        }
        out[total] = '\0';
        /* Trim trailing newline */
        while (total > 0 && (out[total-1] == '\n' || out[total-1] == '\r'))
            out[--total] = '\0';
    } else {
        /* Drain output */
        char buf[512];
        while (fgets(buf, sizeof(buf), fp)) {}
    }

    int status = pclose(fp);
    if (status == -1) return -1;

#ifdef _WIN32
    return status;
#else
    /* POSIX: extract exit code */
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    return -1;
#endif
}
