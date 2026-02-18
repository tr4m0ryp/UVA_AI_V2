#include "auth.h"
#include "upstream.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

static volatile int g_refresh_running = 0;
static pthread_t    g_refresh_tid;
static int          g_refresh_interval = 600; /* seconds */
static const proxy_config_t *g_cfg_ref = NULL;

int auth_validate_session(const proxy_config_t *cfg)
{
    if (!cfg->session_cookie[0]) {
        fprintf(stderr, "No session cookie configured.\n");
        return 0;
    }

    /* upstream_session_valid uses the global config set by upstream_init */
    int valid = upstream_session_valid();
    if (valid) {
        fprintf(stderr, "Session cookie is valid.\n");
    } else {
        fprintf(stderr, "Session cookie is invalid or expired.\n");
    }
    return valid;
}

static void *refresh_thread(void *arg)
{
    (void)arg;

    while (g_refresh_running) {
        sleep((unsigned)g_refresh_interval);
        if (!g_refresh_running) break;

        int valid = upstream_session_valid();
        if (!valid) {
            fprintf(stderr,
                "WARNING: Session cookie has expired. "
                "Re-run with --login to authenticate.\n");
        }
    }
    return NULL;
}

int auth_start_refresh(const proxy_config_t *cfg, int interval_sec)
{
    if (g_refresh_running) return 0;

    g_cfg_ref = cfg;
    g_refresh_interval = interval_sec > 0 ? interval_sec : 600;
    g_refresh_running = 1;

    if (pthread_create(&g_refresh_tid, NULL, refresh_thread, NULL) != 0) {
        perror("auth_start_refresh: pthread_create");
        g_refresh_running = 0;
        return -1;
    }

    pthread_detach(g_refresh_tid);
    fprintf(stderr, "Session refresh thread started (interval: %ds)\n",
            g_refresh_interval);
    return 0;
}

void auth_stop_refresh(void)
{
    g_refresh_running = 0;
}
