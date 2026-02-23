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
    "CREATE INDEX IF NOT EXISTS idx_trajectory_type ON trajectory(entry_type);"

    /* ── SeaZero v3 Tables ─────────────────────────────────── */

    "CREATE TABLE IF NOT EXISTS schema_version ("
    "  version    TEXT NOT NULL,"
    "  applied_at DATETIME DEFAULT (datetime('now'))"
    ");"

    "CREATE TABLE IF NOT EXISTS seazero_agents ("
    "  id         INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  agent_id   TEXT NOT NULL UNIQUE,"
    "  status     TEXT NOT NULL DEFAULT 'stopped',"
    "  container  TEXT,"
    "  port       INTEGER,"
    "  provider   TEXT,"
    "  model      TEXT,"
    "  created_at DATETIME DEFAULT (datetime('now')),"
    "  last_seen  DATETIME DEFAULT (datetime('now'))"
    ");"

    "CREATE TABLE IF NOT EXISTS seazero_tasks ("
    "  id           INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  task_id      TEXT NOT NULL UNIQUE,"
    "  agent_id     TEXT NOT NULL,"
    "  chat_id      INTEGER,"
    "  status       TEXT NOT NULL DEFAULT 'pending',"
    "  task_text    TEXT NOT NULL,"
    "  context      TEXT,"
    "  result       TEXT,"
    "  error        TEXT,"
    "  steps_taken  INTEGER DEFAULT 0,"
    "  files        TEXT,"
    "  created_at   DATETIME DEFAULT (datetime('now')),"
    "  started_at   DATETIME,"
    "  completed_at DATETIME,"
    "  elapsed_sec  REAL DEFAULT 0"
    ");"

    "CREATE TABLE IF NOT EXISTS seazero_llm_usage ("
    "  id         INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  caller     TEXT NOT NULL,"
    "  provider   TEXT NOT NULL,"
    "  model      TEXT NOT NULL,"
    "  tokens_in  INTEGER DEFAULT 0,"
    "  tokens_out INTEGER DEFAULT 0,"
    "  cost_usd   REAL DEFAULT 0,"
    "  latency_ms INTEGER DEFAULT 0,"
    "  status     TEXT DEFAULT 'ok',"
    "  task_id    TEXT,"
    "  created_at DATETIME DEFAULT (datetime('now'))"
    ");"

    "CREATE TABLE IF NOT EXISTS seazero_audit ("
    "  id         INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  event_type TEXT NOT NULL,"
    "  source     TEXT NOT NULL,"
    "  target     TEXT,"
    "  detail     TEXT,"
    "  severity   TEXT DEFAULT 'info',"
    "  created_at DATETIME DEFAULT (datetime('now'))"
    ");"

    "CREATE INDEX IF NOT EXISTS idx_sz_tasks_status ON seazero_tasks(status);"
    "CREATE INDEX IF NOT EXISTS idx_sz_tasks_agent ON seazero_tasks(agent_id);"
    "CREATE INDEX IF NOT EXISTS idx_sz_tasks_chat ON seazero_tasks(chat_id);"
    "CREATE INDEX IF NOT EXISTS idx_sz_llm_caller ON seazero_llm_usage(caller);"
    "CREATE INDEX IF NOT EXISTS idx_sz_llm_task ON seazero_llm_usage(task_id);"
    "CREATE INDEX IF NOT EXISTS idx_sz_audit_type ON seazero_audit(event_type);"
    "CREATE INDEX IF NOT EXISTS idx_sz_audit_source ON seazero_audit(source);"
    "CREATE INDEX IF NOT EXISTS idx_sz_agents_status ON seazero_agents(status);"

    /* ── Usability Testing (E13–E17) ─────────────────────── */

    "CREATE TABLE IF NOT EXISTS usability_tests ("
    "  id          INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  sprint      TEXT NOT NULL,"
    "  test_name   TEXT NOT NULL,"
    "  category    TEXT NOT NULL,"
    "  status      TEXT NOT NULL DEFAULT 'pending',"
    "  input       TEXT,"
    "  expected    TEXT,"
    "  actual      TEXT,"
    "  latency_ms  INTEGER DEFAULT 0,"
    "  error       TEXT,"
    "  env         TEXT DEFAULT 'docker',"
    "  created_at  DATETIME DEFAULT (datetime('now')),"
    "  finished_at DATETIME"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_ut_sprint ON usability_tests(sprint);"
    "CREATE INDEX IF NOT EXISTS idx_ut_status ON usability_tests(status);"
    "CREATE INDEX IF NOT EXISTS idx_ut_category ON usability_tests(category);";

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

