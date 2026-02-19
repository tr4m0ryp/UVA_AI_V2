#ifdef PLATFORM_WINDOWS

#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
#include <shlobj.h>

/* --- Lifecycle --- */

int platform_init(void)
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return -1;
    return 0;
}

void platform_cleanup(void)
{
    WSACleanup();
}

/* --- Paths --- */

const char *platform_home_dir(void)
{
    static char buf[PLATFORM_PATH_MAX];
    static int done = 0;
    if (done) return buf;

    const char *profile = getenv("USERPROFILE");
    if (profile) {
        snprintf(buf, sizeof(buf), "%s", profile);
    } else {
        snprintf(buf, sizeof(buf), "C:\\Users\\Default");
    }
    done = 1;
    return buf;
}

const char *platform_config_dir(void)
{
    static char buf[PLATFORM_PATH_MAX];
    static int done = 0;
    if (done) return buf;

    const char *appdata = getenv("APPDATA");
    if (appdata) {
        snprintf(buf, sizeof(buf), "%s\\uva-proxy", appdata);
    } else {
        snprintf(buf, sizeof(buf), "%s\\AppData\\Roaming\\uva-proxy",
                 platform_home_dir());
    }
    platform_mkdir_p(buf);
    done = 1;
    return buf;
}

const char *platform_temp_dir(void)
{
    static char buf[PLATFORM_PATH_MAX];
    static int done = 0;
    if (done) return buf;

    const char *tmp = getenv("TEMP");
    if (!tmp) tmp = getenv("TMP");
    if (tmp) {
        snprintf(buf, sizeof(buf), "%s", tmp);
    } else {
        snprintf(buf, sizeof(buf), "C:\\Windows\\Temp");
    }
    done = 1;
    return buf;
}

/* --- Browser --- */

static const char *chrome_registry_paths[] = {
    "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\chrome.exe",
    "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\msedge.exe",
    NULL
};

static const char *chrome_common_paths[] = {
    "C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe",
    "C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe",
    "C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe",
    "C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe",
    "C:\\Program Files\\BraveSoftware\\Brave-Browser\\Application\\brave.exe",
    NULL
};

const char *platform_find_chrome(void)
{
    static char found[PLATFORM_PATH_MAX];
    static int searched = 0;
    if (searched) return found[0] ? found : NULL;
    searched = 1;
    found[0] = '\0';

    /* Try registry first */
    for (int i = 0; chrome_registry_paths[i]; i++) {
        HKEY hkey;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, chrome_registry_paths[i],
                          0, KEY_READ, &hkey) == ERROR_SUCCESS) {
            DWORD size = sizeof(found);
            if (RegQueryValueExA(hkey, NULL, NULL, NULL,
                                 (LPBYTE)found, &size) == ERROR_SUCCESS) {
                RegCloseKey(hkey);
                if (found[0]) return found;
            }
            RegCloseKey(hkey);
        }
    }

    /* Try LocalAppData paths */
    const char *local = getenv("LOCALAPPDATA");
    if (local) {
        char path[PLATFORM_PATH_MAX];
        snprintf(path, sizeof(path),
                 "%s\\Google\\Chrome\\Application\\chrome.exe", local);
        if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) {
            snprintf(found, sizeof(found), "%s", path);
            return found;
        }
        snprintf(path, sizeof(path),
                 "%s\\Microsoft\\Edge\\Application\\msedge.exe", local);
        if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) {
            snprintf(found, sizeof(found), "%s", path);
            return found;
        }
    }

    /* Try common hardcoded paths */
    for (int i = 0; chrome_common_paths[i]; i++) {
        if (GetFileAttributesA(chrome_common_paths[i]) !=
            INVALID_FILE_ATTRIBUTES) {
            snprintf(found, sizeof(found), "%s", chrome_common_paths[i]);
            return found;
        }
    }

    return NULL;
}

int platform_open_url(const char *url)
{
    if (!url) return -1;
    HINSTANCE r = ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
    return (intptr_t)r > 32 ? 0 : -1;
}

int platform_open_app_window(const char *url)
{
    const char *chrome = platform_find_chrome();
    if (!chrome) {
        fprintf(stderr, "No Chrome/Edge found, opening in default browser\n");
        return platform_open_url(url);
    }

    char profile_dir[PLATFORM_PATH_MAX];
    snprintf(profile_dir, sizeof(profile_dir), "%s\\browser",
             platform_config_dir());
    platform_mkdir_p(profile_dir);

    char args[2048];
    snprintf(args, sizeof(args),
             "--app=\"%s\" --user-data-dir=\"%s\" "
             "--no-first-run --disable-default-apps",
             url, profile_dir);

    HINSTANCE r = ShellExecuteA(NULL, "open", chrome, args,
                                NULL, SW_SHOWNORMAL);
    return (intptr_t)r > 32 ? 0 : -1;
}

/* --- Network --- */

int platform_close_socket(int fd)
{
    return closesocket((SOCKET)fd);
}

int platform_send(int fd, const char *data, size_t len)
{
    size_t sent = 0;
    while (sent < len) {
        int n = send((SOCKET)fd, data + sent, (int)(len - sent), 0);
        if (n <= 0) return -1;
        sent += (size_t)n;
    }
    return 0;
}

/* --- Misc --- */

void platform_sleep_ms(unsigned int ms)
{
    Sleep(ms);
}

#endif /* PLATFORM_WINDOWS */
