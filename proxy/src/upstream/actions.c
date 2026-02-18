#include "upstream.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <json-c/json.h>

/*
 * Server action IDs extracted from UvA source maps.
 * These may change on redeployment.
 */
#define ACTION_UPSERT_THREAD   "7f5606233197c25071eb2ef91303c87f6ccc18fa87"
#define ACTION_DELETE_THREAD   "7ff009fa85af417d5914c562140adf3847b804201b"
#define ACTION_FIND_ALL_CHATS  "7fe85c163c4d01355d846dacf9c0eb4a1c47eb6612"
#define ACTION_FETCH_MODELS    "7f97bd7faa76e73dd6f8166e9eb66eda8d773789aa"

/* Thread pool for reusing chat threads */
#define MAX_THREADS 8

static char g_thread_ids[MAX_THREADS][MAX_THREAD_ID_LEN];
static int  g_thread_count = 0;
static int  g_thread_next = 0;
static pthread_mutex_t g_thread_lock = PTHREAD_MUTEX_INITIALIZER;

static char *generate_thread_id(void)
{
    /* Generate a random-looking ID similar to UvA format */
    static const char charset[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    char id[37];

    srand((unsigned)time(NULL) ^ (unsigned)rand());
    for (int i = 0; i < 36; i++)
        id[i] = charset[rand() % (sizeof(charset) - 1)];
    id[36] = '\0';

    return strdup(id);
}

/*
 * Create a new chat thread via the UvA server action.
 * Returns the thread ID string (caller frees), or NULL on failure.
 *
 * The server action expects a JSON array body:
 *   [{ "id": "...", "name": "New chat", ... }]
 *
 * Note: The exact format may need adjustment after Phase 1 traffic capture.
 * For now we attempt a minimal upsert with just id and name.
 */
static char *create_thread_via_action(void)
{
    char *tid = generate_thread_id();
    if (!tid) return NULL;

    /* Build the server action body */
    char body[512];
    snprintf(body, sizeof(body),
        "[{\"id\":\"%s\",\"name\":\"Proxy chat\","
        "\"personaMessage\":\"\","
        "\"personaMessageTitle\":\"\","
        "\"createdAt\":\"%ld\","
        "\"isBookmarked\":false,"
        "\"bookmarkedAt\":\"\","
        "\"type\":\"chat\"}]",
        tid, (long)time(NULL));

    buffer_t resp;
    int code = upstream_server_action(ACTION_UPSERT_THREAD,
                                       body, strlen(body), &resp);

    if (code < 200 || code >= 300) {
        fprintf(stderr, "create_thread: server action returned %d\n", code);
        if (resp.data)
            fprintf(stderr, "  response: %.200s\n", resp.data);
        buffer_free(&resp);
        /* Fall back to using the generated ID anyway -- the chat API
         * might create the thread implicitly */
    } else {
        buffer_free(&resp);
    }

    return tid;
}

char *actions_get_or_create_thread(void)
{
    pthread_mutex_lock(&g_thread_lock);

    /* If we have threads in the pool, rotate through them */
    if (g_thread_count > 0) {
        int idx = g_thread_next % g_thread_count;
        g_thread_next++;
        char *result = strdup(g_thread_ids[idx]);
        pthread_mutex_unlock(&g_thread_lock);
        return result;
    }

    pthread_mutex_unlock(&g_thread_lock);

    /* Create a new thread */
    char *tid = create_thread_via_action();
    if (!tid) return NULL;

    /* Add to pool */
    pthread_mutex_lock(&g_thread_lock);
    if (g_thread_count < MAX_THREADS) {
        strncpy(g_thread_ids[g_thread_count], tid, MAX_THREAD_ID_LEN - 1);
        g_thread_count++;
    }
    pthread_mutex_unlock(&g_thread_lock);

    return tid;
}

int actions_delete_thread(const char *thread_id)
{
    char body[128];
    snprintf(body, sizeof(body), "[\"%s\"]", thread_id);

    buffer_t resp;
    int code = upstream_server_action(ACTION_DELETE_THREAD,
                                       body, strlen(body), &resp);
    buffer_free(&resp);

    /* Remove from pool */
    pthread_mutex_lock(&g_thread_lock);
    for (int i = 0; i < g_thread_count; i++) {
        if (strcmp(g_thread_ids[i], thread_id) == 0) {
            memmove(&g_thread_ids[i], &g_thread_ids[i+1],
                    (size_t)(g_thread_count - i - 1) * MAX_THREAD_ID_LEN);
            g_thread_count--;
            break;
        }
    }
    pthread_mutex_unlock(&g_thread_lock);

    return (code >= 200 && code < 300) ? 0 : -1;
}

void actions_cleanup_threads(void)
{
    pthread_mutex_lock(&g_thread_lock);
    for (int i = 0; i < g_thread_count; i++) {
        char body[MAX_THREAD_ID_LEN + 64];
        snprintf(body, sizeof(body), "[\"%.*s\"]",
                 MAX_THREAD_ID_LEN - 1, g_thread_ids[i]);
        buffer_t resp;
        upstream_server_action(ACTION_DELETE_THREAD,
                               body, strlen(body), &resp);
        buffer_free(&resp);
    }
    g_thread_count = 0;
    pthread_mutex_unlock(&g_thread_lock);
}
