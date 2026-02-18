#include "config.h"
#include "server.h"
#include "upstream.h"
#include "auth.h"
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

/* actions.c */
void actions_cleanup_threads(void);

static void signal_handler(int sig)
{
    (void)sig;
    fprintf(stderr, "\nShutting down...\n");
    server_stop();
}

int main(int argc, char **argv)
{
    proxy_config_t cfg;

    if (config_load(&cfg, argc, argv) != 0) {
        fprintf(stderr, "Failed to load configuration.\n");
        return 1;
    }

    if (cfg.verbose)
        config_print(&cfg);

    /* Initialize database */
    if (db_init("proxy.db") != 0) {
        fprintf(stderr, "Failed to initialize database.\n");
        return 1;
    }

    /* Initialize upstream client (needed for login validation too) */
    if (upstream_init(&cfg) != 0) {
        fprintf(stderr, "Failed to initialize upstream client.\n");
        db_close();
        return 1;
    }

    /* Handle --login flow */
    if (cfg.do_login) {
        int ret = auth_browser_login(&cfg);
        if (ret != 0) {
            fprintf(stderr, "Login failed.\n");
            upstream_cleanup();
            return 1;
        }
        /* Re-init upstream with new cookie */
        upstream_cleanup();
        if (upstream_init(&cfg) != 0) {
            fprintf(stderr, "Failed to re-initialize after login.\n");
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
    signal(SIGPIPE, SIG_IGN);

    fprintf(stderr,
        "\n"
        "uva-proxy started\n"
        "  Endpoint:  http://127.0.0.1:%d/v1/chat/completions\n"
        "  Models:    http://127.0.0.1:%d/v1/models\n"
        "  Health:    http://127.0.0.1:%d/health\n"
        "  Dashboard: http://127.0.0.1:%d/dashboard\n"
        "\n"
        "Configure your OpenAI client:\n"
        "  Base URL: http://127.0.0.1:%d/v1\n"
        "  API Key:  any-value (or uva-sk-xxx from dashboard)\n"
        "  Model:    %s\n"
        "\n",
        cfg.listen_port, cfg.listen_port, cfg.listen_port,
        cfg.listen_port, cfg.listen_port, cfg.default_model);

    /* Start server (blocks until shutdown) */
    int ret = server_start(&cfg);

    /* Cleanup */
    fprintf(stderr, "Cleaning up...\n");
    auth_stop_refresh();
    actions_cleanup_threads();
    upstream_cleanup();
    db_close();

    return ret;
}
