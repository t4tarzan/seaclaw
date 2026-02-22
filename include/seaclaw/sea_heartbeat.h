/*
 * sea_heartbeat.h — Proactive Agent Heartbeat
 *
 * Periodically scans HEARTBEAT.md for uncompleted tasks and
 * injects them into the agent loop via the message bus.
 * Turns SeaBot from a reactive assistant into a proactive one.
 *
 * HEARTBEAT.md format:
 *   - [ ] Check inbox and draft replies
 *   - [ ] Summarize today's meetings
 *   - [x] Already done item (skipped)
 *
 * Inspired by MimicLaw's heartbeat system.
 *
 * "The Vault doesn't wait to be asked. It acts."
 */

#ifndef SEA_HEARTBEAT_H
#define SEA_HEARTBEAT_H

#include "sea_types.h"
#include "sea_arena.h"
#include "sea_bus.h"
#include "sea_memory.h"

/* Forward declare */
typedef struct SeaDb SeaDb;

/* ── Configuration ────────────────────────────────────────── */

#define SEA_HEARTBEAT_FILE       "HEARTBEAT.md"
#define SEA_HEARTBEAT_MAX_TASKS  16
#define SEA_HEARTBEAT_TASK_MAX   512

/* Default interval: 30 minutes */
#define SEA_HEARTBEAT_DEFAULT_INTERVAL_SEC  1800

/* ── Task Structure ───────────────────────────────────────── */

typedef struct {
    char    text[SEA_HEARTBEAT_TASK_MAX];
    bool    completed;
    u32     line;       /* Line number in HEARTBEAT.md */
} SeaHeartbeatTask;

/* ── Heartbeat Manager ────────────────────────────────────── */

typedef struct {
    SeaMemory*  memory;         /* Memory system (for workspace path) */
    SeaBus*     bus;            /* Bus for injecting agent prompts    */
    SeaDb*      db;             /* Optional: if set, heartbeat events are logged */
    u64         interval_sec;   /* Seconds between heartbeat checks   */
    u64         last_check;     /* Epoch time of last check           */
    u32         total_checks;   /* Total heartbeat cycles             */
    u32         total_injected; /* Total tasks injected into agent    */
    bool        enabled;
} SeaHeartbeat;

/* ── API ──────────────────────────────────────────────────── */

/* Initialize the heartbeat system (in-memory logging only). */
void sea_heartbeat_init(SeaHeartbeat* hb, SeaMemory* memory,
                         SeaBus* bus, u64 interval_sec);

/* Initialize with SQLite logging. Creates heartbeat_log table. */
SeaError sea_heartbeat_init_db(SeaHeartbeat* hb, SeaMemory* memory,
                                SeaBus* bus, u64 interval_sec, SeaDb* db);

/* Parse HEARTBEAT.md and return uncompleted tasks.
 * Returns count of pending tasks found. */
u32 sea_heartbeat_parse(SeaHeartbeat* hb, SeaHeartbeatTask* out, u32 max);

/* Tick: check if interval has elapsed, parse tasks, inject into bus.
 * Returns number of tasks injected this tick (0 if not time yet). */
u32 sea_heartbeat_tick(SeaHeartbeat* hb);

/* Force an immediate heartbeat check regardless of interval. */
u32 sea_heartbeat_trigger(SeaHeartbeat* hb);

/* Mark a task as completed in HEARTBEAT.md (changes [ ] to [x]). */
SeaError sea_heartbeat_complete(SeaHeartbeat* hb, u32 task_line);

/* Enable/disable the heartbeat. */
void sea_heartbeat_enable(SeaHeartbeat* hb, bool enabled);

/* Get stats. */
u32 sea_heartbeat_check_count(const SeaHeartbeat* hb);
u32 sea_heartbeat_injected_count(const SeaHeartbeat* hb);

#endif /* SEA_HEARTBEAT_H */
