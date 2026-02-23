#include "ui/webui/webui.h"
#include "ui/webui/webui_internal.h"
#include "core/platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>

#define MAX_RETRIES      3
#define HEALTH_TIMEOUT   60   /* seconds to wait for startup */
#define HEALTH_INTERVAL  1000 /* ms between health checks */
#define MONITOR_INTERVAL 5000 /* ms between child checks */

/* Global state shared with process.c via webui_internal.h */
volatile int    g_webui_running = 0;
int             g_webui_port = 8080;
int             g_webui_proxy_port = 8787;
char            g_webui_dir[WEBUI_PATH_MAX];

#ifdef PLATFORM_WINDOWS
HANDLE          g_webui_child_handle = NULL;
#else
volatile pid_t  g_webui_child_pid = 0;
#endif

static pthread_t g_thread;

int webui_get_port(void)
{
    return g_webui_port;
}

int webui_is_ready(void)
{
    return webui_check_health(g_webui_port);
}

/* Wait for health endpoint to respond. Returns 0 on success. */
static int wait_for_ready(void)
{
    int elapsed = 0;
    while (elapsed < HEALTH_TIMEOUT * 1000 && g_webui_running) {
        if (webui_check_health(g_webui_port)) return 0;
        if (!webui_child_alive()) return -1;
        platform_sleep_ms(HEALTH_INTERVAL);
        elapsed += HEALTH_INTERVAL;
    }
    return -1;
}

/* Run post-startup initialization (model icons, etc.) */
static void run_post_init(void)
{
    char script[WEBUI_PATH_MAX + 128];
    char cmd[WEBUI_PATH_MAX + 256];

    /* Find the init script relative to the source tree */
    char cwd[PLATFORM_PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) return;

    snprintf(script, sizeof(script),
             "%s/src/webui/init_models.py", cwd);

    struct stat st;
    if (stat(script, &st) != 0) return;

    snprintf(cmd, sizeof(cmd),
             "\"%s/.venv/bin/python3\" \"%s\" %d",
             g_webui_dir, script, g_webui_port);

    fprintf(stderr, "[webui] Running post-startup init...\n");
    system(cmd);
}

static void *webui_thread(void *arg)
{
    (void)arg;

    if (webui_ensure_setup(g_webui_dir) != 0) {
        fprintf(stderr, "[webui] Setup failed, chat interface unavailable.\n");
        return NULL;
    }

    /* Check if something is already running on the port */
    if (webui_port_in_use(g_webui_port)) {
        if (webui_check_health(g_webui_port)) {
            fprintf(stderr,
                    "[webui] Open WebUI already running on port %d.\n",
                    g_webui_port);
            return NULL;
        }
        fprintf(stderr, "[webui] Port %d in use by another process.\n",
                g_webui_port);
        return NULL;
    }

    int retries = 0;
    while (g_webui_running && retries < MAX_RETRIES) {
        fprintf(stderr, "[webui] Starting Open WebUI on port %d...\n",
                g_webui_port);

        if (webui_spawn_child() != 0) {
            retries++;
            platform_sleep_ms(2000);
            continue;
        }

        if (wait_for_ready() == 0) {
            fprintf(stderr, "[webui] Open WebUI is ready.\n");
            run_post_init();
        } else if (g_webui_running) {
            fprintf(stderr, "[webui] Open WebUI failed to start.\n");
            webui_kill_child();
            retries++;
            platform_sleep_ms(2000);
            continue;
        }

        /* Monitor loop */
        while (g_webui_running && webui_child_alive()) {
            platform_sleep_ms(MONITOR_INTERVAL);
        }

        if (!g_webui_running) break;

        fprintf(stderr, "[webui] Open WebUI process exited unexpectedly.\n");
        retries++;
        if (retries < MAX_RETRIES) {
            fprintf(stderr, "[webui] Restarting (attempt %d/%d)...\n",
                    retries + 1, MAX_RETRIES);
            platform_sleep_ms(2000);
        }
    }

    if (retries >= MAX_RETRIES) {
        fprintf(stderr, "[webui] Max retries reached, giving up.\n");
    }

    return NULL;
}

/* Resolve the path to vendor/open-webui. Returns 0 on success. */
static int resolve_webui_dir(void)
{
    char cwd[PLATFORM_PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("[webui] getcwd");
        return -1;
    }

    /* The proxy runs from services/proxy/, webui is at vendor/open-webui/ */
    snprintf(g_webui_dir, sizeof(g_webui_dir),
             "%s/../../vendor/open-webui", cwd);

    char check[WEBUI_BUF_MAX];
    struct stat st;
    snprintf(check, sizeof(check), "%s/backend", g_webui_dir);
    if (stat(check, &st) == 0) return 0;

    /* Try absolute fallback from home directory */
    const char *home = platform_home_dir();
    snprintf(g_webui_dir, sizeof(g_webui_dir),
             "%s/Projects/reversed_uva_ai/vendor/open-webui", home);
    snprintf(check, sizeof(check), "%s/backend", g_webui_dir);
    if (stat(check, &st) == 0) return 0;

    fprintf(stderr, "[webui] Could not find vendor/open-webui directory.\n");
    return -1;
}

int webui_start(const proxy_config_t *cfg)
{
    if (g_webui_running) return 0;

    g_webui_port = cfg->webui_port;
    g_webui_proxy_port = cfg->listen_port;

    if (resolve_webui_dir() != 0) return -1;

    g_webui_running = 1;

    if (pthread_create(&g_thread, NULL, webui_thread, NULL) != 0) {
        perror("[webui] pthread_create");
        g_webui_running = 0;
        return -1;
    }

    pthread_detach(g_thread);
    return 0;
}

void webui_stop(void)
{
    if (!g_webui_running) return;
    g_webui_running = 0;

    /* Give the thread a moment to notice the flag */
    platform_sleep_ms(200);

    webui_kill_child();
    fprintf(stderr, "[webui] Stopped.\n");
}
