#include "database.h"
#include "database_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int db_key_create(const db_api_key_t *key, int64_t *out_id)
{
    db_lock();
    const char *sql =
        "INSERT INTO api_keys (user_id, key_hash, key_prefix, name, model, "
        "temperature, top_p, max_tokens, frequency_penalty, presence_penalty, "
        "reasoning_effort, system_prompt, allow_tools, tool_allowlist, "
        "max_tokens_tool) "
        "VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15)";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "db_key_create: prepare: %s\n",
                sqlite3_errmsg(g_db));
        db_unlock();
        return -1;
    }

    sqlite3_bind_int64(stmt, 1, key->user_id);
    sqlite3_bind_text(stmt, 2, key->key_hash, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, key->key_prefix, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, key->name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, key->model, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 6, key->temperature);
    sqlite3_bind_double(stmt, 7, key->top_p);
    sqlite3_bind_int(stmt, 8, key->max_tokens);
    sqlite3_bind_double(stmt, 9, key->frequency_penalty);
    sqlite3_bind_double(stmt, 10, key->presence_penalty);
    sqlite3_bind_text(stmt, 11, key->reasoning_effort, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 12, key->system_prompt, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 13, key->allow_tools);
    sqlite3_bind_text(stmt, 14, key->tool_allowlist, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 15, key->max_tokens_tool > 0 ? key->max_tokens_tool : 8192);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE && out_id)
        *out_id = sqlite3_last_insert_rowid(g_db);

    sqlite3_finalize(stmt);
    db_unlock();
    return (rc == SQLITE_DONE) ? 0 : -1;
}

/*
 * Column order for SELECT queries (0-based):
 * 0  id, 1  user_id, 2  key_hash, 3  key_prefix, 4  name,
 * 5  model, 6  temperature, 7  top_p, 8  max_tokens,
 * 9  frequency_penalty, 10 presence_penalty, 11 reasoning_effort,
 * 12 system_prompt, 13 is_active,
 * 14 allow_tools, 15 tool_allowlist, 16 max_tokens_tool
 * [17 u.uva_session -- only in find_by_hash JOIN]
 */
static void fill_key(sqlite3_stmt *stmt, db_api_key_t *out)
{
    memset(out, 0, sizeof(*out));
    out->id = sqlite3_column_int64(stmt, 0);
    out->user_id = sqlite3_column_int64(stmt, 1);

    const char *s;
    s = (const char *)sqlite3_column_text(stmt, 2);
    if (s) snprintf(out->key_hash, sizeof(out->key_hash), "%s", s);
    s = (const char *)sqlite3_column_text(stmt, 3);
    if (s) snprintf(out->key_prefix, sizeof(out->key_prefix), "%s", s);
    s = (const char *)sqlite3_column_text(stmt, 4);
    if (s) snprintf(out->name, sizeof(out->name), "%s", s);
    s = (const char *)sqlite3_column_text(stmt, 5);
    if (s) snprintf(out->model, sizeof(out->model), "%s", s);

    out->temperature = sqlite3_column_double(stmt, 6);
    out->top_p = sqlite3_column_double(stmt, 7);
    out->max_tokens = sqlite3_column_int(stmt, 8);
    out->frequency_penalty = sqlite3_column_double(stmt, 9);
    out->presence_penalty = sqlite3_column_double(stmt, 10);

    s = (const char *)sqlite3_column_text(stmt, 11);
    if (s) snprintf(out->reasoning_effort, sizeof(out->reasoning_effort),
                    "%s", s);
    s = (const char *)sqlite3_column_text(stmt, 12);
    if (s) snprintf(out->system_prompt, sizeof(out->system_prompt), "%s", s);

    out->is_active = sqlite3_column_int(stmt, 13);

    /* Tool configuration columns (added via migration) */
    out->allow_tools = sqlite3_column_int(stmt, 14);
    /* Default to 1 if column returned NULL (old rows before migration) */
    if (sqlite3_column_type(stmt, 14) == SQLITE_NULL)
        out->allow_tools = 1;
    s = (const char *)sqlite3_column_text(stmt, 15);
    if (s) snprintf(out->tool_allowlist, sizeof(out->tool_allowlist), "%s", s);
    out->max_tokens_tool = sqlite3_column_int(stmt, 16);
    if (out->max_tokens_tool == 0)
        out->max_tokens_tool = 8192;
}

