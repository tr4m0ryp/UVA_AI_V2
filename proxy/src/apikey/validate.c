#include "apikey/apikey.h"
#include "config/config.h"
#include <stdio.h>
#include <string.h>

/* Global config pointer (set in main.c) */
extern const proxy_config_t *g_config;

const char *apikey_extract(const char *auth_header)
{
    if (!auth_header) return NULL;

    /* Skip "Bearer " prefix */
    const char *bearer = "Bearer ";
    if (strncmp(auth_header, bearer, 7) != 0)
        return NULL;

    const char *key = auth_header + 7;

    /* Accept the local webui key */
    if (strcmp(key, APIKEY_LOCAL) == 0)
        return key;

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
    /* Handle local webui key: use proxy's own session cookie */
    if (strcmp(key, APIKEY_LOCAL) == 0) {
        if (!g_config || !g_config->session_cookie[0]) {
            fprintf(stderr, "apikey_resolve: local key used but "
                    "no session cookie configured\n");
            return -1;
        }
        memset(out, 0, sizeof(*out));
        out->id = 0;
        out->user_id = 0;
        out->is_active = 1;
        out->temperature = -1;
        out->top_p = -1;
        strncpy(out->key_prefix, "uva-local...", DB_MAX_KEY_PREFIX - 1);
        strncpy(out->name, "Local WebUI", DB_MAX_KEY_NAME - 1);
        snprintf(out->model, sizeof(out->model), "%s",
                 g_config->default_model);
        snprintf(out->user_session, sizeof(out->user_session), "%s",
                 g_config->session_cookie);

        /* Default system prompt: professional academic tone, no emojis */
        snprintf(out->system_prompt, sizeof(out->system_prompt),
            "You are an academic assistant for the University of Amsterdam "
            "(UvA). Follow these guidelines strictly:\n"
            "1. Never use emojis in any response. Use plain text only.\n"
            "2. Maintain a professional, academic tone suitable for a "
            "university environment.\n"
            "3. Be precise and accurate. Clearly state limitations when "
            "uncertain.\n"
            "4. Structure responses with clear headings, numbered lists, "
            "or bullet points when appropriate.\n"
            "5. Reference established theories, methodologies, and sources "
            "when discussing academic topics.\n"
            "6. Present multiple perspectives on complex topics and "
            "encourage critical thinking.\n"
            "7. Use proper academic language. Avoid colloquialisms and "
            "informal expressions.\n"
            "8. Support learning through guided explanation rather than "
            "providing direct answers to assignments.");
        return 0;
    }

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
