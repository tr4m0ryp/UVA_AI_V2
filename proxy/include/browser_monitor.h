#ifndef UVA_PROXY_BROWSER_MONITOR_H
#define UVA_PROXY_BROWSER_MONITOR_H

#include "config.h"
#include <stddef.h>

/* Opaque handle for a browser login session. */
typedef struct browser_login_session browser_login_session_t;

/* Status codes returned by browser_login_poll(). */
#define BROWSER_LOGIN_PENDING   0
#define BROWSER_LOGIN_SUCCESS   1
#define BROWSER_LOGIN_ERROR    -1
#define BROWSER_LOGIN_TIMEOUT  -2

/* Start a browser login session:
 * - Opens the login URL in the default browser via xdg-open
 * - Spawns a background thread that monitors Firefox/Chrome cookie databases
 * - Returns an opaque session handle, or NULL on failure.
 * The caller must eventually call browser_login_cancel() to free resources. */
browser_login_session_t *browser_login_start(const char *login_url);

/* Poll the current status of a browser login session.
 * Returns BROWSER_LOGIN_PENDING, BROWSER_LOGIN_SUCCESS, or an error code.
 * On success, cookie_out/email_out/name_out are populated. */
int browser_login_poll(browser_login_session_t *session,
                       char *cookie_out, size_t cookie_size,
                       char *email_out, size_t email_size,
                       char *name_out, size_t name_size);

/* Cancel and free a browser login session.
 * Safe to call even if the session already completed. */
void browser_login_cancel(browser_login_session_t *session);

#endif /* UVA_PROXY_BROWSER_MONITOR_H */
