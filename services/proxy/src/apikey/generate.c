#include "apikey.h"
#include <stdio.h>
#include <string.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

int apikey_generate(char *plaintext_out, size_t pt_size,
                    char *hash_out, size_t hash_size,
                    char *prefix_out, size_t prefix_size)
{
    unsigned char raw[APIKEY_RAW_BYTES];
    if (RAND_bytes(raw, APIKEY_RAW_BYTES) != 1)
        return -1;

    /* Build plaintext: uva-sk- + 64 hex chars */
    char hex[APIKEY_HEX_LEN + 1];
    for (int i = 0; i < APIKEY_RAW_BYTES; i++)
        snprintf(hex + i * 2, 3, "%02x", raw[i]);

    snprintf(plaintext_out, pt_size, "%s%s", APIKEY_PREFIX, hex);

    /* Prefix: first 12 chars of the full key for display */
    snprintf(prefix_out, prefix_size, "%.12s...", plaintext_out);

    /* Hash the full plaintext key */
    return apikey_hash(plaintext_out, hash_out, hash_size);
}

int apikey_hash(const char *key, char *hash_out, size_t hash_size)
{
    if (hash_size < 65) return -1;

    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char *)key, strlen(key), digest);

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        snprintf(hash_out + i * 2, 3, "%02x", digest[i]);

    return 0;
}
