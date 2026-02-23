#include "auth/auth.h"
#include "auth/auth_internal.h"
#include "upstream/upstream.h"
#include "config/config.h"
#include "platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>

#define PATH_MAX_LEN 512
#define CMD_MAX_LEN  1200

/* Firefox profile directory patterns per platform */
static const char *firefox_patterns[] = {
#ifdef PLATFORM_LINUX
    "%s/snap/firefox/common/.mozilla/firefox",
    "%s/.mozilla/firefox",
#elif defined(PLATFORM_MACOS)
    "%s/Library/Application Support/Firefox/Profiles",
    "%s/Library/Application Support/Firefox",
#endif
    NULL
};

/*
 * Find the Firefox cookies.sqlite (most recently modified profile).
 */
int find_firefox_cookies(char *out, size_t out_size)
{
    const char *home = platform_home_dir();
    if (!home) return -1;

    char base[PATH_MAX_LEN];
    int found = 0;
    time_t newest = 0;

    for (int p = 0; firefox_patterns[p]; p++) {
        snprintf(base, sizeof(base), firefox_patterns[p], home);
        DIR *dir = opendir(base);
        if (!dir) continue;

        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            if (ent->d_name[0] == '.') continue;
            char path[PATH_MAX_LEN];
            snprintf(path, sizeof(path), "%s/%s/cookies.sqlite",
                     base, ent->d_name);
            struct stat st;
            if (stat(path, &st) == 0 && st.st_mtime > newest) {
                newest = st.st_mtime;
                snprintf(out, out_size, "%s", path);
                found = 1;
            }
        }
        closedir(dir);
    }
    return found ? 0 : -1;
}

/*
 * Extract cookies for aichat.uva.nl from Firefox cookies.sqlite.
 * Copies the DB first since Firefox holds a lock on it.
 */
int extract_firefox_cookies(const char *db_path, char *out,
                            size_t out_size)
{
    char tmp[PATH_MAX_LEN];
    snprintf(tmp, sizeof(tmp), "%s/uva_cookies_%d.sqlite",
             platform_temp_dir(), (int)getpid());

    /* Copy DB + WAL file */
    char cmd[CMD_MAX_LEN];
    snprintf(cmd, sizeof(cmd), "cp '%.400s' '%s'", db_path, tmp);
    system(cmd);

    char wal_src[PATH_MAX_LEN], wal_dst[PATH_MAX_LEN];
    snprintf(wal_src, sizeof(wal_src), "%.400s-wal", db_path);
    snprintf(wal_dst, sizeof(wal_dst), "%s-wal", tmp);
    snprintf(cmd, sizeof(cmd), "cp '%s' '%s' 2>/dev/null", wal_src, wal_dst);
    system(cmd);

    /* Query cookies -- try sqlite3 CLI first, fallback to python3 */
    snprintf(cmd, sizeof(cmd),
        "sqlite3 '%s' \"SELECT name || '=' || value FROM moz_cookies "
        "WHERE host LIKE '%%aichat.uva.nl%%' ORDER BY name;\" "
        "2>/dev/null || python3 -c \""
        "import sqlite3,sys; "
        "c=sqlite3.connect('%s'); "
        "r=c.execute(\\\"SELECT name||'='||value FROM moz_cookies "
        "WHERE host LIKE '%%aichat.uva.nl%%' ORDER BY name\\\"); "
        "[print(x[0]) for x in r]; c.close()\"", tmp, tmp);

    FILE *pipe = popen(cmd, "r");
    if (!pipe) { unlink(tmp); return -1; }

    out[0] = '\0';
    size_t offset = 0;
    char line[2048];

    while (fgets(line, sizeof(line), pipe)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';
        if (len == 0) continue;

        if (offset > 0 && offset + 2 < out_size)
            offset += (size_t)snprintf(out + offset, out_size - offset,
                                        "; ");
        if (offset < out_size)
            offset += (size_t)snprintf(out + offset, out_size - offset,
                                        "%s", line);
    }
    pclose(pipe);

    unlink(tmp);
    unlink(wal_dst);
    return offset > 0 ? 0 : -1;
}

/*
 * Try to extract a valid session cookie from browser databases.
 * Checks Firefox first, then Chrome/Chromium.
 * Returns 0 on success with cookie written to out, -1 if not found.
 */
static int try_extract_cookie(char *out, size_t out_size)
{
    /* Try Firefox */
    char db_path[PATH_MAX_LEN];
    if (find_firefox_cookies(db_path, sizeof(db_path)) == 0) {
        fprintf(stderr, "Found Firefox cookies: %s\n", db_path);
        if (extract_firefox_cookies(db_path, out, out_size) == 0) {
            if (strstr(out, "next-auth.session-token") ||
                strstr(out, "authjs.session-token") ||
                strstr(out, "__Secure-next-auth"))
                return 0;
        }
    }

    /* Try Chrome/Chromium */
    fprintf(stderr, "Trying Chrome/Chromium cookies...\n");
    char value[MAX_COOKIE_LEN - 64];
    const char *names[] = {
        "next-auth.session-token",
        "authjs.session-token",
        "__Secure-next-auth.session-token",
    };
    for (int i = 0; i < 3; i++) {
        if (chrome_extract_cookie("aichat.uva.nl", names[i],
                                   value, sizeof(value)) == 0 && value[0]) {
            snprintf(out, out_size, "%s=%s", names[i], value);
            return 0;
        }
    }

    return -1;
}

int auth_browser_login(proxy_config_t *cfg)
{
    fprintf(stderr, "\n=== UvA AI Chat Login ===\n\n");

    char cookie[MAX_COOKIE_LEN];
    if (try_extract_cookie(cookie, sizeof(cookie)) != 0) {
        fprintf(stderr,
            "No session token found in any browser.\n"
            "Make sure you are logged in to %s first.\n\n",
            cfg->base_url);
        goto manual;
    }

    /* Validate against upstream before saving */
    char email[256] = {0};
    char name[256] = {0};
    if (upstream_validate_cookie(cookie, email, sizeof(email),
                                  name, sizeof(name)) && email[0]) {
        fprintf(stderr, "Session valid for: %s (%s)\n", email, name);
    } else {
        fprintf(stderr,
            "Cookie found but session is expired or invalid.\n"
            "Please log in again at %s\n\n", cfg->base_url);
        goto manual;
    }

    snprintf(cfg->session_cookie, MAX_COOKIE_LEN, "%s", cookie);
    config_save_cookie(cfg);
    fprintf(stderr, "Session cookie extracted and saved to %s\n\n",
            CONFIG_FILE);
    return 0;

manual:
    fprintf(stderr,
        "Manual cookie extraction:\n\n"
        "1. Log in to: %s\n\n"
        "2. Open DevTools (F12) -> Storage -> Cookies\n\n"
        "3. Find 'next-auth.session-token' (or similar)\n\n"
        "4. Copy its value into proxy.env:\n"
        "   UVA_SESSION_COOKIE=\"next-auth.session-token=VALUE\"\n\n",
        cfg->base_url);
    return -1;
}
