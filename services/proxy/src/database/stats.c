#include "database.h"
#include "database_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int db_key_get_stats(int64_t key_id, int64_t user_id,
                     db_usage_stats_t *stats,
                     db_daily_stats_t **daily, int *daily_count)
{
    db_lock();

    /* Aggregate totals - JOIN ensures the key belongs to user_id */
    const char *sql =
        "SELECT COUNT(*),"
        " SUM(CASE WHEN r.success=1 THEN 1 ELSE 0 END),"
        " SUM(CASE WHEN r.success=0 THEN 1 ELSE 0 END),"
        " COALESCE(SUM(r.input_chars),0),"
        " COALESCE(SUM(r.output_chars),0),"
        " COALESCE(AVG(CASE WHEN r.success=1 THEN r.latency_ms ELSE NULL END),0)"
        " FROM request_logs r"
        " JOIN api_keys k ON r.api_key_id = k.id"
        " WHERE r.api_key_id=?1 AND k.user_id=?2";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { db_unlock(); return -1; }

    sqlite3_bind_int64(stmt, 1, key_id);
    sqlite3_bind_int64(stmt, 2, user_id);

    memset(stats, 0, sizeof(*stats));
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        stats->total_requests      = sqlite3_column_int64(stmt, 0);
        stats->successful_requests = sqlite3_column_int64(stmt, 1);
        stats->error_requests      = sqlite3_column_int64(stmt, 2);
        /* Estimate tokens: 1 token ~= 4 chars */
        stats->total_input_tokens  = sqlite3_column_int64(stmt, 3) / 4;
        stats->total_output_tokens = sqlite3_column_int64(stmt, 4) / 4;
        stats->avg_latency_ms      = (int64_t)sqlite3_column_double(stmt, 5);
    }
    sqlite3_finalize(stmt);

    /* Daily breakdown - last 30 days, newest first */
    const char *dsql =
        "SELECT date(r.created_at) as day,"
        " COUNT(*), COALESCE(SUM(r.input_chars),0),"
        " COALESCE(SUM(r.output_chars),0)"
        " FROM request_logs r"
        " JOIN api_keys k ON r.api_key_id = k.id"
        " WHERE r.api_key_id=?1 AND k.user_id=?2"
        " AND r.created_at >= datetime('now','-30 days')"
        " GROUP BY day ORDER BY day DESC";

    rc = sqlite3_prepare_v2(g_db, dsql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        db_unlock();
        *daily = NULL;
        *daily_count = 0;
        return 0; /* stats still valid */
    }

    sqlite3_bind_int64(stmt, 1, key_id);
    sqlite3_bind_int64(stmt, 2, user_id);

    int n = 0, cap = 32;
    db_daily_stats_t *days = malloc(sizeof(db_daily_stats_t) * (size_t)cap);
    if (!days) {
        sqlite3_finalize(stmt);
        db_unlock();
        *daily = NULL;
        *daily_count = 0;
        return 0;
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (n >= cap) {
            cap *= 2;
            db_daily_stats_t *tmp = realloc(days,
                sizeof(db_daily_stats_t) * (size_t)cap);
            if (!tmp) break;
            days = tmp;
        }
        const char *d = (const char *)sqlite3_column_text(stmt, 0);
        snprintf(days[n].day, sizeof(days[n].day), "%s", d ? d : "");
        days[n].requests      = sqlite3_column_int(stmt, 1);
        days[n].input_tokens  = sqlite3_column_int64(stmt, 2) / 4;
        days[n].output_tokens = sqlite3_column_int64(stmt, 3) / 4;
        n++;
    }

    sqlite3_finalize(stmt);
    db_unlock();

    *daily = days;
    *daily_count = n;
    return 0;
}
