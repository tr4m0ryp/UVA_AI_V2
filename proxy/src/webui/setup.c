#include "webui/webui.h"
#include "platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* Find a working python3 binary on PATH. Returns static buffer or NULL. */
static const char *find_python(void)
{
    static char path[PLATFORM_PATH_MAX];
    const char *candidates[] = { "python3.12", "python3.11", "python3",
                                  "python", NULL };

    for (int i = 0; candidates[i]; i++) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd),
                 "command -v %s >/dev/null 2>&1", candidates[i]);
        if (system(cmd) == 0) {
            strncpy(path, candidates[i], sizeof(path) - 1);
            return path;
        }
    }
    return NULL;
}

/* Find uv package manager. Returns static buffer or NULL. */
static const char *find_uv(void)
{
    if (system("command -v uv >/dev/null 2>&1") == 0)
        return "uv";
    return NULL;
}

/* Check if a file exists. */
static int file_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

/* Ensure venv exists at webui_dir/.venv */
static int ensure_venv(const char *webui_dir, const char *python)
{
    char venv_python[PLATFORM_PATH_MAX];
#ifdef PLATFORM_WINDOWS
    snprintf(venv_python, sizeof(venv_python),
             "%s\\.venv\\Scripts\\python.exe", webui_dir);
#else
    snprintf(venv_python, sizeof(venv_python),
             "%s/.venv/bin/python3", webui_dir);
#endif

    if (file_exists(venv_python)) return 0;

    fprintf(stderr, "[webui] Creating Python virtual environment...\n");
    char cmd[PLATFORM_PATH_MAX + 128];
    snprintf(cmd, sizeof(cmd), "%s -m venv \"%s/.venv\"", python, webui_dir);
    if (system(cmd) != 0) {
        fprintf(stderr, "[webui] Failed to create venv.\n");
        return -1;
    }
    return 0;
}

/* Ensure packages are installed (check import open_webui) */
static int ensure_packages(const char *webui_dir, const char *uv)
{
    char check[PLATFORM_PATH_MAX + 128];
#ifdef PLATFORM_WINDOWS
    snprintf(check, sizeof(check),
             "\"%s\\.venv\\Scripts\\python.exe\" -c "
             "\"import open_webui\" >NUL 2>&1", webui_dir);
#else
    snprintf(check, sizeof(check),
             "\"%s/.venv/bin/python3\" -c "
             "\"import open_webui\" >/dev/null 2>&1", webui_dir);
#endif

    if (system(check) == 0) return 0;

    fprintf(stderr, "[webui] Installing Open WebUI packages...\n");
    char cmd[PLATFORM_PATH_MAX + 256];

    if (uv) {
        snprintf(cmd, sizeof(cmd),
                 "cd \"%s\" && uv pip install -e . "
                 "--python .venv/bin/python3 2>&1", webui_dir);
    } else {
#ifdef PLATFORM_WINDOWS
        snprintf(cmd, sizeof(cmd),
                 "cd /d \"%s\" && .venv\\Scripts\\pip install -e . 2>&1",
                 webui_dir);
#else
        snprintf(cmd, sizeof(cmd),
                 "cd \"%s\" && .venv/bin/pip install -e . 2>&1", webui_dir);
#endif
    }

    if (system(cmd) != 0) {
        fprintf(stderr, "[webui] Package installation failed.\n");
        return -1;
    }
    return 0;
}

/* Extra pip packages needed for integrations (not in open-webui defaults) */
static const char *g_extra_packages[] = {
    "duckduckgo-search",   /* web search via DuckDuckGo (no API key) */
    NULL
};

/* Install extra packages that are not bundled with open-webui */
static int ensure_extras(const char *webui_dir, const char *uv)
{
    char check[PLATFORM_PATH_MAX + 256];
    char cmd[PLATFORM_PATH_MAX + 512];

    for (int i = 0; g_extra_packages[i]; i++) {
        /* Quick import check -- map package name to importable module */
        const char *pkg = g_extra_packages[i];
        char module[128];
        /* duckduckgo-search -> ddgs */
        if (strcmp(pkg, "duckduckgo-search") == 0)
            strncpy(module, "ddgs", sizeof(module) - 1);
        else
            snprintf(module, sizeof(module), "%s", pkg);

#ifdef PLATFORM_WINDOWS
        snprintf(check, sizeof(check),
                 "\"%s\\.venv\\Scripts\\python.exe\" -c "
                 "\"import %s\" >NUL 2>&1", webui_dir, module);
#else
        snprintf(check, sizeof(check),
                 "\"%s/.venv/bin/python3\" -c "
                 "\"import %s\" >/dev/null 2>&1", webui_dir, module);
#endif
        if (system(check) == 0) continue;

        fprintf(stderr, "[webui] Installing %s...\n", pkg);
        if (uv) {
            snprintf(cmd, sizeof(cmd),
                     "uv pip install %s "
                     "--python \"%s/.venv/bin/python3\" 2>&1", pkg, webui_dir);
        } else {
#ifdef PLATFORM_WINDOWS
            snprintf(cmd, sizeof(cmd),
                     "\"%s\\.venv\\Scripts\\pip\" install %s 2>&1",
                     webui_dir, pkg);
#else
            snprintf(cmd, sizeof(cmd),
                     "\"%s/.venv/bin/pip\" install %s 2>&1",
                     webui_dir, pkg);
#endif
        }
        if (system(cmd) != 0) {
            fprintf(stderr, "[webui] Warning: failed to install %s "
                    "(web search may not work)\n", pkg);
            /* Non-fatal: continue without this integration */
        }
    }
    return 0;
}

/* Ensure frontend is built (build/index.html exists) */
static int ensure_frontend(const char *webui_dir)
{
    char index[PLATFORM_PATH_MAX];
    snprintf(index, sizeof(index), "%s/build/index.html", webui_dir);

    if (file_exists(index)) return 0;

    fprintf(stderr, "[webui] Frontend not built. "
            "Run 'npm run build' in vendor/open-webui/ first.\n");
    return -1;
}

int webui_ensure_setup(const char *webui_dir)
{
    /* Check that the directory exists at all */
    if (!file_exists(webui_dir)) {
        fprintf(stderr, "[webui] Open WebUI directory not found: %s\n",
                webui_dir);
        fprintf(stderr, "[webui] Run: git submodule update --init\n");
        return -1;
    }

    const char *python = find_python();
    if (!python) {
        fprintf(stderr, "[webui] Python 3 not found on PATH. "
                "Install Python 3.11+ to use the chat interface.\n");
        return -1;
    }

    if (ensure_venv(webui_dir, python) != 0) return -1;

    const char *uv = find_uv();
    if (ensure_packages(webui_dir, uv) != 0) return -1;
    if (ensure_extras(webui_dir, uv) != 0) return -1;
    if (ensure_frontend(webui_dir) != 0) return -1;

    fprintf(stderr, "[webui] Setup complete.\n");
    return 0;
}
