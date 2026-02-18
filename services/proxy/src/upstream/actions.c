#include "upstream.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static char *generate_thread_id(void)
{
    /*
     * Generate a UUID v4 format thread ID: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
     * The UvA server requires proper UUID format; non-UUID strings cause HTTP 500.
     * Each request gets a fresh ID because the server rejects reused thread IDs.
     */
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    /* Mix in process entropy for each call */
    unsigned int seed = (unsigned)ts.tv_nsec ^ (unsigned)ts.tv_sec
                        ^ (unsigned)rand();
    srand(seed);

    static const char hex[] = "0123456789abcdef";
    char id[37];
    int pos = 0;

    /* xxxxxxxx - 4 */
    for (int i = 0; i < 8; i++) id[pos++] = hex[rand() & 0xf];
    id[pos++] = '-';
    /* xxxx */
    for (int i = 0; i < 4; i++) id[pos++] = hex[rand() & 0xf];
    id[pos++] = '-';
    /* 4xxx - version 4 */
    id[pos++] = '4';
    for (int i = 0; i < 3; i++) id[pos++] = hex[rand() & 0xf];
    id[pos++] = '-';
    /* yxxx - variant bits 10xx */
    id[pos++] = hex[8 + (rand() & 0x3)]; /* 8, 9, a, or b */
    for (int i = 0; i < 3; i++) id[pos++] = hex[rand() & 0xf];
    id[pos++] = '-';
    /* xxxxxxxxxxxx */
    for (int i = 0; i < 12; i++) id[pos++] = hex[rand() & 0xf];
    id[pos] = '\0';

    return strdup(id);
}

char *actions_get_or_create_thread(void)
{
    return generate_thread_id();
}

int actions_delete_thread(const char *thread_id)
{
    (void)thread_id;
    return 0;
}

void actions_cleanup_threads(void)
{
}
