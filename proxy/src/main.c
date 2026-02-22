#include "config.h"
#include "server.h"
#include "upstream.h"
#include "auth.h"
#include "database.h"
#include "project.h"
#include "responses.h"
#include "thread_state.h"
#include "webui.h"
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>

/* actions.c */
void actions_cleanup_threads(void);

/* Global config pointer for VPS, GitHub, etc. */
const proxy_config_t *g_config = NULL;

static void signal_handler(int sig)
{
    (void)sig;
    fprintf(stderr, "\nShutting down...\n");
    server_stop();
}

/* Open dashboard in Chrome --app mode (runs in detached thread) */
static void *app_window_thread(void *arg)
{
    const proxy_config_t *cfg = (const proxy_config_t *)arg;
    char url[256];
    snprintf(url, sizeof(url), "http://127.0.0.1:%d/dashboard",
             cfg->listen_port);
    platform_open_app_window(url);
    return NULL;
}

int main(int argc, char **argv)
{
    /* Platform-specific init (Winsock on Windows, etc.) */
    if (platform_init() != 0) {
        fprintf(stderr, "Platform initialization failed.\n");
        return 1;
    }

    proxy_config_t cfg;

    if (config_load(&cfg, argc, argv) != 0) {
        fprintf(stderr, "Failed to load configuration.\n");
        platform_cleanup();
        return 1;
    }

    g_config = &cfg;

    if (cfg.verbose)
        config_print(&cfg);

    /* Initialize database */
    if (db_init("proxy.db") != 0) {
        fprintf(stderr, "Failed to initialize database.\n");
        platform_cleanup();
        return 1;
    }

    /* Initialize projects schema */
    if (db_project_init_schema() != 0) {
        fprintf(stderr, "Failed to initialize projects schema.\n");
        db_close();
        platform_cleanup();
        return 1;
    }

    /* Initialize response state schema */
    if (resp_state_init_schema() != 0) {
        fprintf(stderr, "Failed to initialize response state schema.\n");
        db_close();
        platform_cleanup();
        return 1;
    }

    /* Initialize thread state schema */
    if (thread_state_init_schema() != 0) {
        fprintf(stderr, "Failed to initialize thread state schema.\n");
        db_close();
        platform_cleanup();
        return 1;
    }

    /* Initialize upstream client (needed for login validation too) */
    if (upstream_init(&cfg) != 0) {
        fprintf(stderr, "Failed to initialize upstream client.\n");
        db_close();
        platform_cleanup();
        return 1;
    }

    /* Handle --login flow */
    if (cfg.do_login) {
        int ret = auth_browser_login(&cfg);
        if (ret != 0) {
            fprintf(stderr, "Login failed.\n");
            upstream_cleanup();
            platform_cleanup();
            return 1;
        }
        /* Re-init upstream with new cookie */
        upstream_cleanup();
        if (upstream_init(&cfg) != 0) {
            fprintf(stderr, "Failed to re-initialize after login.\n");
            platform_cleanup();
            return 1;
        }
    }

    /* Auto-grab cookie from browser if none configured */
    if (!cfg.session_cookie[0]) {
        fprintf(stderr, "No session cookie configured, "
                "checking browser databases...\n");
        int grabbed = auth_browser_login(&cfg);
        if (grabbed == 0) {
            upstream_cleanup();
            if (upstream_init(&cfg) != 0) {
                fprintf(stderr, "Failed to re-initialize after auto-login.\n");
                db_close();
                platform_cleanup();
                return 1;
            }
        }
    }

    /* Check if we have a cookie */
    if (!cfg.session_cookie[0]) {
        fprintf(stderr,
            "WARNING: No session cookie configured.\n"
            "Run with --login to authenticate, or set "
            "UVA_SESSION_COOKIE in proxy.env\n\n");
    } else {
        /* Validate session */
        if (!auth_validate_session(&cfg)) {
            fprintf(stderr,
                "WARNING: Session may be expired. "
                "Run with --login to re-authenticate.\n\n");
        }
        /* Start session refresh thread */
        auth_start_refresh(&cfg, 600);
    }

    /* Set up signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
#ifndef PLATFORM_WINDOWS
    signal(SIGPIPE, SIG_IGN);
#endif

    fprintf(stderr,
        "\n"
        "uva-proxy started\n"
        "  Endpoint:   http://127.0.0.1:%d/v1/chat/completions\n"
        "  Responses:  http://127.0.0.1:%d/v1/responses\n"
        "  Models:     http://127.0.0.1:%d/v1/models\n"
        "  Health:     http://127.0.0.1:%d/health\n"
        "  Dashboard:  http://127.0.0.1:%d/dashboard\n"
        "\n"
        "Configure your OpenAI client:\n"
        "  Base URL: http://127.0.0.1:%d/v1\n"
        "  API Key:  any-value (or uva-sk-xxx from dashboard)\n"
        "  Model:    %s\n"
        "\n",
        cfg.listen_port, cfg.listen_port, cfg.listen_port,
        cfg.listen_port, cfg.listen_port, cfg.listen_port,
        cfg.default_model);

    /* Bind + listen (non-blocking) */
    if (server_listen(&cfg) != 0) {
        upstream_cleanup();
        db_close();
        platform_cleanup();
        return 1;
    }

    /* Launch Open WebUI unless disabled */
    if (cfg.webui_enabled) {
        if (webui_start(&cfg) == 0)
            fprintf(stderr, "  Chat UI:    http://127.0.0.1:%d\n\n",
                    cfg.webui_port);
        else
            fprintf(stderr, "  Chat UI:    unavailable (setup failed)\n\n");
    }

    /* Launch app-mode window unless --headless */
    if (!cfg.headless) {
        pthread_t tid;
        if (pthread_create(&tid, NULL, app_window_thread,
                           (void *)&cfg) == 0) {
            pthread_detach(tid);
        }
    }

    /* Accept loop (blocks until shutdown) */
    int ret = server_accept_loop();

    /* Cleanup */
    fprintf(stderr, "Cleaning up...\n");
    webui_stop();
    auth_stop_refresh();
    actions_cleanup_threads();
    upstream_cleanup();
    db_close();
    platform_cleanup();

    return ret;
}
