/*
 * sea_db.h — Embedded Database (The Ledger)
 *
 * SQLite-backed persistent storage. Single file, zero-config.
 * The agent's memory lives here: tasks, trajectory, config.
 *
 * "The Vault keeps its own records."
 */

#ifndef SEA_DB_H
#define SEA_DB_H

#include "sea_types.h"
#include "sea_arena.h"

/* Opaque handle */
typedef struct SeaDb SeaDb;

/* ── Lifecycle ────────────────────────────────────────────── */

/* Open (or create) the database at the given path.
 * Creates tables if they don't exist. */
SeaError sea_db_open(SeaDb** db, const char* path);

/* Close and flush */
void sea_db_close(SeaDb* db);

/* ── Trajectory (audit log) ───────────────────────────────── */

typedef struct {
    i32         id;
    const char* entry_type;
    const char* title;
    const char* content;
    const char* created_at;
} SeaDbEvent;

SeaError sea_db_log_event(SeaDb* db, const char* entry_type,
                          const char* title, const char* content);

/* Load last N events. Returns count loaded. */
i32 sea_db_recent_events(SeaDb* db, SeaDbEvent* out, i32 max_count,
                         SeaArena* arena);

/* ── Key-Value Config ─────────────────────────────────────── */

SeaError sea_db_config_set(SeaDb* db, const char* key, const char* value);

/* Returns value allocated in arena. NULL if not found. */
const char* sea_db_config_get(SeaDb* db, const char* key, SeaArena* arena);

/* ── Tasks ────────────────────────────────────────────────── */

typedef struct {
    i32         id;
    const char* title;
    const char* status;     /* pending, in_progress, completed */
    const char* priority;   /* low, medium, high, critical */
    const char* content;
} SeaDbTask;

SeaError sea_db_task_create(SeaDb* db, const char* title,
                            const char* priority, const char* content);

SeaError sea_db_task_update_status(SeaDb* db, i32 task_id, const char* status);

/* List tasks into arena. Returns count. */
i32 sea_db_task_list(SeaDb* db, const char* status_filter,
                     SeaDbTask* out, i32 max_count, SeaArena* arena);

/* ── Chat History ─────────────────────────────────────────── */

typedef struct {
    const char* role;       /* "user", "assistant", "system", "tool" */
    const char* content;
} SeaDbChatMsg;

SeaError sea_db_chat_log(SeaDb* db, i64 chat_id, const char* role,
                         const char* content);

/* Load last N messages for a chat. Returns count loaded. */
i32 sea_db_chat_history(SeaDb* db, i64 chat_id,
                        SeaDbChatMsg* out, i32 max_count, SeaArena* arena);

/* Clear chat history for a chat */
SeaError sea_db_chat_clear(SeaDb* db, i64 chat_id);

/* ── Raw SQL (escape hatch) ───────────────────────────────── */

SeaError sea_db_exec(SeaDb* db, const char* sql);

#endif /* SEA_DB_H */
