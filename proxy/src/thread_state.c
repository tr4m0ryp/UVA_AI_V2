#include "utils/thread_state.h"
#include "database/database_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *TS_SCHEMA =
    "CREATE TABLE IF NOT EXISTS thread_state ("
    "  conv_key TEXT PRIMARY KEY,"
    "  uva_thread_id TEXT NOT NULL,"
    "  model TEXT DEFAULT '',"
    "  message_count INTEGER DEFAULT 0,"
    "  created_at INTEGER DEFAULT (strftime('%s','now')),"
    "  last_used INTEGER DEFAULT (strftime('%s','now'))"
    ");";

/* Throttle purge to once per 5 minutes */
static time_t last_purge = 0;
#define PURGE_INTERVAL 300

int thread_state_init_schema(void)
{
    db_lock();
    char *err = NULL;
    int rc = sqlite3_exec(g_db, TS_SCHEMA, NULL, NULL, &err);
    db_unlock();

    if (rc != SQLITE_OK) {
        fprintf(stderr, "thread_state_init_schema: %s\n", err);
        sqlite3_free(err);
        return -1;
    }
    return 0;
}

int thread_state_find(const char *conv_key, thread_state_t *out)
{
    if (!conv_key || !out) return -1;

    /* Opportunistic purge */
    time_t now = time(NULL);
    if (now - last_purge > PURGE_INTERVAL) {
        last_purge = now;
        thread_state_purge();
    }

    db_lock();
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db,
        "SELECT conv_key, uva_thread_id, model, message_count, last_used "
        "FROM thread_state WHERE conv_key = ?",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        db_unlock();
        return -1;
    }

    sqlite3_bind_text(stmt, 1, conv_key, -1, SQLITE_STATIC);

    int found = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int64_t last_used = sqlite3_column_int64(stmt, 4);
        if (now - (time_t)last_used > THREAD_STATE_TTL_SECONDS) {
            /* Expired -- delete it */
            sqlite3_finalize(stmt);
            sqlite3_stmt *del;
            sqlite3_prepare_v2(g_db,
                "DELETE FROM thread_state WHERE conv_key = ?",
                -1, &del, NULL);
            if (del) {
                sqlite3_bind_text(del, 1, conv_key, -1, SQLITE_STATIC);
                sqlite3_step(del);
                sqlite3_finalize(del);
            }
            db_unlock();
            return -1;
        }

        memset(out, 0, sizeof(*out));
        const char *k = (const char *)sqlite3_column_text(stmt, 0);
        const char *t = (const char *)sqlite3_column_text(stmt, 1);
        const char *m = (const char *)sqlite3_column_text(stmt, 2);
        if (k) snprintf(out->conv_key, sizeof(out->conv_key), "%s", k);
        if (t) snprintf(out->uva_thread_id, sizeof(out->uva_thread_id),
                        "%s", t);
        if (m) snprintf(out->model, sizeof(out->model), "%s", m);
        out->message_count = sqlite3_column_int(stmt, 3);
        found = 0;
    }

    sqlite3_finalize(stmt);
    db_unlock();
    return found;
}

int thread_state_save(const thread_state_t *ts)
{
    if (!ts) return -1;

    db_lock();
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db,
        "INSERT OR REPLACE INTO thread_state "
        "(conv_key, uva_thread_id, model, message_count, "
        " created_at, last_used) "
        "VALUES (?, ?, ?, ?, strftime('%s','now'), strftime('%s','now'))",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        db_unlock();
        return -1;
    }

    sqlite3_bind_text(stmt, 1, ts->conv_key, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, ts->uva_thread_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, ts->model, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, ts->message_count);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    db_unlock();

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "thread_state_save: %s\n", sqlite3_errmsg(g_db));
        return -1;
    }
    return 0;
}

int thread_state_incr(const char *conv_key)
{
    if (!conv_key) return -1;

    db_lock();
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db,
        "UPDATE thread_state SET message_count = message_count + 1, "
        "last_used = strftime('%s','now') WHERE conv_key = ?",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        db_unlock();
        return -1;
    }

    sqlite3_bind_text(stmt, 1, conv_key, -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    db_unlock();

    return (rc == SQLITE_DONE) ? 0 : -1;
}

int thread_state_delete(const char *conv_key)
{
    if (!conv_key) return -1;

    db_lock();
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db,
        "DELETE FROM thread_state WHERE conv_key = ?",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        db_unlock();
        return -1;
    }

    sqlite3_bind_text(stmt, 1, conv_key, -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    db_unlock();

    return (rc == SQLITE_DONE) ? 0 : -1;
}

void thread_state_purge(void)
{
    db_lock();
    char sql[128];
    snprintf(sql, sizeof(sql),
        "DELETE FROM thread_state WHERE last_used < strftime('%%s','now') - %d",
        THREAD_STATE_TTL_SECONDS);
    sqlite3_exec(g_db, sql, NULL, NULL, NULL);
    db_unlock();
}
