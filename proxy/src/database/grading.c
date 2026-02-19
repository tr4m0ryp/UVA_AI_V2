#include "grading.h"
#include "database_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int db_grading_session_create(int64_t user_id, const char *name,
                              const char *assignment, const char *rubric,
                              const char *example, int64_t *out_id)
{
    db_lock();
    const char *sql =
        "INSERT INTO grading_sessions "
        "(user_id, name, assignment_text, rubric_text, example_text) "
        "VALUES (?1,?2,?3,?4,?5)";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "db_grading_session_create: %s\n",
                sqlite3_errmsg(g_db));
        db_unlock();
        return -1;
    }

    sqlite3_bind_int64(stmt, 1, user_id);
    sqlite3_bind_text(stmt, 2, name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, assignment ? assignment : "", -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, rubric ? rubric : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, example ? example : "", -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE && out_id)
        *out_id = sqlite3_last_insert_rowid(g_db);

    sqlite3_finalize(stmt);
    db_unlock();
    return (rc == SQLITE_DONE) ? 0 : -1;
}

static void fill_session(sqlite3_stmt *stmt, db_grading_session_t *out)
{
    memset(out, 0, sizeof(*out));
    out->id = sqlite3_column_int64(stmt, 0);
    out->user_id = sqlite3_column_int64(stmt, 1);

    const char *s;
    s = (const char *)sqlite3_column_text(stmt, 2);
    if (s) snprintf(out->name, sizeof(out->name), "%s", s);

    out->submission_count = sqlite3_column_int(stmt, 3);
    out->avg_percentage = sqlite3_column_double(stmt, 4);

    s = (const char *)sqlite3_column_text(stmt, 5);
    if (s) snprintf(out->status, sizeof(out->status), "%s", s);

    s = (const char *)sqlite3_column_text(stmt, 6);
    if (s) snprintf(out->created_at, sizeof(out->created_at), "%s", s);
}

int db_grading_session_list(int64_t user_id, db_grading_session_t **out,
                            int *count)
{
    db_lock();
    const char *sql =
        "SELECT id, user_id, name, submission_count, avg_percentage, "
        "status, created_at FROM grading_sessions "
        "WHERE user_id = ?1 ORDER BY id DESC";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { db_unlock(); *count = 0; return -1; }

    sqlite3_bind_int64(stmt, 1, user_id);

    int n = 0, cap = 16;
    db_grading_session_t *list = malloc(sizeof(*list) * (size_t)cap);

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (n >= cap) {
            cap *= 2;
            list = realloc(list, sizeof(*list) * (size_t)cap);
        }
        fill_session(stmt, &list[n]);
        n++;
    }

    sqlite3_finalize(stmt);
    db_unlock();
    *out = list;
    *count = n;
    return 0;
}

int db_grading_session_get(int64_t session_id, int64_t user_id,
                           db_grading_session_t *out)
{
    db_lock();
    const char *sql =
        "SELECT id, user_id, name, submission_count, avg_percentage, "
        "status, created_at FROM grading_sessions "
        "WHERE id = ?1 AND user_id = ?2";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { db_unlock(); return -1; }

    sqlite3_bind_int64(stmt, 1, session_id);
    sqlite3_bind_int64(stmt, 2, user_id);
    rc = sqlite3_step(stmt);
    int found = 0;
    if (rc == SQLITE_ROW) {
        fill_session(stmt, out);
        found = 1;
    }
    sqlite3_finalize(stmt);
    db_unlock();
    return found ? 0 : -1;
}

int db_grading_session_delete(int64_t session_id, int64_t user_id)
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

