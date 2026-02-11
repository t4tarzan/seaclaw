/*
 * sea_db.c — Embedded SQLite database
 *
 * Single-file persistent storage for the agent.
 * All strings returned via arena allocation.
 */

#include "seaclaw/sea_db.h"
#include "seaclaw/sea_log.h"
#include <sqlite3.h>
#include <string.h>
#include <stdlib.h>

/* Copy a C string into arena, returning a const char* (null-terminated). */
static const char* arena_strdup(SeaArena* arena, const char* s) {
    if (!s) return NULL;
    u64 len = strlen(s) + 1;
    char* p = (char*)sea_arena_alloc(arena, len, 1);
    if (!p) return NULL;
    memcpy(p, s, len);
    return p;
}

struct SeaDb {
    sqlite3* handle;
};

/* ── Schema ───────────────────────────────────────────────── */

static const char* SCHEMA_SQL =
    "CREATE TABLE IF NOT EXISTS config ("
    "  key   TEXT PRIMARY KEY,"
    "  value TEXT NOT NULL,"
    "  updated_at DATETIME DEFAULT (datetime('now'))"
    ");"
    "CREATE TABLE IF NOT EXISTS trajectory ("
    "  id         INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  entry_type TEXT NOT NULL,"
    "  title      TEXT NOT NULL,"
    "  content    TEXT NOT NULL,"
    "  created_at DATETIME DEFAULT (datetime('now'))"
    ");"
    "CREATE TABLE IF NOT EXISTS tasks ("
    "  id       INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  title    TEXT NOT NULL,"
    "  status   TEXT NOT NULL DEFAULT 'pending',"
    "  priority TEXT NOT NULL DEFAULT 'medium',"
    "  content  TEXT,"
    "  created_at  DATETIME DEFAULT (datetime('now')),"
    "  updated_at  DATETIME DEFAULT (datetime('now'))"
    ");"
    "CREATE TABLE IF NOT EXISTS chat_history ("
    "  id       INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  chat_id  INTEGER NOT NULL,"
    "  role     TEXT NOT NULL,"
    "  content  TEXT NOT NULL,"
    "  created_at DATETIME DEFAULT (datetime('now'))"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_tasks_status ON tasks(status);"
    "CREATE INDEX IF NOT EXISTS idx_chat_history_chat ON chat_history(chat_id);"
    "CREATE INDEX IF NOT EXISTS idx_trajectory_type ON trajectory(entry_type);";

/* ── Lifecycle ────────────────────────────────────────────── */

SeaError sea_db_open(SeaDb** db, const char* path) {
    if (!db || !path) return SEA_ERR_CONFIG;

    *db = malloc(sizeof(SeaDb));
    if (!*db) return SEA_ERR_OOM;

    int rc = sqlite3_open(path, &(*db)->handle);
    if (rc != SQLITE_OK) {
        SEA_LOG_ERROR("DB", "Failed to open %s: %s", path, sqlite3_errmsg((*db)->handle));
        free(*db);
        *db = NULL;
        return SEA_ERR_IO;
    }

    /* Enable WAL mode for better concurrent read performance */
    sqlite3_exec((*db)->handle, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
    sqlite3_exec((*db)->handle, "PRAGMA synchronous=NORMAL;", NULL, NULL, NULL);
    sqlite3_exec((*db)->handle, "PRAGMA foreign_keys=ON;", NULL, NULL, NULL);

    /* Create schema */
    char* errmsg = NULL;
    rc = sqlite3_exec((*db)->handle, SCHEMA_SQL, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        SEA_LOG_ERROR("DB", "Schema creation failed: %s", errmsg ? errmsg : "unknown");
        sqlite3_free(errmsg);
        sqlite3_close((*db)->handle);
        free(*db);
        *db = NULL;
        return SEA_ERR_IO;
    }

    SEA_LOG_INFO("DB", "Opened database: %s", path);
    return SEA_OK;
}

void sea_db_close(SeaDb* db) {
    if (!db) return;
    if (db->handle) {
        sqlite3_close(db->handle);
        SEA_LOG_INFO("DB", "Database closed.");
    }
    free(db);
}

/* ── Trajectory ───────────────────────────────────────────── */

SeaError sea_db_log_event(SeaDb* db, const char* entry_type,
                          const char* title, const char* content) {
    if (!db || !entry_type || !title || !content) return SEA_ERR_IO;

    sqlite3_stmt* stmt;
    const char* sql = "INSERT INTO trajectory (entry_type, title, content) VALUES (?, ?, ?)";

    int rc = sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return SEA_ERR_IO;

    sqlite3_bind_text(stmt, 1, entry_type, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, title, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, content, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        SEA_LOG_ERROR("DB", "log_event failed: %s", sqlite3_errmsg(db->handle));
        return SEA_ERR_IO;
    }

    return SEA_OK;
}

/* ── Config ───────────────────────────────────────────────── */

SeaError sea_db_config_set(SeaDb* db, const char* key, const char* value) {
    if (!db || !key || !value) return SEA_ERR_IO;

    sqlite3_stmt* stmt;
    const char* sql =
        "INSERT INTO config (key, value, updated_at) VALUES (?, ?, datetime('now')) "
        "ON CONFLICT(key) DO UPDATE SET value = excluded.value, updated_at = datetime('now')";

    int rc = sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return SEA_ERR_IO;

    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, value, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? SEA_OK : SEA_ERR_IO;
}

const char* sea_db_config_get(SeaDb* db, const char* key, SeaArena* arena) {
    if (!db || !key || !arena) return NULL;

    sqlite3_stmt* stmt;
    const char* sql = "SELECT value FROM config WHERE key = ?";

    int rc = sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return NULL;

    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);

    const char* result = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* val = (const char*)sqlite3_column_text(stmt, 0);
        if (val) {
            result = arena_strdup(arena, val);
        }
    }

    sqlite3_finalize(stmt);
    return result;
}

