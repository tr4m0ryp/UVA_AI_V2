#include "utils/idgen.h"
#include <stdlib.h>
#include <time.h>
#include <stdint.h>

static const char CHARSET[] =
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789";

/* 62 characters in the charset */
#define CHARSET_LEN 62

void gen_nanoid(char *buf, int len)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    srand((unsigned)ts.tv_nsec ^ (unsigned)ts.tv_sec
          ^ (unsigned)(uintptr_t)buf);

    for (int i = 0; i < len; i++)
        buf[i] = CHARSET[rand() % CHARSET_LEN];
    buf[len] = '\0';
}
