#include "core/database/database.h"
#include "core/database/database_internal.h"
#include <stdio.h>
#include <string.h>

int db_user_upsert(const char *email, const char *name, const char *session)
{
    db_lock();

    const char *sql =
        "INSERT INTO users (email, name, uva_session, last_login) "
        "VALUES (?1, ?2, ?3, datetime('now')) "
        "ON CONFLICT(email) DO UPDATE SET "
        "name=?2, uva_session=?3, last_login=datetime('now')";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "db_user_upsert: prepare: %s\n",
                sqlite3_errmsg(g_db));
        db_unlock();
        return -1;
    }

    sqlite3_bind_text(stmt, 1, email, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, name ? name : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, session, -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    db_unlock();

    return (rc == SQLITE_DONE) ? 0 : -1;
}

static int fill_user(sqlite3_stmt *stmt, db_user_t *out)
{
    memset(out, 0, sizeof(*out));
    out->id = sqlite3_column_int64(stmt, 0);

    const char *e = (const char *)sqlite3_column_text(stmt, 1);
    if (e) snprintf(out->email, sizeof(out->email), "%s", e);

    const char *n = (const char *)sqlite3_column_text(stmt, 2);
    if (n) snprintf(out->name, sizeof(out->name), "%s", n);

    const char *s = (const char *)sqlite3_column_text(stmt, 3);
    if (s) snprintf(out->uva_session, sizeof(out->uva_session), "%s", s);

    const char *t = (const char *)sqlite3_column_text(stmt, 4);
    if (t) snprintf(out->dashboard_token, sizeof(out->dashboard_token), "%s", t);

    const char *gt = (const char *)sqlite3_column_text(stmt, 5);
    if (gt) snprintf(out->github_token, sizeof(out->github_token), "%s", gt);

    const char *gl = (const char *)sqlite3_column_text(stmt, 6);
    if (gl) snprintf(out->github_login, sizeof(out->github_login), "%s", gl);

    return 0;
}

int db_user_find_by_email(const char *email, db_user_t *out)
{
    db_lock();
    const char *sql = "SELECT id, email, name, uva_session, dashboard_token, github_token, github_login "
                      "FROM users WHERE email = ?1";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { db_unlock(); return -1; }

    sqlite3_bind_text(stmt, 1, email, -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    int found = 0;
    if (rc == SQLITE_ROW) {
        fill_user(stmt, out);
        found = 1;
    }
    sqlite3_finalize(stmt);
    db_unlock();
    return found ? 0 : -1;
}

int db_user_find_by_token(const char *token, db_user_t *out)
{
    db_lock();
    const char *sql = "SELECT id, email, name, uva_session, dashboard_token, github_token, github_login "
                      "FROM users WHERE dashboard_token = ?1";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { db_unlock(); return -1; }

    sqlite3_bind_text(stmt, 1, token, -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    int found = 0;
    if (rc == SQLITE_ROW) {
        fill_user(stmt, out);
        found = 1;
    }
    sqlite3_finalize(stmt);
    db_unlock();
    return found ? 0 : -1;
}

int db_user_update_session(int64_t user_id, const char *session)
{
    db_lock();
    const char *sql = "UPDATE users SET uva_session = ?1 WHERE id = ?2";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { db_unlock(); return -1; }

    sqlite3_bind_text(stmt, 1, session, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, user_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    db_unlock();
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_user_set_token(int64_t user_id, const char *token)
{
    db_lock();
    const char *sql = "UPDATE users SET dashboard_token = ?1 WHERE id = ?2";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { db_unlock(); return -1; }

    sqlite3_bind_text(stmt, 1, token, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, user_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    db_unlock();
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_user_clear_token(int64_t user_id)
{
    db_lock();
    const char *sql = "UPDATE users SET dashboard_token = NULL WHERE id = ?1";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { db_unlock(); return -1; }

    sqlite3_bind_int64(stmt, 1, user_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    db_unlock();
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_user_set_github(int64_t user_id, const char *token, const char *login)
{
    db_lock();
    const char *sql = "UPDATE users SET github_token = ?1, "
                      "github_login = ?2 WHERE id = ?3";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { db_unlock(); return -1; }

    sqlite3_bind_text(stmt, 1, token, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, login, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, user_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    db_unlock();
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_user_clear_github(int64_t user_id)
{
    db_lock();
    const char *sql = "UPDATE users SET github_token = NULL, "
                      "github_login = NULL WHERE id = ?1";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { db_unlock(); return -1; }

    sqlite3_bind_int64(stmt, 1, user_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    db_unlock();
    return (rc == SQLITE_DONE) ? 0 : -1;
}
