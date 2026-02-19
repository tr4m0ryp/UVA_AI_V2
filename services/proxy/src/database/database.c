#include "database.h"
#include "database_internal.h"
#include <stdio.h>
#include <string.h>

sqlite3       *g_db = NULL;
pthread_mutex_t g_db_lock = PTHREAD_MUTEX_INITIALIZER;

void db_lock(void)   { pthread_mutex_lock(&g_db_lock); }
void db_unlock(void) { pthread_mutex_unlock(&g_db_lock); }

static const char *SCHEMA_SQL =
    "CREATE TABLE IF NOT EXISTS users ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  email TEXT UNIQUE NOT NULL,"
    "  name TEXT,"
    "  uva_session TEXT NOT NULL,"
    "  dashboard_token TEXT UNIQUE,"
    "  created_at TEXT DEFAULT (datetime('now')),"
    "  last_login TEXT DEFAULT (datetime('now'))"
    ");"
    "CREATE TABLE IF NOT EXISTS api_keys ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,"
    "  key_hash TEXT UNIQUE NOT NULL,"
    "  key_prefix TEXT NOT NULL,"
    "  name TEXT DEFAULT '',"
    "  model TEXT NOT NULL,"
    "  temperature REAL DEFAULT 0.5,"
    "  top_p REAL DEFAULT 0.5,"
    "  max_tokens INTEGER DEFAULT 4096,"
    "  frequency_penalty REAL DEFAULT 0.0,"
    "  presence_penalty REAL DEFAULT 0.0,"
    "  reasoning_effort TEXT DEFAULT 'medium',"
    "  system_prompt TEXT DEFAULT '',"
    "  created_at TEXT DEFAULT (datetime('now')),"
    "  last_used TEXT,"
    "  is_active INTEGER DEFAULT 1"
    ");";

int db_init(const char *path)
{
    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                SQLITE_OPEN_FULLMUTEX;
    int rc = sqlite3_open_v2(path, &g_db, flags, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "db_init: failed to open %s: %s\n",
                path, sqlite3_errmsg(g_db));
        return -1;
    }

    /* Enable WAL mode for concurrent reads */
    char *err = NULL;
    sqlite3_exec(g_db, "PRAGMA journal_mode=WAL;", NULL, NULL, &err);
    if (err) { sqlite3_free(err); err = NULL; }

    /* Enable foreign keys */
    sqlite3_exec(g_db, "PRAGMA foreign_keys=ON;", NULL, NULL, &err);
    if (err) { sqlite3_free(err); err = NULL; }

    /* Create schema */
    rc = sqlite3_exec(g_db, SCHEMA_SQL, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "db_init: schema error: %s\n", err);
        sqlite3_free(err);
        sqlite3_close(g_db);
        g_db = NULL;
        return -1;
    }

    fprintf(stderr, "Database initialized: %s\n", path);
    return 0;
}

void db_close(void)
{
    db_lock();
    if (g_db) {
        sqlite3_close(g_db);
        g_db = NULL;
    }
    db_unlock();
}
