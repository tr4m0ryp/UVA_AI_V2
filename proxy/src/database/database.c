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
    ");"
    "CREATE TABLE IF NOT EXISTS request_logs ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  api_key_id INTEGER NOT NULL REFERENCES api_keys(id) ON DELETE CASCADE,"
    "  user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,"
    "  model TEXT NOT NULL,"
    "  prompt_tokens INTEGER DEFAULT 0,"
    "  completion_tokens INTEGER DEFAULT 0,"
    "  total_tokens INTEGER DEFAULT 0,"
    "  status INTEGER DEFAULT 200,"
    "  created_at TEXT DEFAULT (datetime('now'))"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_request_logs_key "
    "  ON request_logs(api_key_id);"
    "CREATE INDEX IF NOT EXISTS idx_request_logs_created "
    "  ON request_logs(created_at);"
    "CREATE TABLE IF NOT EXISTS grading_sessions ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,"
    "  name TEXT NOT NULL,"
    "  assignment_text TEXT DEFAULT '',"
    "  rubric_text TEXT DEFAULT '',"
    "  example_text TEXT DEFAULT '',"
    "  submission_count INTEGER DEFAULT 0,"
    "  avg_percentage REAL DEFAULT 0,"
    "  status TEXT DEFAULT 'in_progress',"
    "  created_at TEXT DEFAULT (datetime('now'))"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_grading_sessions_user "
    "  ON grading_sessions(user_id);"
    "CREATE TABLE IF NOT EXISTS grading_results ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  session_id INTEGER NOT NULL REFERENCES grading_sessions(id) ON DELETE CASCADE,"
    "  student_name TEXT NOT NULL,"
    "  student_id_str TEXT NOT NULL,"
    "  file_name TEXT DEFAULT '',"
    "  total_score REAL DEFAULT 0,"
    "  total_max REAL DEFAULT 0,"
    "  percentage REAL DEFAULT 0,"
    "  overall_feedback TEXT DEFAULT '',"
    "  criteria_json TEXT DEFAULT '[]',"
    "  status TEXT DEFAULT 'pending',"
    "  error_text TEXT DEFAULT '',"
    "  created_at TEXT DEFAULT (datetime('now'))"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_grading_results_session "
    "  ON grading_results(session_id);";

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

    /* Schema migrations -- idempotent ALTER TABLE statements */
    static const char *MIGRATIONS[] = {
        "ALTER TABLE users ADD COLUMN github_token TEXT DEFAULT NULL",
        "ALTER TABLE users ADD COLUMN github_login TEXT DEFAULT NULL",
        NULL
    };
    for (int i = 0; MIGRATIONS[i]; i++) {
        /* Ignore "duplicate column" errors */
        sqlite3_exec(g_db, MIGRATIONS[i], NULL, NULL, NULL);
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
