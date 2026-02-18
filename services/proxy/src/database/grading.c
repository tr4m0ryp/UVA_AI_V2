#include "database.h"
#include "database_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int db_grading_create(int64_t user_id, const char *title,
                      int submission_count, const char *results_json,
                      int64_t *out_id)
{
    db_lock();
    const char *sql =
        "INSERT INTO grading_sessions (user_id, title, submission_count, results_json) "
        "VALUES (?1, ?2, ?3, ?4)";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "db_grading_create: prepare: %s\n",
                sqlite3_errmsg(g_db));
        db_unlock();
        return -1;
    }

    sqlite3_bind_int64(stmt, 1, user_id);
    sqlite3_bind_text(stmt, 2, title, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, submission_count);
    sqlite3_bind_text(stmt, 4, results_json ? results_json : "{}", -1,
                      SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE && out_id)
        *out_id = sqlite3_last_insert_rowid(g_db);

    sqlite3_finalize(stmt);
    db_unlock();
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_grading_list(int64_t user_id, db_grading_session_t **out, int *count)
{
    db_lock();
    const char *sql =
        "SELECT id, user_id, title, submission_count, created_at "
        "FROM grading_sessions WHERE user_id = ?1 ORDER BY created_at DESC";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "db_grading_list: prepare: %s\n",
                sqlite3_errmsg(g_db));
        db_unlock();
        *count = 0;
        return -1;
    }

    sqlite3_bind_int64(stmt, 1, user_id);

    int n = 0;
    int cap = 16;
    db_grading_session_t *sessions = malloc(sizeof(db_grading_session_t) * (size_t)cap);
    if (!sessions) {
        sqlite3_finalize(stmt);
        db_unlock();
        *count = 0;
        return -1;
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (n >= cap) {
            cap *= 2;
            db_grading_session_t *tmp = realloc(sessions,
                sizeof(db_grading_session_t) * (size_t)cap);
            if (!tmp) { free(sessions); sessions = NULL; break; }
            sessions = tmp;
        }
        db_grading_session_t *s = &sessions[n];
        memset(s, 0, sizeof(*s));
        s->id = sqlite3_column_int64(stmt, 0);
        s->user_id = sqlite3_column_int64(stmt, 1);

        const char *txt;
        txt = (const char *)sqlite3_column_text(stmt, 2);
        if (txt) snprintf(s->title, sizeof(s->title), "%s", txt);
        s->submission_count = sqlite3_column_int(stmt, 3);
        txt = (const char *)sqlite3_column_text(stmt, 4);
        if (txt) snprintf(s->created_at, sizeof(s->created_at), "%s", txt);
        n++;
    }

    sqlite3_finalize(stmt);
    db_unlock();

    if (!sessions) { *count = 0; return -1; }
    *out = sessions;
    *count = n;
    return 0;
}

int db_grading_get_json(int64_t session_id, int64_t user_id, char **out_json)
{
    db_lock();
    const char *sql =
        "SELECT results_json FROM grading_sessions WHERE id = ?1 AND user_id = ?2";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        db_unlock();
        return -1;
    }

    sqlite3_bind_int64(stmt, 1, session_id);
    sqlite3_bind_int64(stmt, 2, user_id);

    rc = sqlite3_step(stmt);
    int found = 0;
    if (rc == SQLITE_ROW) {
        const char *txt = (const char *)sqlite3_column_text(stmt, 0);
        if (txt) {
            *out_json = strdup(txt);
            found = (*out_json != NULL) ? 1 : 0;
        }
    }

    sqlite3_finalize(stmt);
    db_unlock();
    return found ? 0 : -1;
}

int db_grading_delete(int64_t session_id, int64_t user_id)
{
    db_lock();
    const char *sql =
        "DELETE FROM grading_sessions WHERE id = ?1 AND user_id = ?2";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { db_unlock(); return -1; }

    sqlite3_bind_int64(stmt, 1, session_id);
    sqlite3_bind_int64(stmt, 2, user_id);
    rc = sqlite3_step(stmt);
    int changes = sqlite3_changes(g_db);
    sqlite3_finalize(stmt);
    db_unlock();
    return (rc == SQLITE_DONE && changes > 0) ? 0 : -1;
}
