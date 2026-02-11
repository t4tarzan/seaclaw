/*
 * sea_cron.h — Persistent Cron Scheduler
 *
 * Background job scheduler with cron-expression timing,
 * SQLite-backed persistence, and bus integration.
 * Jobs survive restarts. Execution results are logged.
 *
 * Supports:
 *   - Standard cron expressions (min hour dom mon dow)
 *   - Interval-based scheduling (@every 5m, @every 1h)
 *   - One-shot delayed execution (@once 30s)
 *   - Job types: shell command, tool call, bus message
 *
 * "The clock never stops. The Vault keeps its schedule."
 */

#ifndef SEA_CRON_H
#define SEA_CRON_H

#include "sea_types.h"
#include "sea_arena.h"
#include "sea_db.h"
#include "sea_bus.h"

/* ── Job Types ────────────────────────────────────────────── */

typedef enum {
    SEA_CRON_SHELL = 0,     /* Execute a shell command          */
    SEA_CRON_TOOL,          /* Call a registered tool            */
    SEA_CRON_BUS_MSG,       /* Publish a message to the bus     */
} SeaCronJobType;

typedef enum {
    SEA_CRON_ACTIVE = 0,
    SEA_CRON_PAUSED,
    SEA_CRON_COMPLETED,     /* One-shot job that has fired      */
    SEA_CRON_FAILED,
} SeaCronJobState;

/* ── Schedule Types ───────────────────────────────────────── */

typedef enum {
    SEA_SCHED_CRON = 0,     /* Standard cron expression         */
    SEA_SCHED_INTERVAL,     /* @every Ns/Nm/Nh                  */
    SEA_SCHED_ONCE,         /* @once Ns (fire once after delay) */
} SeaSchedType;

/* ── Cron Job ─────────────────────────────────────────────── */

#define SEA_CRON_NAME_MAX    64
#define SEA_CRON_EXPR_MAX    64
#define SEA_CRON_CMD_MAX     512

typedef struct {
    i32             id;
    char            name[SEA_CRON_NAME_MAX];
    SeaCronJobType  type;
    SeaCronJobState state;
    SeaSchedType    sched_type;
    char            schedule[SEA_CRON_EXPR_MAX];  /* cron expr or @every/@once     */
    char            command[SEA_CRON_CMD_MAX];     /* Shell cmd, tool name, or bus msg */
    char            args[SEA_CRON_CMD_MAX];        /* Tool args or bus channel:chat_id */
    u64             interval_sec;                  /* Computed interval in seconds     */
    u64             next_run;                      /* Next execution time (epoch sec)  */
    u64             last_run;                      /* Last execution time              */
    u32             run_count;                     /* Total executions                 */
    u32             fail_count;                    /* Total failures                   */
    u64             created_at;
} SeaCronJob;

/* ── Cron Scheduler ───────────────────────────────────────── */

#define SEA_MAX_CRON_JOBS 64

typedef struct {
    SeaCronJob  jobs[SEA_MAX_CRON_JOBS];
    u32         count;
    SeaDb*      db;
    SeaBus*     bus;        /* Optional: for SEA_CRON_BUS_MSG jobs */
    SeaArena    arena;
    bool        running;
    u64         tick_count;
} SeaCronScheduler;

/* ── API ──────────────────────────────────────────────────── */

/* Initialize the scheduler. Creates DB tables if needed. */
SeaError sea_cron_init(SeaCronScheduler* sched, SeaDb* db, SeaBus* bus);

/* Destroy the scheduler. */
void sea_cron_destroy(SeaCronScheduler* sched);

/* Add a new job. Returns the job ID (>= 0) or -1 on error. */
i32 sea_cron_add(SeaCronScheduler* sched, const char* name,
                  SeaCronJobType type, const char* schedule,
                  const char* command, const char* args);

/* Remove a job by ID. */
SeaError sea_cron_remove(SeaCronScheduler* sched, i32 job_id);

/* Pause a job. */
SeaError sea_cron_pause(SeaCronScheduler* sched, i32 job_id);

/* Resume a paused job. */
SeaError sea_cron_resume(SeaCronScheduler* sched, i32 job_id);

/* Tick: check all jobs and execute any that are due.
 * Call this once per second from a dedicated thread.
 * Returns the number of jobs executed this tick. */
u32 sea_cron_tick(SeaCronScheduler* sched);

/* Get a job by ID. Returns NULL if not found. */
SeaCronJob* sea_cron_get(SeaCronScheduler* sched, i32 job_id);

/* List all jobs. Returns count. */
u32 sea_cron_list(SeaCronScheduler* sched, SeaCronJob** out, u32 max_count);

/* Get job count. */
u32 sea_cron_count(SeaCronScheduler* sched);

/* Save all jobs to DB. */
SeaError sea_cron_save(SeaCronScheduler* sched);

/* Load jobs from DB. */
SeaError sea_cron_load(SeaCronScheduler* sched);

/* Parse a schedule string and compute the interval/next_run.
 * Exported for testing. */
SeaError sea_cron_parse_schedule(const char* schedule, SeaSchedType* out_type,
                                  u64* out_interval, u64* out_next_run);

#endif /* SEA_CRON_H */
