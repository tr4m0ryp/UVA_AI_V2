#include "platform/platform.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

int platform_path_join(char *out, size_t out_size,
                       const char *dir, const char *file)
{
    if (!out || !dir || !file || out_size == 0)
        return -1;

#ifdef PLATFORM_WINDOWS
    const char sep = '\\';
#else
    const char sep = '/';
#endif

    size_t dlen = strlen(dir);
    /* Strip trailing separator from dir */
    while (dlen > 0 && (dir[dlen - 1] == '/' || dir[dlen - 1] == '\\'))
        dlen--;

    int n = snprintf(out, out_size, "%.*s%c%s", (int)dlen, dir, sep, file);
    if (n < 0 || (size_t)n >= out_size)
        return -1;
    return 0;
}

int platform_mkdir_p(const char *path)
{
    if (!path || !path[0])
        return -1;

    char tmp[PLATFORM_PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s", path);

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            *p = '\0';
#ifdef PLATFORM_WINDOWS
            CreateDirectoryA(tmp, NULL);
#else
            mkdir(tmp, 0755);
#endif
            *p = '/';
        }
    }

#ifdef PLATFORM_WINDOWS
    return CreateDirectoryA(tmp, NULL) || GetLastError() == ERROR_ALREADY_EXISTS
           ? 0 : -1;
#else
    return (mkdir(tmp, 0755) == 0 || errno == EEXIST) ? 0 : -1;
#endif
}
