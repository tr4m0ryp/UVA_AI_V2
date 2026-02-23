#ifndef UVA_PROXY_WEBUI_INTERNAL_H
#define UVA_PROXY_WEBUI_INTERNAL_H

/* Shared state and helpers between webui/launcher.c and webui/process.c */

#include "platform/platform.h"

#define WEBUI_PATH_MAX   1024
#define WEBUI_BUF_MAX    (WEBUI_PATH_MAX + 64)

#ifdef PLATFORM_WINDOWS
  #include <windows.h>
#else
  #include <sys/types.h>
#endif

/* Shared globals (defined in launcher.c) */
extern volatile int    g_webui_running;
extern int             g_webui_port;
extern int             g_webui_proxy_port;
extern char            g_webui_dir[WEBUI_PATH_MAX];

#ifdef PLATFORM_WINDOWS
extern HANDLE          g_webui_child_handle;
#else
extern volatile pid_t  g_webui_child_pid;
#endif

/* Process management (process.c) */
int  webui_spawn_child(void);
void webui_kill_child(void);
int  webui_child_alive(void);

#endif /* UVA_PROXY_WEBUI_INTERNAL_H */
