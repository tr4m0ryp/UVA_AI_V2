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
#define DB_MAX_ALLOWLIST   512

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
    /* tool configuration */
    int     allow_tools;                    /* 1 = tool use permitted */
    char    tool_allowlist[DB_MAX_ALLOWLIST]; /* "" = all, or "bash,read" */
    int     max_tokens_tool;               /* token budget for tool turns */
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

#define DB_MAX_GRADING_TITLE  256
#define DB_MAX_RESULTS_JSON   524288   /* 512 KB cap */

typedef struct {
    int64_t id;
    int64_t user_id;
    char    title[DB_MAX_GRADING_TITLE];
    int     submission_count;
    char    created_at[32];
    /* results_json omitted from list responses; fetched via get */
} db_grading_session_t;

/* Grading session CRUD */
int db_grading_create(int64_t user_id, const char *title,
                      int submission_count, const char *results_json,
                      int64_t *out_id);
int db_grading_list(int64_t user_id, db_grading_session_t **out, int *count);
int db_grading_get_json(int64_t session_id, int64_t user_id, char **out_json);
int db_grading_delete(int64_t session_id, int64_t user_id);

/* API Key CRUD */
int db_key_create(const db_api_key_t *key, int64_t *out_id);
int db_key_find_by_hash(const char *hash, db_api_key_t *out);
int db_key_list_by_user(int64_t user_id, db_api_key_t **out, int *count);
int db_key_update(int64_t key_id, int64_t user_id, const db_api_key_t *key);
int db_key_delete(int64_t key_id, int64_t user_id);
int db_key_toggle(int64_t key_id, int64_t user_id);
int db_key_touch(int64_t key_id);

/* Usage tracking */
typedef struct {
    int64_t total_requests;
    int64_t successful_requests;
    int64_t error_requests;
    int64_t total_input_tokens;
    int64_t total_output_tokens;
    int64_t avg_latency_ms;
} db_usage_stats_t;

typedef struct {
    char    day[12];
    int     requests;
    int64_t input_tokens;
    int64_t output_tokens;
} db_daily_stats_t;

/* Log one completed request. input_chars/output_chars are raw byte counts;
 * token estimates are derived at query time (chars / 4). */
int db_log_request(int64_t api_key_id, const char *model,
                   size_t input_chars, size_t output_chars,
                   int success, long latency_ms);

/* Aggregate usage stats for a key owned by user_id.
 * On success, *daily points to a heap-allocated array; caller must free it. */
int db_key_get_stats(int64_t key_id, int64_t user_id,
                     db_usage_stats_t *stats,
                     db_daily_stats_t **daily, int *daily_count);

#endif /* UVA_PROXY_DATABASE_H */