i32 sea_db_recent_events(SeaDb* db, SeaDbEvent* out, i32 max_count,
                         SeaArena* arena) {
    if (!db || !out || !arena || max_count <= 0) return 0;

    sqlite3_stmt* stmt;
    const char* sql =
        "SELECT id, entry_type, title, content, created_at FROM ("
        "  SELECT id, entry_type, title, content, created_at FROM trajectory"
        "  ORDER BY id DESC LIMIT ?"
        ") ORDER BY id DESC";

    int rc = sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return 0;

    sqlite3_bind_int(stmt, 1, max_count);

    i32 count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_count) {
        out[count].id         = sqlite3_column_int(stmt, 0);
        out[count].entry_type = arena_strdup(arena,
                                  (const char*)sqlite3_column_text(stmt, 1));
        out[count].title      = arena_strdup(arena,
                                  (const char*)sqlite3_column_text(stmt, 2));
        out[count].content    = arena_strdup(arena,
                                  (const char*)sqlite3_column_text(stmt, 3));
        out[count].created_at = arena_strdup(arena,
                                  (const char*)sqlite3_column_text(stmt, 4));
        count++;
    }

    sqlite3_finalize(stmt);
    return count;
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

i32 sea_db_chat_history(SeaDb* db, i64 chat_id,
                        SeaDbChatMsg* out, i32 max_count, SeaArena* arena) {
    if (!db || !out || !arena || max_count <= 0) return 0;

    sqlite3_stmt* stmt;
    /* Select last N messages in chronological order using a subquery */
    const char* sql =
        "SELECT role, content FROM ("
        "  SELECT role, content, id FROM chat_history"
        "  WHERE chat_id = ? ORDER BY id DESC LIMIT ?"
        ") ORDER BY id ASC";

    int rc = sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return 0;

    sqlite3_bind_int64(stmt, 1, chat_id);
    sqlite3_bind_int(stmt, 2, max_count);

    i32 count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_count) {
        out[count].role    = arena_strdup(arena,
                               (const char*)sqlite3_column_text(stmt, 0));
        out[count].content = arena_strdup(arena,
                               (const char*)sqlite3_column_text(stmt, 1));
        count++;
    }

    sqlite3_finalize(stmt);
    return count;
}

