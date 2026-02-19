#ifndef UVA_PROXY_PLATFORM_H
#define UVA_PROXY_PLATFORM_H

/*
 * Cross-platform abstraction layer.
 * Only one of linux.c / macos.c / windows.c compiles per target.
 */

#ifdef PLATFORM_WINDOWS
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>
  #ifndef ssize_t
    #define ssize_t int
  #endif
  #define strcasecmp  _stricmp
  #define strncasecmp _strnicmp
#else
  #include <unistd.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
#endif

#include <stddef.h>

/* Max path buffer size used across platform calls */
#define PLATFORM_PATH_MAX 512

/* --- Lifecycle --- */
int  platform_init(void);
void platform_cleanup(void);

/* --- Paths --- */
const char *platform_home_dir(void);
const char *platform_config_dir(void);
const char *platform_temp_dir(void);

/* --- Browser --- */
int  platform_open_url(const char *url);
int  platform_open_app_window(const char *url);
const char *platform_find_chrome(void);

/* --- Network (Windows compat) --- */
int  platform_close_socket(int fd);
int  platform_send(int fd, const char *data, size_t len);

/* --- Misc --- */
void platform_sleep_ms(unsigned int ms);

/* --- Path helpers (common.c) --- */
int  platform_path_join(char *out, size_t out_size,
                        const char *dir, const char *file);
int  platform_mkdir_p(const char *path);

#endif /* UVA_PROXY_PLATFORM_H */
