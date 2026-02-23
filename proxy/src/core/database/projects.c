#include "core/database/project.h"
#include "core/database/database.h"
#include "core/database/database_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *PROJECTS_SCHEMA =
    "CREATE TABLE IF NOT EXISTS projects ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,"
    "  name TEXT NOT NULL,"
    "  repo_full_name TEXT DEFAULT '',"
    "  model TEXT DEFAULT 'gpt-4.1',"
    "  instructions TEXT DEFAULT '',"
    "  api_key_plain TEXT DEFAULT '',"
    "  api_key_id INTEGER DEFAULT 0,"
    "  container_id TEXT DEFAULT '',"
    "  branch_name TEXT DEFAULT '',"
    "  status TEXT DEFAULT 'pending',"
    "  created_at TEXT DEFAULT (datetime('now')),"
    "  updated_at TEXT DEFAULT (datetime('now'))"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_projects_user ON projects(user_id);";

/* Migrations for existing databases that had the old schema */
static const char *PROJECT_MIGRATIONS[] = {
    "ALTER TABLE projects ADD COLUMN repo_full_name TEXT DEFAULT ''",
    "ALTER TABLE projects ADD COLUMN container_id TEXT DEFAULT ''",
    "ALTER TABLE projects ADD COLUMN branch_name TEXT DEFAULT ''",
    "ALTER TABLE projects ADD COLUMN status TEXT DEFAULT 'pending'",
    NULL
};

int db_project_init_schema(void)
{
    db_lock();
    char *err = NULL;
    int rc = sqlite3_exec(g_db, PROJECTS_SCHEMA, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "db_project_init_schema: %s\n", err);
        sqlite3_free(err);
        db_unlock();
        return -1;
    }
    /* Run idempotent migrations */
    for (int i = 0; PROJECT_MIGRATIONS[i]; i++)
        sqlite3_exec(g_db, PROJECT_MIGRATIONS[i], NULL, NULL, NULL);
    db_unlock();
    return 0;
}

#define PROJ_COLS "id, user_id, name, repo_full_name, model, instructions, " \
    "api_key_plain, api_key_id, container_id, branch_name, status, "         \
    "created_at, updated_at"

#define COL_STR(i, field) do { \
    const char *_s = (const char *)sqlite3_column_text(stmt, i); \
    if (_s) snprintf(p->field, sizeof(p->field), "%s", _s); \
} while(0)

static void row_to_project(sqlite3_stmt *stmt, db_project_t *p)
{
    memset(p, 0, sizeof(*p));
    p->id = sqlite3_column_int64(stmt, 0);
    p->user_id = sqlite3_column_int64(stmt, 1);
    COL_STR(2, name);           COL_STR(3, repo_full_name);
    COL_STR(4, model);          COL_STR(5, instructions);
    COL_STR(6, api_key_plain);
    p->api_key_id = sqlite3_column_int64(stmt, 7);
    COL_STR(8, container_id);   COL_STR(9, branch_name);
    COL_STR(10, status);        COL_STR(11, created_at);
    COL_STR(12, updated_at);
}

int db_project_create(const db_project_t *proj, int64_t *out_id)
{
    db_lock();
    const char *sql =
        "INSERT INTO projects (user_id, name, repo_full_name, model, "
        "instructions, api_key_plain, api_key_id, container_id, "
        "branch_name, status) VALUES (?,?,?,?,?,?,?,?,?,?)";

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "db_project_create: prepare: %s\n",
                sqlite3_errmsg(g_db));
        db_unlock();
        return -1;
    }

    sqlite3_bind_int64(stmt, 1, proj->user_id);
    sqlite3_bind_text(stmt, 2, proj->name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, proj->repo_full_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, proj->model, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, proj->instructions, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, proj->api_key_plain, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 7, proj->api_key_id);
    sqlite3_bind_text(stmt, 8, proj->container_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 9, proj->branch_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 10, proj->status, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "db_project_create: step: %s\n",
                sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        db_unlock();
        return -1;
    }

    *out_id = sqlite3_last_insert_rowid(g_db);
    sqlite3_finalize(stmt);
    db_unlock();
    return 0;
}

