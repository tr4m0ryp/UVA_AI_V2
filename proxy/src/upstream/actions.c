#include "upstream.h"
#include "idgen.h"
#include <stdlib.h>
#include <string.h>

static char *generate_thread_id(void)
{
    /*
     * Generate a nanoid-style thread ID matching UvA frontend format.
     * UvA uses 36-char alphanumeric IDs (not UUID v4).
     */
    char id[NANOID_THREAD_LEN + 1];
    gen_nanoid(id, NANOID_THREAD_LEN);
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
