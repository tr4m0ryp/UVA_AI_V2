#include "api/responses/responses.h"
#include "core/database/database_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *STATE_SCHEMA =
    "CREATE TABLE IF NOT EXISTS response_state ("
    "  response_id TEXT PRIMARY KEY,"
    "  messages_json TEXT NOT NULL,"
    "  created_at TEXT DEFAULT (datetime('now'))"
    ");";

/* TTL for response state entries: 2 hours */
#define STATE_TTL_SECONDS (2 * 3600)

int resp_state_init_schema(void)
{
    db_lock();
    char *err = NULL;
    int rc = sqlite3_exec(g_db, STATE_SCHEMA, NULL, NULL, &err);
    db_unlock();

    if (rc != SQLITE_OK) {
        fprintf(stderr, "resp_state_init_schema: %s\n", err);
        sqlite3_free(err);
        return -1;
    }
    return 0;
}

int resp_state_save(const char *response_id, const char *messages_json)
{
    if (!response_id || !messages_json) return -1;

    db_lock();
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db,
        "INSERT OR REPLACE INTO response_state "
        "(response_id, messages_json) VALUES (?, ?)",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        db_unlock();
        return -1;
    }

    sqlite3_bind_text(stmt, 1, response_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, messages_json, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    db_unlock();

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "resp_state_save: insert failed: %s\n",
                sqlite3_errmsg(g_db));
        return -1;
    }

    return 0;
}

char *resp_state_load(const char *response_id)
{
    if (!response_id) return NULL;

    db_lock();
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db,
        "SELECT messages_json, created_at FROM response_state "
        "WHERE response_id = ?",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        db_unlock();
        return NULL;
    }

    sqlite3_bind_text(stmt, 1, response_id, -1, SQLITE_STATIC);

    char *result = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *json = (const char *)sqlite3_column_text(stmt, 0);
        const char *created = (const char *)sqlite3_column_text(stmt, 1);

        /* Check TTL */
        struct tm tm_created;
        memset(&tm_created, 0, sizeof(tm_created));
        if (created && strptime(created, "%Y-%m-%d %H:%M:%S", &tm_created)) {
            time_t ct = timegm(&tm_created);
            time_t now = time(NULL);
            if (difftime(now, ct) > STATE_TTL_SECONDS) {
                /* Expired: delete and return NULL */
                sqlite3_finalize(stmt);
                sqlite3_exec(g_db,
                    "DELETE FROM response_state WHERE response_id = ?",
                    NULL, NULL, NULL);
                /* Use a separate statement for the delete */
                sqlite3_stmt *del;
                sqlite3_prepare_v2(g_db,
                    "DELETE FROM response_state WHERE response_id = ?",
                    -1, &del, NULL);
                if (del) {
                    sqlite3_bind_text(del, 1, response_id, -1, SQLITE_STATIC);
                    sqlite3_step(del);
                    sqlite3_finalize(del);
                }
                db_unlock();
                return NULL;
            }
        }

        if (json)
            result = strdup(json);
    }

    sqlite3_finalize(stmt);
    db_unlock();
    return result;
}

int resp_state_delete(const char *response_id)
{
    if (!response_id) return -1;

    db_lock();
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db,
        "DELETE FROM response_state WHERE response_id = ?",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        db_unlock();
        return -1;
    }

    sqlite3_bind_text(stmt, 1, response_id, -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    db_unlock();

    return (rc == SQLITE_DONE) ? 0 : -1;
}
