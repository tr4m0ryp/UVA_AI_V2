#ifdef PLATFORM_MACOS

#include "core/platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

/* --- Lifecycle --- */

int platform_init(void)
{
    return 0;
}

void platform_cleanup(void)
{
}

/* --- Paths --- */

const char *platform_home_dir(void)
{
    const char *home = getenv("HOME");
    return home ? home : "/tmp";
}

const char *platform_config_dir(void)
{
    static char buf[PLATFORM_PATH_MAX];
    static int done = 0;
    if (done) return buf;

    const char *home = platform_home_dir();
    snprintf(buf, sizeof(buf),
             "%s/Library/Application Support/uva-proxy", home);
    platform_mkdir_p(buf);
    done = 1;
    return buf;
}

const char *platform_temp_dir(void)
{
    const char *tmp = getenv("TMPDIR");
    return tmp ? tmp : "/tmp";
}

/* --- Browser --- */

static const char *chrome_app_paths[] = {
    "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome",
    "/Applications/Chromium.app/Contents/MacOS/Chromium",
    "/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge",
    "/Applications/Brave Browser.app/Contents/MacOS/Brave Browser",
    NULL
};

const char *platform_find_chrome(void)
{
    static char found[PLATFORM_PATH_MAX];
    static int searched = 0;
    if (searched) return found[0] ? found : NULL;
    searched = 1;
    found[0] = '\0';

    for (int i = 0; chrome_app_paths[i]; i++) {
        if (access(chrome_app_paths[i], X_OK) == 0) {
            snprintf(found, sizeof(found), "%s", chrome_app_paths[i]);
            return found;
        }
    }
    return NULL;
}

int platform_open_url(const char *url)
{
    if (!url) return -1;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "open '%s'", url);
    return system(cmd) == 0 ? 0 : -1;
}

int platform_open_app_window(const char *url)
{
    const char *chrome = platform_find_chrome();
    if (!chrome) {
        fprintf(stderr, "No Chrome/Chromium/Edge found, "
                "opening in default browser\n");
        return platform_open_url(url);
    }

    char profile_dir[PLATFORM_PATH_MAX];
    snprintf(profile_dir, sizeof(profile_dir), "%s/browser",
             platform_config_dir());
    platform_mkdir_p(profile_dir);

    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "'%s' --app='%s' --user-data-dir='%s' "
             "--no-first-run --disable-default-apps "
             ">/dev/null 2>&1 &",
             chrome, url, profile_dir);
    return system(cmd) == 0 ? 0 : -1;
}

/* --- Network --- */

int platform_close_socket(int fd)
{
    return close(fd);
}

int platform_send(int fd, const char *data, size_t len)
{
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, data + sent, len - sent, 0);
        if (n <= 0) return -1;
        sent += (size_t)n;
    }
    return 0;
}

/* --- Misc --- */

void platform_sleep_ms(unsigned int ms)
{
    usleep(ms * 1000u);
}

#endif /* PLATFORM_MACOS */