int db_project_list(int64_t user_id, db_project_t **out, int *count)
{
    *out = NULL;
    *count = 0;
    db_lock();

    char sql[256];
    snprintf(sql, sizeof(sql),
        "SELECT " PROJ_COLS " FROM projects "
        "WHERE user_id = ? ORDER BY id DESC");

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        db_unlock();
        return -1;
    }
    sqlite3_bind_int64(stmt, 1, user_id);

    int cap = 16;
    db_project_t *arr = malloc(sizeof(db_project_t) * (size_t)cap);
    if (!arr) { sqlite3_finalize(stmt); db_unlock(); return -1; }

    int n = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (n >= cap) {
            cap *= 2;
            db_project_t *tmp = realloc(arr,
                sizeof(db_project_t) * (size_t)cap);
            if (!tmp) break;
            arr = tmp;
        }
        row_to_project(stmt, &arr[n]);
        n++;
    }

    sqlite3_finalize(stmt);
    db_unlock();
    *out = arr;
    *count = n;
    return 0;
}

int db_project_get(int64_t id, int64_t user_id, db_project_t *out)
{
    db_lock();
    char sql[256];
    snprintf(sql, sizeof(sql),
        "SELECT " PROJ_COLS " FROM projects "
        "WHERE id = ? AND user_id = ?");

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        db_unlock();
        return -1;
    }
    sqlite3_bind_int64(stmt, 1, id);
    sqlite3_bind_int64(stmt, 2, user_id);

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        db_unlock();
        return -1;
    }

    row_to_project(stmt, out);
    sqlite3_finalize(stmt);
    db_unlock();
    return 0;
}

int db_project_update(int64_t id, int64_t user_id, const db_project_t *proj)
{
    db_lock();
    const char *sql =
        "UPDATE projects SET name = ?, repo_full_name = ?, model = ?, "
        "instructions = ?, updated_at = datetime('now') "
        "WHERE id = ? AND user_id = ?";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        db_unlock();
        return -1;
    }

    sqlite3_bind_text(stmt, 1, proj->name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, proj->repo_full_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, proj->model, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, proj->instructions, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 5, id);
    sqlite3_bind_int64(stmt, 6, user_id);

    int rc = sqlite3_step(stmt);
    int changes = sqlite3_changes(g_db);
    sqlite3_finalize(stmt);
    db_unlock();

    if (rc != SQLITE_DONE || changes == 0) return -1;
    return 0;
}

int db_project_set_container(int64_t id, const char *container_id,
                             const char *branch, const char *status)
{
    db_lock();
    const char *sql = "UPDATE projects SET container_id = ?, branch_name = ?, "
        "status = ?, updated_at = datetime('now') WHERE id = ?";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK)
        { db_unlock(); return -1; }
    sqlite3_bind_text(stmt, 1, container_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, branch, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, status, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    db_unlock();
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_project_set_status(int64_t id, const char *status)
{
    db_lock();
    const char *sql = "UPDATE projects SET status = ?, "
        "updated_at = datetime('now') WHERE id = ?";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK)
        { db_unlock(); return -1; }
    sqlite3_bind_text(stmt, 1, status, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    db_unlock();
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_project_delete(int64_t id, int64_t user_id)
{
    db_lock();

    /* Get the api_key_id so we can also delete the API key */
    const char *sel = "SELECT api_key_id FROM projects "
                      "WHERE id = ? AND user_id = ?";
    sqlite3_stmt *stmt;
    int64_t key_id = 0;

    if (sqlite3_prepare_v2(g_db, sel, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, id);
        sqlite3_bind_int64(stmt, 2, user_id);
        if (sqlite3_step(stmt) == SQLITE_ROW)
            key_id = sqlite3_column_int64(stmt, 0);
        sqlite3_finalize(stmt);
    }

    /* Delete the project */
    const char *del = "DELETE FROM projects WHERE id = ? AND user_id = ?";
    if (sqlite3_prepare_v2(g_db, del, -1, &stmt, NULL) != SQLITE_OK) {
        db_unlock();
        return -1;
    }
    sqlite3_bind_int64(stmt, 1, id);
    sqlite3_bind_int64(stmt, 2, user_id);
    sqlite3_step(stmt);
    int changes = sqlite3_changes(g_db);
    sqlite3_finalize(stmt);

    /* Delete the associated API key */
    if (key_id > 0) {
        const char *dk = "DELETE FROM api_keys WHERE id = ? AND user_id = ?";
        if (sqlite3_prepare_v2(g_db, dk, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int64(stmt, 1, key_id);
            sqlite3_bind_int64(stmt, 2, user_id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    db_unlock();
    return (changes > 0) ? 0 : -1;
}
