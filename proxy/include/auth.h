#ifndef UVA_PROXY_AUTH_H
#define UVA_PROXY_AUTH_H

#include "config.h"

/* Run the browser-based login flow:
 * 1. Start temporary HTTP server on a random port
 * 2. Open browser to aichat.uva.nl
 * 3. Print instructions for the user to send cookies
 * 4. Receive cookies and save to config
 * Returns 0 on success, -1 on error. */
int auth_browser_login(proxy_config_t *cfg);

/* Validate the stored session cookie by calling /api/auth/session.
 * Returns 1 if valid, 0 if expired/invalid. */
int auth_validate_session(const proxy_config_t *cfg);

/* Start a background thread that periodically refreshes the session.
 * interval_sec is how often to call /api/auth/session (e.g. 600 = 10 min). */
int auth_start_refresh(const proxy_config_t *cfg, int interval_sec);

/* Stop the session refresh thread. */
void auth_stop_refresh(void);

#endif /* UVA_PROXY_AUTH_H */
