#ifndef UVA_PROXY_CONFIG_H
#define UVA_PROXY_CONFIG_H

#include <stdint.h>

#define DEFAULT_LISTEN_PORT    8787
#define DEFAULT_BASE_URL       "https://aichat.uva.nl"
#define DEFAULT_MODEL          "claude-sonnet-4.5"
#define CONFIG_FILE            "proxy.env"
#define MAX_COOKIE_LEN         4096
#define MAX_URL_LEN            512
#define MAX_MODEL_LEN          128

typedef struct {
    uint16_t listen_port;
    char     session_cookie[MAX_COOKIE_LEN];
    char     base_url[MAX_URL_LEN];
    char     default_model[MAX_MODEL_LEN];
    int      do_login;
    int      verbose;
} proxy_config_t;

/* Load config from proxy.env file, then override with CLI args.
 * Returns 0 on success, -1 on error. */
int config_load(proxy_config_t *cfg, int argc, char **argv);

/* Save current cookie back to proxy.env. Returns 0 on success. */
int config_save_cookie(const proxy_config_t *cfg);

/* Print loaded config to stderr (for debugging). */
void config_print(const proxy_config_t *cfg);

#endif /* UVA_PROXY_CONFIG_H */
