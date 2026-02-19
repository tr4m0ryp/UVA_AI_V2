#ifndef UVA_PROXY_DATABASE_H
#define UVA_PROXY_DATABASE_H

#include <stdint.h>
#include <time.h>

#define DB_MAX_EMAIL       256
#define DB_MAX_NAME        256
#define DB_MAX_COOKIE      4096
#define DB_MAX_TOKEN       128
#define DB_MAX_KEY_HASH    128
#define DB_MAX_KEY_PREFIX  32
#define DB_MAX_KEY_NAME    128
#define DB_MAX_MODEL       128
#define DB_MAX_PROMPT      4096

typedef struct {
    int64_t id;
    char    email[DB_MAX_EMAIL];
    char    name[DB_MAX_NAME];
    char    uva_session[DB_MAX_COOKIE];
    char    dashboard_token[DB_MAX_TOKEN];
} db_user_t;

typedef struct {
    int64_t id;
    int64_t user_id;
    char    key_hash[DB_MAX_KEY_HASH];
    char    key_prefix[DB_MAX_KEY_PREFIX];
    char    name[DB_MAX_KEY_NAME];
    char    model[DB_MAX_MODEL];
    double  temperature;
    double  top_p;
    int     max_tokens;
    double  frequency_penalty;
    double  presence_penalty;
    char    reasoning_effort[16];
    char    system_prompt[DB_MAX_PROMPT];
    int     is_active;
    /* resolved from JOIN */
    char    user_session[DB_MAX_COOKIE];
} db_api_key_t;

/* Initialize database. Creates schema if needed. Returns 0 on success. */
int db_init(const char *path);

/* Close database. */
void db_close(void);

/* User CRUD */
int db_user_upsert(const char *email, const char *name, const char *session);
int db_user_find_by_email(const char *email, db_user_t *out);
int db_user_find_by_token(const char *token, db_user_t *out);
int db_user_update_session(int64_t user_id, const char *session);
int db_user_set_token(int64_t user_id, const char *token);
int db_user_clear_token(int64_t user_id);

/* API Key CRUD */
int db_key_create(const db_api_key_t *key, int64_t *out_id);
int db_key_find_by_hash(const char *hash, db_api_key_t *out);
int db_key_list_by_user(int64_t user_id, db_api_key_t **out, int *count);
int db_key_update(int64_t key_id, int64_t user_id, const db_api_key_t *key);
int db_key_delete(int64_t key_id, int64_t user_id);
int db_key_toggle(int64_t key_id, int64_t user_id);
int db_key_touch(int64_t key_id);

#endif /* UVA_PROXY_DATABASE_H */
