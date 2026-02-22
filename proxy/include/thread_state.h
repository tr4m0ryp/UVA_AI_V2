#ifndef UVA_PROXY_THREAD_STATE_H
#define UVA_PROXY_THREAD_STATE_H

#define THREAD_STATE_TTL_SECONDS (2 * 3600)  /* 2 hours */
#define THREAD_STATE_MAX_KEY     128
#define THREAD_STATE_MAX_THREAD   64
#define THREAD_STATE_MAX_MODEL    64

typedef struct {
    char conv_key[THREAD_STATE_MAX_KEY];
    char uva_thread_id[THREAD_STATE_MAX_THREAD];
    char model[THREAD_STATE_MAX_MODEL];
    int  message_count;
} thread_state_t;

/* Create the thread_state table if it doesn't exist. */
int thread_state_init_schema(void);

/* Look up a thread state by conversation key.
 * Returns 0 and fills out on success, -1 if not found or expired. */
int thread_state_find(const char *conv_key, thread_state_t *out);

/* Insert or replace a thread state record. Returns 0 on success. */
int thread_state_save(const thread_state_t *ts);

/* Increment message_count and update last_used for a conversation key.
 * Returns 0 on success. */
int thread_state_incr(const char *conv_key);

/* Delete a thread state record. Returns 0 on success. */
int thread_state_delete(const char *conv_key);

/* Remove expired entries (older than TTL). Called opportunistically. */
void thread_state_purge(void);

#endif /* UVA_PROXY_THREAD_STATE_H */
