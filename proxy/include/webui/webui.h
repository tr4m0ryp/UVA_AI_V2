#ifndef UVA_PROXY_WEBUI_H
#define UVA_PROXY_WEBUI_H

#include "config/config.h"

/* Start Open WebUI in a background thread.
 * Runs first-time setup if needed, then spawns uvicorn.
 * Returns 0 on success, -1 on failure. */
int webui_start(const proxy_config_t *cfg);

/* Stop the Open WebUI child process and background thread. */
void webui_stop(void);

/* Returns 1 if Open WebUI is responding on its port. */
int webui_is_ready(void);

/* Returns the configured webui port. */
int webui_get_port(void);

/* --- Internal (used by launcher.c) --- */

/* Run first-time setup: venv, packages, frontend build.
 * webui_dir is the absolute path to vendor/open-webui.
 * Returns 0 on success, -1 on failure. */
int webui_ensure_setup(const char *webui_dir);

/* HTTP health check on 127.0.0.1:port/health. Returns 1 if OK. */
int webui_check_health(int port);

/* TCP connect check. Returns 1 if port is in use. */
int webui_port_in_use(int port);

#endif /* UVA_PROXY_WEBUI_H */
