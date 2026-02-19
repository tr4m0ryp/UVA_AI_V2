#include "apikey.h"
#include <stdio.h>
#include <string.h>

const char *apikey_extract(const char *auth_header)
{
    if (!auth_header) return NULL;

    /* Skip "Bearer " prefix */
    const char *bearer = "Bearer ";
    if (strncmp(auth_header, bearer, 7) != 0)
        return NULL;

    const char *key = auth_header + 7;

    /* Must start with uva-sk- prefix */
    if (strncmp(key, APIKEY_PREFIX, APIKEY_PREFIX_LEN) != 0)
        return NULL;

    /* Sanity check length: prefix(7) + hex(64) = 71 */
    size_t len = strlen(key);
    if (len != APIKEY_FULL_LEN)
        return NULL;

    return key;
}

int apikey_resolve(const char *key, db_api_key_t *out)
{
    char hash[65];
    if (apikey_hash(key, hash, sizeof(hash)) != 0)
        return -1;

    if (db_key_find_by_hash(hash, out) != 0)
        return -1;

    if (!out->is_active) {
        fprintf(stderr, "apikey_resolve: key %s is inactive\n",
                out->key_prefix);
        return -1;
    }

    /* Update last_used timestamp */
    db_key_touch(out->id);

    return 0;
}
