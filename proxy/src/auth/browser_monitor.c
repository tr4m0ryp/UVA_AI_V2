#include "browser_monitor.h"
#include "auth_internal.h"
#include "upstream.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <openssl/rand.h>

#define POLL_INTERVAL_SEC  2
#define MAX_POLL_ATTEMPTS  60   /* 60 * 2s = 120s timeout */
#define SESSION_ID_LEN     32

struct browser_login_session {
    char            session_id[SESSION_ID_LEN * 2 + 1];
    pthread_t       thread;
    pthread_mutex_t lock;
    volatile int    status;   /* BROWSER_LOGIN_PENDING/SUCCESS/ERROR/TIMEOUT */
    char            cookie[MAX_COOKIE_LEN];
    char            email[256];
    char            name[256];
    volatile int    cancelled;
    char            login_url[MAX_URL_LEN];
    int             thread_started;
};

/*
 * Try to extract a session token from Firefox cookies.
 * Builds a full cookie string and checks for session token presence.
 */
static int try_firefox(char *cookie_out, size_t cookie_size)
{
    char db_path[512];
    if (find_firefox_cookies(db_path, sizeof(db_path)) != 0)
        return -1;

    if (extract_firefox_cookies(db_path, cookie_out, cookie_size) != 0)
        return -1;

    /* Verify it contains a session token */
    if (!strstr(cookie_out, "next-auth.session-token") &&
        !strstr(cookie_out, "authjs.session-token") &&
        !strstr(cookie_out, "__Secure-next-auth"))
        return -1;

    return 0;
}

/*
 * Try to extract a session token from Chrome/Chromium cookies.
 * Builds a cookie string in "name=value" format.
 */
static int try_chrome(char *cookie_out, size_t cookie_size)
{
    /* Leave room for "name=" prefix (max 34 chars) */
    char value[MAX_COOKIE_LEN - 64];

    /* Try the standard cookie name first */
    if (chrome_extract_cookie("aichat.uva.nl",
                              "next-auth.session-token",
                              value, sizeof(value)) == 0 && value[0]) {
        snprintf(cookie_out, cookie_size,
                 "next-auth.session-token=%s", value);
        return 0;
    }

    /* Try alternative cookie names */
    if (chrome_extract_cookie("aichat.uva.nl",
                              "authjs.session-token",
                              value, sizeof(value)) == 0 && value[0]) {
        snprintf(cookie_out, cookie_size,
                 "authjs.session-token=%s", value);
        return 0;
    }

    if (chrome_extract_cookie("aichat.uva.nl",
                              "__Secure-next-auth.session-token",
                              value, sizeof(value)) == 0 && value[0]) {
        snprintf(cookie_out, cookie_size,
                 "__Secure-next-auth.session-token=%s", value);
        return 0;
    }

    return -1;
}

/*
 * Background thread: polls browser cookie databases every 2 seconds.
 * On finding a valid session token, validates it against UvA upstream.
 */
static void *monitor_thread(void *arg)
{
    browser_login_session_t *s = (browser_login_session_t *)arg;
    char cookie[MAX_COOKIE_LEN];
    char email[256];
    char name[256];

    for (int attempt = 0; attempt < MAX_POLL_ATTEMPTS; attempt++) {
        if (s->cancelled)
            break;

        sleep(POLL_INTERVAL_SEC);

        if (s->cancelled)
            break;

        cookie[0] = '\0';
        int found = 0;

        /* Try Firefox first, then Chrome */
        if (try_firefox(cookie, sizeof(cookie)) == 0)
            found = 1;
        else if (try_chrome(cookie, sizeof(cookie)) == 0)
            found = 1;

        if (!found)
            continue;

        /* Validate the cookie against UvA */
        email[0] = '\0';
        name[0] = '\0';
        int valid = upstream_validate_cookie(cookie, email, sizeof(email),
                                             name, sizeof(name));
        if (!valid || !email[0])
            continue;

        /* Success -- write results behind mutex */
        pthread_mutex_lock(&s->lock);
        snprintf(s->cookie, sizeof(s->cookie), "%s", cookie);
        snprintf(s->email, sizeof(s->email), "%s", email);
        snprintf(s->name, sizeof(s->name), "%s", name);
        s->status = BROWSER_LOGIN_SUCCESS;
        pthread_mutex_unlock(&s->lock);

        fprintf(stderr, "Browser login: detected session for %s\n", email);
        return NULL;
    }

    /* Timed out or cancelled */
    pthread_mutex_lock(&s->lock);
    if (s->status == BROWSER_LOGIN_PENDING) {
        s->status = s->cancelled ? BROWSER_LOGIN_ERROR : BROWSER_LOGIN_TIMEOUT;
    }
    pthread_mutex_unlock(&s->lock);

    return NULL;
}

