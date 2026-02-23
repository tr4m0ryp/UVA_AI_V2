#ifndef UVA_PROXY_GITHUB_H
#define UVA_PROXY_GITHUB_H

#include <stdint.h>

#define GITHUB_MAX_NAME     256
#define GITHUB_MAX_URL      512
#define GITHUB_MAX_DESC     512

typedef struct {
    int64_t github_id;
    char    full_name[GITHUB_MAX_NAME];
    char    description[GITHUB_MAX_DESC];
    char    clone_url[GITHUB_MAX_URL];
    int     is_private;
} github_repo_t;

/* Exchange an OAuth authorization code for an access token.
 * On success writes the token to token_out and the user login
 * to login_out. Returns 0 on success, -1 on error. */
int github_exchange_code(const char *client_id, const char *client_secret,
                         const char *code,
                         char *token_out, int token_size,
                         char *login_out, int login_size);

/* List repos for the authenticated user (up to 100).
 * Caller must free *repos_out. Returns count or -1 on error. */
int github_list_repos(const char *token,
                      github_repo_t **repos_out, int *count);

#endif /* UVA_PROXY_GITHUB_H */
