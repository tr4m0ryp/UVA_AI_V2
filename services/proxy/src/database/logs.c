#include "database.h"
#include "database_internal.h"
#include <stdio.h>

int db_log_request(int64_t api_key_id, const char *model,
                   size_t input_chars, size_t output_chars,
                   int success, long latency_ms)
{
    db_lock();
    const char *sql =
        "INSERT INTO request_logs "
        "(api_key_id, model, input_chars, output_chars, success, latency_ms) "
        "VALUES (?1,?2,?3,?4,?5,?6)";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "db_log_request: prepare: %s\n",
                sqlite3_errmsg(g_db));
        db_unlock();
        return -1;
    }

    sqlite3_bind_int64(stmt, 1, api_key_id);
    sqlite3_bind_text(stmt, 2, model, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, (int64_t)input_chars);
    sqlite3_bind_int64(stmt, 4, (int64_t)output_chars);
    sqlite3_bind_int(stmt, 5, success);
    sqlite3_bind_int64(stmt, 6, (int64_t)latency_ms);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    db_unlock();
    return (rc == SQLITE_DONE) ? 0 : -1;
}
