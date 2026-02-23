#include "webui/webui_internal.h"
#include "platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef PLATFORM_WINDOWS
  #include <windows.h>
#else
  #include <unistd.h>
  #include <signal.h>
  #include <sys/wait.h>
#endif

/* Build the uvicorn command line */
static void build_command(char *cmd, size_t cmd_size)
{
#ifdef PLATFORM_WINDOWS
    snprintf(cmd, cmd_size,
             "\"%s\\.venv\\Scripts\\python.exe\" -m uvicorn "
             "open_webui.main:app --host 127.0.0.1 --port %d "
             "--forwarded-allow-ips '*'",
             g_webui_dir, g_webui_port);
#else
    snprintf(cmd, cmd_size,
             "\"%s/.venv/bin/python3\" -m uvicorn "
             "open_webui.main:app --host 127.0.0.1 --port %d "
             "--forwarded-allow-ips '*'",
             g_webui_dir, g_webui_port);
#endif
}

/* Set environment variables for the child process */
static void set_child_env(void)
{
    char data_dir[WEBUI_BUF_MAX];
    char api_base[128];

    snprintf(data_dir, sizeof(data_dir), "%s/backend/data", g_webui_dir);
    snprintf(api_base, sizeof(api_base),
             "http://127.0.0.1:%d/v1", g_webui_proxy_port);

    /* Title generation prompt: no emojis, professional academic tone */
    static const char *title_prompt =
        "### Task:\n"
        "Generate a concise, 3-5 word title summarizing the chat history.\n"
        "### Guidelines:\n"
        "- The title must clearly represent the main theme or subject.\n"
        "- Do NOT use emojis. Use plain text only.\n"
        "- Do NOT use quotation marks or special formatting.\n"
        "- Write the title in the chat's primary language; "
        "default to English if multilingual.\n"
        "- Keep it clear, professional, and accurate.\n"
        "- Your entire response must be a single raw JSON object.\n"
        "### Output:\n"
        "JSON format: { \"title\": \"your concise title here\" }\n"
        "### Examples:\n"
        "- { \"title\": \"Stock Market Analysis\" }\n"
        "- { \"title\": \"Organic Chemistry Mechanisms\" }\n"
        "- { \"title\": \"Linear Algebra Eigenvalues\" }\n"
        "- { \"title\": \"Dutch Colonial History\" }\n"
        "- { \"title\": \"Machine Learning Classification\" }\n"
        "### Chat History:\n"
        "<chat_history>\n"
        "{{MESSAGES:END:2}}\n"
        "</chat_history>";

    /* Helper: pairs of (key, value) to set */
    const char *env_pairs[][2] = {
        /* Core */
        {"DATA_DIR",            data_dir},
        {"OPENAI_API_BASE_URL", api_base},
        {"OPENAI_API_KEY",      "uva-local"},
        {"WEBUI_AUTH",          "false"},

        /* Web search: DuckDuckGo (no API key required) */
        {"ENABLE_WEB_SEARCH",   "True"},
        {"WEB_SEARCH_ENGINE",   "duckduckgo"},
        {"WEB_SEARCH_RESULT_COUNT", "5"},
        {"BYPASS_WEB_SEARCH_EMBEDDING_AND_RETRIEVAL", "True"},

        /* Code interpreter (Pyodide, runs in-browser) */
        {"ENABLE_CODE_INTERPRETER", "True"},

        /* Search query auto-generation */
        {"ENABLE_SEARCH_QUERY_GENERATION", "True"},

        /* Title generation: no-emoji prompt */
        {"TITLE_GENERATION_PROMPT_TEMPLATE", title_prompt},

        /* Disable chat controls (advanced params, valves, etc.) */
        {"USER_PERMISSIONS_CHAT_CONTROLS",     "False"},
        {"USER_PERMISSIONS_CHAT_VALVES",       "False"},
        {"USER_PERMISSIONS_CHAT_PARAMS",       "False"},

        /* Default model for all users */
        {"DEFAULT_MODELS", "gpt-5.1"},

        {NULL, NULL}
    };

    for (int i = 0; env_pairs[i][0]; i++) {
#ifdef PLATFORM_WINDOWS
        SetEnvironmentVariableA(env_pairs[i][0], env_pairs[i][1]);
#else
        setenv(env_pairs[i][0], env_pairs[i][1], 1);
#endif
    }
}

int webui_spawn_child(void)
{
    char cmd[WEBUI_PATH_MAX + 256];
    build_command(cmd, sizeof(cmd));

    set_child_env();

#ifdef PLATFORM_WINDOWS
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));

    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE,
                        CREATE_NO_WINDOW, NULL, g_webui_dir, &si, &pi)) {
        fprintf(stderr, "[webui] Failed to start uvicorn (error %lu)\n",
                GetLastError());
        return -1;
    }
    CloseHandle(pi.hThread);
    g_webui_child_handle = pi.hProcess;
#else
    pid_t pid = fork();
    if (pid < 0) {
        perror("[webui] fork");
        return -1;
    }
    if (pid == 0) {
        if (chdir(g_webui_dir) != 0) _exit(1);

        char python_path[WEBUI_BUF_MAX];
        snprintf(python_path, sizeof(python_path),
                 "%s/.venv/bin/python3", g_webui_dir);

        char port_str[16];
        snprintf(port_str, sizeof(port_str), "%d", g_webui_port);

        char *argv[] = {
            python_path, "-m", "uvicorn",
            "open_webui.main:app",
            "--host", "127.0.0.1",
            "--port", port_str,
            "--forwarded-allow-ips", "*",
            NULL
        };
        execvp(python_path, argv);
        perror("[webui] execvp");
        _exit(1);
    }
    g_webui_child_pid = pid;
#endif

    return 0;
}

void webui_kill_child(void)
{
#ifdef PLATFORM_WINDOWS
    if (g_webui_child_handle) {
        TerminateProcess(g_webui_child_handle, 1);
        WaitForSingleObject(g_webui_child_handle, 3000);
        CloseHandle(g_webui_child_handle);
        g_webui_child_handle = NULL;
    }
#else
    if (g_webui_child_pid > 0) {
        kill(g_webui_child_pid, SIGTERM);
        int status;
        int waited = 0;
        while (waitpid(g_webui_child_pid, &status, WNOHANG) == 0
               && waited < 3000) {
            platform_sleep_ms(100);
            waited += 100;
        }
        if (waited >= 3000) {
            kill(g_webui_child_pid, SIGKILL);
            waitpid(g_webui_child_pid, &status, 0);
        }
        g_webui_child_pid = 0;
    }
#endif
}

int webui_child_alive(void)
{
#ifdef PLATFORM_WINDOWS
    if (!g_webui_child_handle) return 0;
    DWORD code;
    if (GetExitCodeProcess(g_webui_child_handle, &code))
        return (code == STILL_ACTIVE) ? 1 : 0;
    return 0;
#else
    if (g_webui_child_pid <= 0) return 0;
    int status;
    pid_t ret = waitpid(g_webui_child_pid, &status, WNOHANG);
    (void)ret;
    if (ret == 0) return 1;  /* still running */
    g_webui_child_pid = 0;
    return 0;
#endif
}