/* ── Tasks ────────────────────────────────────────────────── */

SeaError sea_db_task_create(SeaDb* db, const char* title,
                            const char* priority, const char* content) {
    if (!db || !title) return SEA_ERR_IO;

    sqlite3_stmt* stmt;
    const char* sql =
        "INSERT INTO tasks (title, priority, content) VALUES (?, ?, ?)";

    int rc = sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return SEA_ERR_IO;

    sqlite3_bind_text(stmt, 1, title, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, priority ? priority : "medium", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, content ? content : "", -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? SEA_OK : SEA_ERR_IO;
}

SeaError sea_db_task_update_status(SeaDb* db, i32 task_id, const char* status) {
    if (!db || !status) return SEA_ERR_IO;

    sqlite3_stmt* stmt;
    const char* sql =
        "UPDATE tasks SET status = ?, updated_at = datetime('now') WHERE id = ?";

    int rc = sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return SEA_ERR_IO;

    sqlite3_bind_text(stmt, 1, status, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, task_id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? SEA_OK : SEA_ERR_IO;
}

i32 sea_db_task_list(SeaDb* db, const char* status_filter,
                     SeaDbTask* out, i32 max_count, SeaArena* arena) {
    if (!db || !out || !arena || max_count <= 0) return 0;

    sqlite3_stmt* stmt;
    const char* sql;

    if (status_filter) {
        sql = "SELECT id, title, status, priority, content FROM tasks "
              "WHERE status = ? ORDER BY id";
    } else {
        sql = "SELECT id, title, status, priority, content FROM tasks "
              "ORDER BY id";
    }

    int rc = sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return 0;

    if (status_filter) {
        sqlite3_bind_text(stmt, 1, status_filter, -1, SQLITE_STATIC);
    }

    i32 count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_count) {
        out[count].id       = sqlite3_column_int(stmt, 0);
        out[count].title    = arena_strdup(arena,
                                (const char*)sqlite3_column_text(stmt, 1));
        out[count].status   = arena_strdup(arena,
                                (const char*)sqlite3_column_text(stmt, 2));
        out[count].priority = arena_strdup(arena,
                                (const char*)sqlite3_column_text(stmt, 3));
        const char* c = (const char*)sqlite3_column_text(stmt, 4);
        out[count].content  = c ? arena_strdup(arena, c) : "";
        count++;
    }

    sqlite3_finalize(stmt);
    return count;
}

/* ── Chat History ─────────────────────────────────────────── */

SeaError sea_db_chat_log(SeaDb* db, i64 chat_id, const char* role,
                         const char* content) {
    if (!db || !role || !content) return SEA_ERR_IO;

    sqlite3_stmt* stmt;
    const char* sql =
        "INSERT INTO chat_history (chat_id, role, content) VALUES (?, ?, ?)";

    int rc = sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return SEA_ERR_IO;

    sqlite3_bind_int64(stmt, 1, chat_id);
    sqlite3_bind_text(stmt, 2, role, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, content, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? SEA_OK : SEA_ERR_IO;
}

/* ── Raw SQL ──────────────────────────────────────────────── */

SeaError sea_db_exec(SeaDb* db, const char* sql) {
    if (!db || !sql) return SEA_ERR_IO;

    char* errmsg = NULL;
    int rc = sqlite3_exec(db->handle, sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        SEA_LOG_ERROR("DB", "exec failed: %s", errmsg ? errmsg : "unknown");
        sqlite3_free(errmsg);
        return SEA_ERR_IO;
    }

    return SEA_OK;
}
