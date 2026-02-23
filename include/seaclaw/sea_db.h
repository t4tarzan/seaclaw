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

/* ── SeaZero v3: Agent Management ────────────────────────── */

typedef struct {
    const char* agent_id;
    const char* status;     /* stopped, starting, ready, busy, error */
    const char* container;
    i32         port;
    const char* provider;
    const char* model;
    const char* created_at;
    const char* last_seen;
} SeaDbAgent;

SeaError sea_db_sz_agent_register(SeaDb* db, const char* agent_id,
                                   const char* container, i32 port,
                                   const char* provider, const char* model);

SeaError sea_db_sz_agent_update_status(SeaDb* db, const char* agent_id,
                                        const char* status);

SeaError sea_db_sz_agent_heartbeat(SeaDb* db, const char* agent_id);

i32 sea_db_sz_agent_list(SeaDb* db, SeaDbAgent* out, i32 max_count,
                          SeaArena* arena);

/* ── SeaZero v3: Task Tracking ───────────────────────────── */

typedef struct {
    const char* task_id;
    const char* agent_id;
    i64         chat_id;
    const char* status;     /* pending, running, completed, failed, cancelled */
    const char* task_text;
    const char* result;
    const char* error;
    i32         steps_taken;
    f64         elapsed_sec;
    const char* created_at;
    const char* completed_at;
} SeaDbSzTask;

SeaError sea_db_sz_task_create(SeaDb* db, const char* task_id,
                                const char* agent_id, i64 chat_id,
                                const char* task_text, const char* context);

SeaError sea_db_sz_task_start(SeaDb* db, const char* task_id);

SeaError sea_db_sz_task_complete(SeaDb* db, const char* task_id,
                                  const char* result, const char* files,
                                  i32 steps_taken, f64 elapsed_sec);

SeaError sea_db_sz_task_fail(SeaDb* db, const char* task_id,
                              const char* error, f64 elapsed_sec);

i32 sea_db_sz_task_list(SeaDb* db, const char* status_filter,
                         SeaDbSzTask* out, i32 max_count, SeaArena* arena);

/* ── SeaZero v3: LLM Usage Tracking ─────────────────────── */

SeaError sea_db_sz_llm_log(SeaDb* db, const char* caller,
                            const char* provider, const char* model,
                            i32 tokens_in, i32 tokens_out,
                            f64 cost_usd, i32 latency_ms,
                            const char* status, const char* task_id);

/* Returns total tokens used by a caller. */
i64 sea_db_sz_llm_total_tokens(SeaDb* db, const char* caller);

/* ── SeaZero v3: Security Audit ──────────────────────────── */

SeaError sea_db_sz_audit(SeaDb* db, const char* event_type,
                          const char* source, const char* target,
                          const char* detail, const char* severity);

typedef struct {
    i32         id;
    const char* event_type;
    const char* source;
    const char* target;
    const char* detail;
    const char* severity;
    const char* created_at;
} SeaDbAuditEvent;

/* List recent audit events. Returns count. */
i32 sea_db_sz_audit_list(SeaDb* db, SeaDbAuditEvent* events, u32 max,
                          SeaArena* arena);

/* ── Usability Testing (E13–E17) ─────────────────────────── */

typedef struct {
    i32         id;
    const char* sprint;     /* E13, E14, E15, E16, E17 */
    const char* test_name;
    const char* category;   /* channel, telegram, streaming, multi_agent, gateway */
    const char* status;     /* pending, running, passed, failed, skipped */
    const char* input;
    const char* expected;
    const char* actual;
    i32         latency_ms;
    const char* error;
    const char* env;        /* docker, host */
    const char* created_at;
    const char* finished_at;
} SeaDbUTest;

SeaError sea_db_utest_log(SeaDb* db, const char* sprint,
                           const char* test_name, const char* category,
                           const char* input, const char* expected);

SeaError sea_db_utest_pass(SeaDb* db, i32 test_id,
                            const char* actual, i32 latency_ms);

SeaError sea_db_utest_fail(SeaDb* db, i32 test_id,
                            const char* actual, const char* error,
                            i32 latency_ms);

i32 sea_db_utest_list(SeaDb* db, const char* sprint_filter,
                       SeaDbUTest* out, i32 max_count, SeaArena* arena);

/* Summary: count passed/failed/pending for a sprint */
void sea_db_utest_summary(SeaDb* db, const char* sprint,
                           i32* passed, i32* failed, i32* pending);

#endif /* SEA_DB_H */
