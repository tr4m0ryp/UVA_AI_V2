#ifndef UVA_PROXY_VPS_H
#define UVA_PROXY_VPS_H

#include <stddef.h>
#include <sys/types.h>

/* Run a command on the VPS via SSH. Captures stdout to out buffer.
 * Returns exit code (0 = success). */
int vps_exec(const char *cmd, char *out, size_t out_size);

/* Create + start a Docker container for a coding task.
 * Writes container ID to cid_out. Returns 0 on success. */
int vps_create_container(const char *repo_full_name,
                         const char *github_token,
                         const char *branch, const char *model,
                         const char *instructions,
                         const char *api_key,
                         char *cid_out, size_t cid_size);

/* Attach a terminal: forks ssh + docker exec.
 * Returns master_fd or -1. Sets child_pid. */
int vps_attach_terminal(const char *container_id, pid_t *child_pid,
                        int cols, int rows);

/* Get container status (running/exited/etc). Returns 0 on success. */
int vps_container_status(const char *container_id,
                         char *status_out, size_t status_size);

/* Get container logs (last 100 lines). Caller frees *out. Returns 0. */
int vps_container_logs(const char *container_id, char **out);

/* Stop + remove a container. Returns 0. */
int vps_remove_container(const char *container_id);

#endif /* UVA_PROXY_VPS_H */