SeaError sea_db_chat_clear(SeaDb* db, i64 chat_id) {
    if (!db) return SEA_ERR_IO;

    sqlite3_stmt* stmt;
    const char* sql = "DELETE FROM chat_history WHERE chat_id = ?";

    int rc = sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return SEA_ERR_IO;

    sqlite3_bind_int64(stmt, 1, chat_id);
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

/* ── SeaZero v3: Agent Management ────────────────────────── */

SeaError sea_db_sz_agent_register(SeaDb* db, const char* agent_id,
                                   const char* container, i32 port,
                                   const char* provider, const char* model) {
    if (!db || !agent_id) return SEA_ERR_IO;

    sqlite3_stmt* stmt;
    const char* sql =
        "INSERT INTO seazero_agents (agent_id, status, container, port, provider, model) "
        "VALUES (?, 'ready', ?, ?, ?, ?) "
        "ON CONFLICT(agent_id) DO UPDATE SET "
        "status='ready', container=excluded.container, port=excluded.port, "
        "provider=excluded.provider, model=excluded.model, last_seen=datetime('now')";

    int rc = sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return SEA_ERR_IO;

    sqlite3_bind_text(stmt, 1, agent_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, container, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, port);
    sqlite3_bind_text(stmt, 4, provider ? provider : "", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, model ? model : "", -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? SEA_OK : SEA_ERR_IO;
}

SeaError sea_db_sz_agent_update_status(SeaDb* db, const char* agent_id,
                                        const char* status) {
    if (!db || !agent_id || !status) return SEA_ERR_IO;

    sqlite3_stmt* stmt;
    const char* sql =
        "UPDATE seazero_agents SET status=?, last_seen=datetime('now') WHERE agent_id=?";

    int rc = sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return SEA_ERR_IO;

    sqlite3_bind_text(stmt, 1, status, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, agent_id, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? SEA_OK : SEA_ERR_IO;
}

SeaError sea_db_sz_agent_heartbeat(SeaDb* db, const char* agent_id) {
    if (!db || !agent_id) return SEA_ERR_IO;

    sqlite3_stmt* stmt;
    const char* sql =
        "UPDATE seazero_agents SET last_seen=datetime('now') WHERE agent_id=?";

    int rc = sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return SEA_ERR_IO;

    sqlite3_bind_text(stmt, 1, agent_id, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? SEA_OK : SEA_ERR_IO;
}

i32 sea_db_sz_agent_list(SeaDb* db, SeaDbAgent* out, i32 max_count,
                          SeaArena* arena) {
    if (!db || !out || !arena || max_count <= 0) return 0;

    sqlite3_stmt* stmt;
    const char* sql =
        "SELECT agent_id, status, container, port, provider, model, "
        "created_at, last_seen FROM seazero_agents ORDER BY id";

    int rc = sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return 0;

    i32 count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_count) {
        out[count].agent_id   = arena_strdup(arena, (const char*)sqlite3_column_text(stmt, 0));
        out[count].status     = arena_strdup(arena, (const char*)sqlite3_column_text(stmt, 1));
        out[count].container  = arena_strdup(arena, (const char*)sqlite3_column_text(stmt, 2));
        out[count].port       = sqlite3_column_int(stmt, 3);
        out[count].provider   = arena_strdup(arena, (const char*)sqlite3_column_text(stmt, 4));
        out[count].model      = arena_strdup(arena, (const char*)sqlite3_column_text(stmt, 5));
        out[count].created_at = arena_strdup(arena, (const char*)sqlite3_column_text(stmt, 6));
        out[count].last_seen  = arena_strdup(arena, (const char*)sqlite3_column_text(stmt, 7));
        count++;
    }

    sqlite3_finalize(stmt);
    return count;
}

/* ── SeaZero v3: Task Tracking ───────────────────────────── */

SeaError sea_db_sz_task_create(SeaDb* db, const char* task_id,
                                const char* agent_id, i64 chat_id,
                                const char* task_text, const char* context) {
    if (!db || !task_id || !agent_id || !task_text) return SEA_ERR_IO;

    sqlite3_stmt* stmt;
    const char* sql =
        "INSERT INTO seazero_tasks (task_id, agent_id, chat_id, task_text, context) "
        "VALUES (?, ?, ?, ?, ?)";

    int rc = sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return SEA_ERR_IO;

    sqlite3_bind_text(stmt, 1, task_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, agent_id, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, chat_id);
    sqlite3_bind_text(stmt, 4, task_text, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, context, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? SEA_OK : SEA_ERR_IO;
}

SeaError sea_db_sz_task_start(SeaDb* db, const char* task_id) {
    if (!db || !task_id) return SEA_ERR_IO;

    sqlite3_stmt* stmt;
    const char* sql =
        "UPDATE seazero_tasks SET status='running', started_at=datetime('now') "
        "WHERE task_id=?";

    int rc = sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return SEA_ERR_IO;

    sqlite3_bind_text(stmt, 1, task_id, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? SEA_OK : SEA_ERR_IO;
}

SeaError sea_db_sz_task_complete(SeaDb* db, const char* task_id,
                                  const char* result, const char* files,
                                  i32 steps_taken, f64 elapsed_sec) {
    if (!db || !task_id) return SEA_ERR_IO;

    sqlite3_stmt* stmt;
    const char* sql =
        "UPDATE seazero_tasks SET status='completed', result=?, files=?, "
        "steps_taken=?, elapsed_sec=?, completed_at=datetime('now') "
        "WHERE task_id=?";

    int rc = sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return SEA_ERR_IO;

    sqlite3_bind_text(stmt, 1, result, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, files, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, steps_taken);
    sqlite3_bind_double(stmt, 4, elapsed_sec);
    sqlite3_bind_text(stmt, 5, task_id, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? SEA_OK : SEA_ERR_IO;
}

SeaError sea_db_sz_task_fail(SeaDb* db, const char* task_id,
                              const char* error, f64 elapsed_sec) {
    if (!db || !task_id) return SEA_ERR_IO;

    sqlite3_stmt* stmt;
    const char* sql =
        "UPDATE seazero_tasks SET status='failed', error=?, elapsed_sec=?, "
        "completed_at=datetime('now') WHERE task_id=?";

    int rc = sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return SEA_ERR_IO;

    sqlite3_bind_text(stmt, 1, error, -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 2, elapsed_sec);
    sqlite3_bind_text(stmt, 3, task_id, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? SEA_OK : SEA_ERR_IO;
}

i32 sea_db_sz_task_list(SeaDb* db, const char* status_filter,
                         SeaDbSzTask* out, i32 max_count, SeaArena* arena) {
    if (!db || !out || !arena || max_count <= 0) return 0;

    sqlite3_stmt* stmt;
    const char* sql;

    if (status_filter) {
        sql = "SELECT task_id, agent_id, chat_id, status, task_text, result, error, "
              "steps_taken, elapsed_sec, created_at, completed_at "
              "FROM seazero_tasks WHERE status=? ORDER BY id DESC LIMIT ?";
    } else {
        sql = "SELECT task_id, agent_id, chat_id, status, task_text, result, error, "
              "steps_taken, elapsed_sec, created_at, completed_at "
              "FROM seazero_tasks ORDER BY id DESC LIMIT ?";
    }

    int rc = sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return 0;

    int bind_idx = 1;
    if (status_filter) {
        sqlite3_bind_text(stmt, bind_idx++, status_filter, -1, SQLITE_STATIC);
    }
    sqlite3_bind_int(stmt, bind_idx, max_count);

    i32 count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_count) {
        out[count].task_id      = arena_strdup(arena, (const char*)sqlite3_column_text(stmt, 0));
        out[count].agent_id     = arena_strdup(arena, (const char*)sqlite3_column_text(stmt, 1));
        out[count].chat_id      = sqlite3_column_int64(stmt, 2);
        out[count].status       = arena_strdup(arena, (const char*)sqlite3_column_text(stmt, 3));
        out[count].task_text    = arena_strdup(arena, (const char*)sqlite3_column_text(stmt, 4));
        out[count].result       = arena_strdup(arena, (const char*)sqlite3_column_text(stmt, 5));
        out[count].error        = arena_strdup(arena, (const char*)sqlite3_column_text(stmt, 6));
        out[count].steps_taken  = sqlite3_column_int(stmt, 7);
        out[count].elapsed_sec  = sqlite3_column_double(stmt, 8);
        out[count].created_at   = arena_strdup(arena, (const char*)sqlite3_column_text(stmt, 9));
        out[count].completed_at = arena_strdup(arena, (const char*)sqlite3_column_text(stmt, 10));
        count++;
    }

    sqlite3_finalize(stmt);
    return count;
}

/* ── SeaZero v3: LLM Usage Tracking ─────────────────────── */

SeaError sea_db_sz_llm_log(SeaDb* db, const char* caller,
                            const char* provider, const char* model,
                            i32 tokens_in, i32 tokens_out,
                            f64 cost_usd, i32 latency_ms,
                            const char* status, const char* task_id) {
    if (!db || !caller || !provider || !model) return SEA_ERR_IO;

    sqlite3_stmt* stmt;
    const char* sql =
        "INSERT INTO seazero_llm_usage "
        "(caller, provider, model, tokens_in, tokens_out, cost_usd, latency_ms, status, task_id) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)";

    int rc = sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return SEA_ERR_IO;

    sqlite3_bind_text(stmt, 1, caller, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, provider, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, model, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, tokens_in);
    sqlite3_bind_int(stmt, 5, tokens_out);
    sqlite3_bind_double(stmt, 6, cost_usd);
    sqlite3_bind_int(stmt, 7, latency_ms);
    sqlite3_bind_text(stmt, 8, status ? status : "ok", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 9, task_id, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? SEA_OK : SEA_ERR_IO;
}

i64 sea_db_sz_llm_total_tokens(SeaDb* db, const char* caller) {
    if (!db || !caller) return 0;

    sqlite3_stmt* stmt;
    const char* sql =
        "SELECT COALESCE(SUM(tokens_in + tokens_out), 0) FROM seazero_llm_usage "
        "WHERE caller=? AND created_at >= date('now')";

    int rc = sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return 0;

    sqlite3_bind_text(stmt, 1, caller, -1, SQLITE_STATIC);

    i64 total = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        total = sqlite3_column_int64(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return total;
}

/* ── SeaZero v3: Security Audit ──────────────────────────── */

SeaError sea_db_sz_audit(SeaDb* db, const char* event_type,
                          const char* source, const char* target,
                          const char* detail, const char* severity) {
    if (!db || !event_type || !source) return SEA_ERR_IO;

    sqlite3_stmt* stmt;
    const char* sql =
        "INSERT INTO seazero_audit (event_type, source, target, detail, severity) "
        "VALUES (?, ?, ?, ?, ?)";

    int rc = sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return SEA_ERR_IO;

    sqlite3_bind_text(stmt, 1, event_type, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, source, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, target, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, detail, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, severity ? severity : "info", -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? SEA_OK : SEA_ERR_IO;
}

i32 sea_db_sz_audit_list(SeaDb* db, SeaDbAuditEvent* events, u32 max,
                          SeaArena* arena) {
    if (!db || !events || max == 0 || !arena) return 0;

    sqlite3_stmt* stmt;
    const char* sql =
        "SELECT id, event_type, source, target, detail, severity, created_at "
        "FROM seazero_audit ORDER BY id DESC LIMIT ?";

    int rc = sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return 0;

    sqlite3_bind_int(stmt, 1, (int)max);

    i32 count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && (u32)count < max) {
        events[count].id = sqlite3_column_int(stmt, 0);

        const char* col_vals[6];
        col_vals[0] = (const char*)sqlite3_column_text(stmt, 1);
        col_vals[1] = (const char*)sqlite3_column_text(stmt, 2);
        col_vals[2] = (const char*)sqlite3_column_text(stmt, 3);
        col_vals[3] = (const char*)sqlite3_column_text(stmt, 4);
        col_vals[4] = (const char*)sqlite3_column_text(stmt, 5);
        col_vals[5] = (const char*)sqlite3_column_text(stmt, 6);

        const char** dest_ptrs[6];
        dest_ptrs[0] = &events[count].event_type;
        dest_ptrs[1] = &events[count].source;
        dest_ptrs[2] = &events[count].target;
        dest_ptrs[3] = &events[count].detail;
        dest_ptrs[4] = &events[count].severity;
        dest_ptrs[5] = &events[count].created_at;

        for (int ci = 0; ci < 6; ci++) {
            if (col_vals[ci]) {
                u64 slen = strlen(col_vals[ci]);
                char* s = (char*)sea_arena_push(arena, slen + 1);
                if (s) { memcpy(s, col_vals[ci], slen + 1); *dest_ptrs[ci] = s; }
                else { *dest_ptrs[ci] = NULL; }
            } else {
                *dest_ptrs[ci] = NULL;
            }
        }
        count++;
    }

    sqlite3_finalize(stmt);
    return count;
}

/* ── Usability Testing (E13–E17) ────────────────────────── */

SeaError sea_db_utest_log(SeaDb* db, const char* sprint,
                           const char* test_name, const char* category,
                           const char* input, const char* expected) {
    if (!db || !sprint || !test_name || !category) return SEA_ERR_IO;

    sqlite3_stmt* stmt;
    const char* sql =
        "INSERT INTO usability_tests (sprint, test_name, category, status, input, expected) "
        "VALUES (?, ?, ?, 'running', ?, ?)";

    int rc = sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return SEA_ERR_IO;

    sqlite3_bind_text(stmt, 1, sprint, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, test_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, category, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, input, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, expected, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? SEA_OK : SEA_ERR_IO;
}

SeaError sea_db_utest_pass(SeaDb* db, i32 test_id,
                            const char* actual, i32 latency_ms) {
    if (!db) return SEA_ERR_IO;

    sqlite3_stmt* stmt;
    const char* sql =
        "UPDATE usability_tests SET status='passed', actual=?, latency_ms=?, "
        "finished_at=datetime('now') WHERE id=?";

    int rc = sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return SEA_ERR_IO;

    sqlite3_bind_text(stmt, 1, actual, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, latency_ms);
    sqlite3_bind_int(stmt, 3, test_id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? SEA_OK : SEA_ERR_IO;
}

SeaError sea_db_utest_fail(SeaDb* db, i32 test_id,
                            const char* actual, const char* error,
                            i32 latency_ms) {
    if (!db) return SEA_ERR_IO;

    sqlite3_stmt* stmt;
    const char* sql =
        "UPDATE usability_tests SET status='failed', actual=?, error=?, latency_ms=?, "
        "finished_at=datetime('now') WHERE id=?";

    int rc = sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return SEA_ERR_IO;

    sqlite3_bind_text(stmt, 1, actual, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, error, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, latency_ms);
    sqlite3_bind_int(stmt, 4, test_id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? SEA_OK : SEA_ERR_IO;
}

static const char* utest_arena_dup(SeaArena* arena, const char* src) {
    if (!src) return NULL;
    u64 len = strlen(src);
    char* s = (char*)sea_arena_push(arena, len + 1);
    if (!s) return NULL;
    memcpy(s, src, len + 1);
    return s;
}

i32 sea_db_utest_list(SeaDb* db, const char* sprint_filter,
                       SeaDbUTest* out, i32 max_count, SeaArena* arena) {
    if (!db || !out || max_count <= 0 || !arena) return 0;

    sqlite3_stmt* stmt;
    const char* sql;
    if (sprint_filter && *sprint_filter) {
        sql = "SELECT id, sprint, test_name, category, status, input, expected, "
              "actual, latency_ms, error, env, created_at, finished_at "
              "FROM usability_tests WHERE sprint=? ORDER BY id DESC LIMIT ?";
    } else {
        sql = "SELECT id, sprint, test_name, category, status, input, expected, "
              "actual, latency_ms, error, env, created_at, finished_at "
              "FROM usability_tests ORDER BY id DESC LIMIT ?";
    }

    int rc = sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return 0;

    int bind_idx = 1;
    if (sprint_filter && *sprint_filter) {
        sqlite3_bind_text(stmt, bind_idx++, sprint_filter, -1, SQLITE_STATIC);
    }
    sqlite3_bind_int(stmt, bind_idx, max_count);

    i32 count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_count) {
        out[count].id         = sqlite3_column_int(stmt, 0);
        out[count].sprint     = utest_arena_dup(arena, (const char*)sqlite3_column_text(stmt, 1));
        out[count].test_name  = utest_arena_dup(arena, (const char*)sqlite3_column_text(stmt, 2));
        out[count].category   = utest_arena_dup(arena, (const char*)sqlite3_column_text(stmt, 3));
        out[count].status     = utest_arena_dup(arena, (const char*)sqlite3_column_text(stmt, 4));
        out[count].input      = utest_arena_dup(arena, (const char*)sqlite3_column_text(stmt, 5));
        out[count].expected   = utest_arena_dup(arena, (const char*)sqlite3_column_text(stmt, 6));
        out[count].actual     = utest_arena_dup(arena, (const char*)sqlite3_column_text(stmt, 7));
        out[count].latency_ms = sqlite3_column_int(stmt, 8);
        out[count].error      = utest_arena_dup(arena, (const char*)sqlite3_column_text(stmt, 9));
        out[count].env        = utest_arena_dup(arena, (const char*)sqlite3_column_text(stmt, 10));
        out[count].created_at = utest_arena_dup(arena, (const char*)sqlite3_column_text(stmt, 11));
        out[count].finished_at= utest_arena_dup(arena, (const char*)sqlite3_column_text(stmt, 12));
        count++;
    }

    sqlite3_finalize(stmt);
    return count;
}

void sea_db_utest_summary(SeaDb* db, const char* sprint,
                           i32* passed, i32* failed, i32* pending) {
    if (!db) return;
    *passed = *failed = *pending = 0;

    sqlite3_stmt* stmt;
    const char* sql;
    if (sprint && *sprint) {
        sql = "SELECT status, COUNT(*) FROM usability_tests WHERE sprint=? GROUP BY status";
    } else {
        sql = "SELECT status, COUNT(*) FROM usability_tests GROUP BY status";
    }

    int rc = sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return;

    if (sprint && *sprint) {
        sqlite3_bind_text(stmt, 1, sprint, -1, SQLITE_STATIC);
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* st = (const char*)sqlite3_column_text(stmt, 0);
        i32 cnt = sqlite3_column_int(stmt, 1);
        if (!st) continue;
        if (strcmp(st, "passed") == 0) *passed = cnt;
        else if (strcmp(st, "failed") == 0) *failed = cnt;
        else *pending += cnt;
    }

    sqlite3_finalize(stmt);
}
