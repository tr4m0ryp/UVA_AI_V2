#include "core/database/database.h"
#include "core/database/database_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int db_log_request(int64_t api_key_id, int64_t user_id, const char *model,
                   int prompt_tokens, int completion_tokens, int status)
{
    db_lock();
    const char *sql =
        "INSERT INTO request_logs "
        "(api_key_id, user_id, model, prompt_tokens, completion_tokens, "
        "total_tokens, status) VALUES (?1,?2,?3,?4,?5,?6,?7)";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "db_log_request: prepare: %s\n",
                sqlite3_errmsg(g_db));
        db_unlock();
        return -1;
    }

    int total = prompt_tokens + completion_tokens;
    sqlite3_bind_int64(stmt, 1, api_key_id);
    sqlite3_bind_int64(stmt, 2, user_id);
    sqlite3_bind_text(stmt, 3, model, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, prompt_tokens);
    sqlite3_bind_int(stmt, 5, completion_tokens);
    sqlite3_bind_int(stmt, 6, total);
    sqlite3_bind_int(stmt, 7, status);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    db_unlock();
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_key_usage_summary(int64_t key_id, int64_t user_id,
                         db_usage_summary_t *out)
{
    db_lock();
    const char *sql =
        "SELECT COUNT(*), COALESCE(SUM(prompt_tokens),0), "
        "COALESCE(SUM(completion_tokens),0), COALESCE(SUM(total_tokens),0) "
        "FROM request_logs WHERE api_key_id=?1 "
        "AND user_id=?2";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { db_unlock(); return -1; }

    sqlite3_bind_int64(stmt, 1, key_id);
    sqlite3_bind_int64(stmt, 2, user_id);

    memset(out, 0, sizeof(*out));
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        out->total_requests = sqlite3_column_int(stmt, 0);
        out->prompt_tokens = sqlite3_column_int(stmt, 1);
        out->completion_tokens = sqlite3_column_int(stmt, 2);
        out->total_tokens = sqlite3_column_int(stmt, 3);
    }
    sqlite3_finalize(stmt);
    db_unlock();
    return 0;
}

int db_key_usage_daily(int64_t key_id, int64_t user_id, int days,
                       db_usage_day_t **out, int *count)
{
    db_lock();
    const char *sql =
        "SELECT date(created_at) as d, COUNT(*), "
        "COALESCE(SUM(prompt_tokens),0), "
        "COALESCE(SUM(completion_tokens),0), "
        "COALESCE(SUM(total_tokens),0) "
        "FROM request_logs WHERE api_key_id=?1 AND user_id=?2 "
        "AND created_at >= datetime('now', ?3) "
        "GROUP BY d ORDER BY d ASC";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        db_unlock();
        *count = 0;
        return -1;
    }

    sqlite3_bind_int64(stmt, 1, key_id);
    sqlite3_bind_int64(stmt, 2, user_id);

    char offset[32];
    snprintf(offset, sizeof(offset), "-%d days", days);
    sqlite3_bind_text(stmt, 3, offset, -1, SQLITE_TRANSIENT);

    int n = 0;
    int cap = 32;
    db_usage_day_t *rows = malloc(sizeof(db_usage_day_t) * (size_t)cap);

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (n >= cap) {
            cap *= 2;
            rows = realloc(rows, sizeof(db_usage_day_t) * (size_t)cap);
        }
        memset(&rows[n], 0, sizeof(rows[n]));
        const char *d = (const char *)sqlite3_column_text(stmt, 0);
        if (d) snprintf(rows[n].date, sizeof(rows[n].date), "%s", d);
        rows[n].total_requests = sqlite3_column_int(stmt, 1);
        rows[n].prompt_tokens = sqlite3_column_int(stmt, 2);
        rows[n].completion_tokens = sqlite3_column_int(stmt, 3);
        rows[n].total_tokens = sqlite3_column_int(stmt, 4);
        n++;
    }

    sqlite3_finalize(stmt);
    db_unlock();

    *out = rows;
    *count = n;
    return 0;
}