int db_key_find_by_hash(const char *hash, db_api_key_t *out)
{
    db_lock();
    const char *sql =
        "SELECT k.id, k.user_id, k.key_hash, k.key_prefix, k.name, "
        "k.model, k.temperature, k.top_p, k.max_tokens, "
        "k.frequency_penalty, k.presence_penalty, k.reasoning_effort, "
        "k.system_prompt, k.is_active, "
        "k.allow_tools, k.tool_allowlist, k.max_tokens_tool, "
        "u.uva_session "
        "FROM api_keys k JOIN users u ON k.user_id = u.id "
        "WHERE k.key_hash = ?1";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { db_unlock(); return -1; }

    sqlite3_bind_text(stmt, 1, hash, -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    int found = 0;
    if (rc == SQLITE_ROW) {
        fill_key(stmt, out);
        /* Column 17: user session from JOIN */
        const char *sess = (const char *)sqlite3_column_text(stmt, 17);
        if (sess)
            snprintf(out->user_session, sizeof(out->user_session), "%s", sess);
        found = 1;
    }
    sqlite3_finalize(stmt);
    db_unlock();
    return found ? 0 : -1;
}

int db_key_list_by_user(int64_t user_id, db_api_key_t **out, int *count)
{
    db_lock();
    const char *sql =
        "SELECT id, user_id, key_hash, key_prefix, name, model, "
        "temperature, top_p, max_tokens, frequency_penalty, "
        "presence_penalty, reasoning_effort, system_prompt, is_active, "
        "allow_tools, tool_allowlist, max_tokens_tool "
        "FROM api_keys WHERE user_id = ?1 ORDER BY id DESC";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { db_unlock(); *count = 0; return -1; }

    sqlite3_bind_int64(stmt, 1, user_id);

    /* First pass: count rows */
    int n = 0;
    int cap = 16;
    db_api_key_t *keys = malloc(sizeof(db_api_key_t) * (size_t)cap);

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (n >= cap) {
            cap *= 2;
            keys = realloc(keys, sizeof(db_api_key_t) * (size_t)cap);
        }
        fill_key(stmt, &keys[n]);
        n++;
    }

    sqlite3_finalize(stmt);
    db_unlock();

    *out = keys;
    *count = n;
    return 0;
}

int db_key_update(int64_t key_id, int64_t user_id, const db_api_key_t *key)
{
    db_lock();
    const char *sql =
        "UPDATE api_keys SET name=?1, model=?2, temperature=?3, "
        "top_p=?4, max_tokens=?5, frequency_penalty=?6, "
        "presence_penalty=?7, reasoning_effort=?8, system_prompt=?9, "
        "allow_tools=?10, tool_allowlist=?11, max_tokens_tool=?12 "
        "WHERE id=?13 AND user_id=?14";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { db_unlock(); return -1; }

    sqlite3_bind_text(stmt, 1, key->name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, key->model, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 3, key->temperature);
    sqlite3_bind_double(stmt, 4, key->top_p);
    sqlite3_bind_int(stmt, 5, key->max_tokens);
    sqlite3_bind_double(stmt, 6, key->frequency_penalty);
    sqlite3_bind_double(stmt, 7, key->presence_penalty);
    sqlite3_bind_text(stmt, 8, key->reasoning_effort, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 9, key->system_prompt, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 10, key->allow_tools);
    sqlite3_bind_text(stmt, 11, key->tool_allowlist, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 12, key->max_tokens_tool > 0 ? key->max_tokens_tool : 8192);
    sqlite3_bind_int64(stmt, 13, key_id);
    sqlite3_bind_int64(stmt, 14, user_id);

    rc = sqlite3_step(stmt);
    int changes = sqlite3_changes(g_db);
    sqlite3_finalize(stmt);
    db_unlock();
    return (rc == SQLITE_DONE && changes > 0) ? 0 : -1;
}

int db_key_delete(int64_t key_id, int64_t user_id)
{
    db_lock();
    const char *sql = "DELETE FROM api_keys WHERE id=?1 AND user_id=?2";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { db_unlock(); return -1; }

    sqlite3_bind_int64(stmt, 1, key_id);
    sqlite3_bind_int64(stmt, 2, user_id);
    rc = sqlite3_step(stmt);
    int changes = sqlite3_changes(g_db);
    sqlite3_finalize(stmt);
    db_unlock();
    return (rc == SQLITE_DONE && changes > 0) ? 0 : -1;
}

int db_key_toggle(int64_t key_id, int64_t user_id)
{
    db_lock();
    const char *sql =
        "UPDATE api_keys SET is_active = 1 - is_active "
        "WHERE id=?1 AND user_id=?2";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { db_unlock(); return -1; }

    sqlite3_bind_int64(stmt, 1, key_id);
    sqlite3_bind_int64(stmt, 2, user_id);
    rc = sqlite3_step(stmt);
    int changes = sqlite3_changes(g_db);
    sqlite3_finalize(stmt);
    db_unlock();
    return (rc == SQLITE_DONE && changes > 0) ? 0 : -1;
}

int db_key_touch(int64_t key_id)
{
    db_lock();
    const char *sql =
        "UPDATE api_keys SET last_used = datetime('now') WHERE id = ?1";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { db_unlock(); return -1; }

    sqlite3_bind_int64(stmt, 1, key_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    db_unlock();
    return (rc == SQLITE_DONE) ? 0 : -1;
}