int db_grading_session_update_stats(int64_t session_id, int count,
                                    double avg_pct, const char *status)
{
    db_lock();
    const char *sql =
        "UPDATE grading_sessions SET submission_count = ?1, "
        "avg_percentage = ?2, status = ?3 WHERE id = ?4";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { db_unlock(); return -1; }

    sqlite3_bind_int(stmt, 1, count);
    sqlite3_bind_double(stmt, 2, avg_pct);
    sqlite3_bind_text(stmt, 3, status ? status : "completed", -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, session_id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    db_unlock();
    return (rc == SQLITE_DONE) ? 0 : -1;
}

static void fill_result(sqlite3_stmt *stmt, db_grading_result_t *out)
{
    memset(out, 0, sizeof(*out));
    out->id = sqlite3_column_int64(stmt, 0);
    out->session_id = sqlite3_column_int64(stmt, 1);

    const char *s;
    s = (const char *)sqlite3_column_text(stmt, 2);
    if (s) snprintf(out->student_name, sizeof(out->student_name), "%s", s);
    s = (const char *)sqlite3_column_text(stmt, 3);
    if (s) snprintf(out->student_id_str, sizeof(out->student_id_str),
                    "%s", s);
    s = (const char *)sqlite3_column_text(stmt, 4);
    if (s) snprintf(out->file_name, sizeof(out->file_name), "%s", s);

    out->total_score = sqlite3_column_double(stmt, 5);
    out->total_max = sqlite3_column_double(stmt, 6);
    out->percentage = sqlite3_column_double(stmt, 7);

    s = (const char *)sqlite3_column_text(stmt, 8);
    if (s) snprintf(out->overall_feedback, sizeof(out->overall_feedback),
                    "%s", s);
    s = (const char *)sqlite3_column_text(stmt, 9);
    if (s) snprintf(out->criteria_json, sizeof(out->criteria_json), "%s", s);
    s = (const char *)sqlite3_column_text(stmt, 10);
    if (s) snprintf(out->status, sizeof(out->status), "%s", s);
    s = (const char *)sqlite3_column_text(stmt, 11);
    if (s) snprintf(out->error_text, sizeof(out->error_text), "%s", s);
    s = (const char *)sqlite3_column_text(stmt, 12);
    if (s) snprintf(out->created_at, sizeof(out->created_at), "%s", s);
}

int db_grading_result_save(int64_t session_id, const db_grading_result_t *r)
{
    db_lock();
    const char *sql =
        "INSERT INTO grading_results "
        "(session_id, student_name, student_id_str, file_name, "
        "total_score, total_max, percentage, overall_feedback, "
        "criteria_json, status, error_text) "
        "VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11)";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "db_grading_result_save: %s\n",
                sqlite3_errmsg(g_db));
        db_unlock();
        return -1;
    }

    sqlite3_bind_int64(stmt, 1, session_id);
    sqlite3_bind_text(stmt, 2, r->student_name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, r->student_id_str, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, r->file_name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 5, r->total_score);
    sqlite3_bind_double(stmt, 6, r->total_max);
    sqlite3_bind_double(stmt, 7, r->percentage);
    sqlite3_bind_text(stmt, 8, r->overall_feedback, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 9, r->criteria_json, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 10, r->status, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 11, r->error_text, -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    db_unlock();
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_grading_result_list(int64_t session_id, db_grading_result_t **out,
                           int *count)
{
    db_lock();
    const char *sql =
        "SELECT id, session_id, student_name, student_id_str, file_name, "
        "total_score, total_max, percentage, overall_feedback, "
        "criteria_json, status, error_text, created_at "
        "FROM grading_results WHERE session_id = ?1 ORDER BY id ASC";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { db_unlock(); *count = 0; return -1; }

    sqlite3_bind_int64(stmt, 1, session_id);

    int n = 0, cap = 16;
    db_grading_result_t *list = malloc(sizeof(*list) * (size_t)cap);

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (n >= cap) {
            cap *= 2;
            list = realloc(list, sizeof(*list) * (size_t)cap);
        }
        fill_result(stmt, &list[n]);
        n++;
    }

    sqlite3_finalize(stmt);
    db_unlock();
    *out = list;
    *count = n;
    return 0;
}
