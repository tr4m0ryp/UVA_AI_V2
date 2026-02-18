#ifndef UVA_PROXY_APIKEY_H
#define UVA_PROXY_APIKEY_H

#include "database.h"

#define APIKEY_PREFIX     "uva-sk-"
#define APIKEY_PREFIX_LEN 7
#define APIKEY_RAW_BYTES  32
#define APIKEY_HEX_LEN    64  /* 32 bytes * 2 hex chars */
#define APIKEY_FULL_LEN   71  /* prefix + hex */

/* Generate a new API key. Writes full key (uva-sk-...) to plaintext_out,
 * SHA-256 hash (hex) to hash_out, first 12 chars to prefix_out.
 * Returns 0 on success. */
int apikey_generate(char *plaintext_out, size_t pt_size,
                    char *hash_out, size_t hash_size,
                    char *prefix_out, size_t prefix_size);

/* SHA-256 hash a key string. Writes hex digest to hash_out.
 * Returns 0 on success. */
int apikey_hash(const char *key, char *hash_out, size_t hash_size);

/* Extract API key from Authorization header value.
 * Returns pointer to the key string (after "Bearer "), or NULL. */
const char *apikey_extract(const char *auth_header);

/* Resolve an API key: hash it, look up in DB, check active.
 * Fills out with key record + user session cookie.
 * Returns 0 on success, -1 if not found or inactive. */
int apikey_resolve(const char *key, db_api_key_t *out);

#endif /* UVA_PROXY_APIKEY_H */