browser_login_session_t *browser_login_start(const char *login_url)
{
    browser_login_session_t *s = calloc(1, sizeof(*s));
    if (!s) return NULL;

    /* Generate session ID */
    unsigned char raw[SESSION_ID_LEN / 2];
    if (RAND_bytes(raw, sizeof(raw)) != 1) {
        free(s);
        return NULL;
    }
    for (int i = 0; i < (int)sizeof(raw); i++)
        snprintf(s->session_id + i * 2, 3, "%02x", raw[i]);

    snprintf(s->login_url, sizeof(s->login_url), "%s", login_url);
    s->status = BROWSER_LOGIN_PENDING;
    s->cancelled = 0;
    s->thread_started = 0;
    pthread_mutex_init(&s->lock, NULL);

    /* Try to grab existing cookies before opening the browser.
     * If the user is already logged in, we skip the redirect entirely. */
    char cookie[MAX_COOKIE_LEN];
    int found = 0;

    if (try_firefox(cookie, sizeof(cookie)) == 0)
        found = 1;
    else if (try_chrome(cookie, sizeof(cookie)) == 0)
        found = 1;

    if (found) {
        char email[256] = {0};
        char name[256] = {0};
        if (upstream_validate_cookie(cookie, email, sizeof(email),
                                      name, sizeof(name)) && email[0]) {
            /* Already logged in -- return immediately without browser */
            snprintf(s->cookie, sizeof(s->cookie), "%s", cookie);
            snprintf(s->email, sizeof(s->email), "%s", email);
            snprintf(s->name, sizeof(s->name), "%s", name);
            s->status = BROWSER_LOGIN_SUCCESS;
            fprintf(stderr,
                "Browser login: reusing existing session for %s\n", email);
            return s;
        }
    }

    /* No valid cookie found -- open browser and start monitoring */
    char cmd[MAX_URL_LEN + 32];
    snprintf(cmd, sizeof(cmd), "xdg-open '%s' >/dev/null 2>&1 &", login_url);
    system(cmd);

    /* Start monitoring thread */
    if (pthread_create(&s->thread, NULL, monitor_thread, s) != 0) {
        pthread_mutex_destroy(&s->lock);
        free(s);
        return NULL;
    }
    s->thread_started = 1;

    fprintf(stderr, "Browser login session started: %s\n", s->session_id);
    return s;
}

int browser_login_poll(browser_login_session_t *session,
                       char *cookie_out, size_t cookie_size,
                       char *email_out, size_t email_size,
                       char *name_out, size_t name_size)
{
    if (!session) return BROWSER_LOGIN_ERROR;

    pthread_mutex_lock(&session->lock);
    int status = session->status;

    if (status == BROWSER_LOGIN_SUCCESS) {
        if (cookie_out)
            snprintf(cookie_out, cookie_size, "%s", session->cookie);
        if (email_out)
            snprintf(email_out, email_size, "%s", session->email);
        if (name_out)
            snprintf(name_out, name_size, "%s", session->name);
    }
    pthread_mutex_unlock(&session->lock);

    return status;
}

void browser_login_cancel(browser_login_session_t *session)
{
    if (!session) return;

    session->cancelled = 1;

    if (session->thread_started)
        pthread_join(session->thread, NULL);

    pthread_mutex_destroy(&session->lock);
    free(session);
}
