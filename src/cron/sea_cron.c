/*
 * sea_cron.c — Persistent Cron Scheduler Implementation
 *
 * Tick-based scheduler: call sea_cron_tick() once per second.
 * Jobs are persisted to SQLite and survive restarts.
 */

#include "seaclaw/sea_cron.h"
#include "seaclaw/sea_log.h"
#include "seaclaw/sea_tools.h"

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ── Helpers ──────────────────────────────────────────────── */

/* ── Access internal DB handle ───────────────────────────── */

struct SeaDb { sqlite3* handle; };

static u64 now_epoch(void) {
    return (u64)time(NULL);
}

/* ── Schedule Parsing ─────────────────────────────────────── */

/* Parse interval string like "30s", "5m", "1h", "2d" → seconds */
static u64 parse_duration(const char* s) {
    if (!s || !*s) return 0;
    char* end = NULL;
    u64 val = (u64)strtoull(s, &end, 10);
    if (!end || !*end) return val; /* bare number = seconds */
    switch (*end) {
        case 's': return val;
        case 'm': return val * 60;
        case 'h': return val * 3600;
        case 'd': return val * 86400;
        default:  return val;
    }
}

/* Parse cron-style "min hour dom mon dow" into next_run.
 * Simplified: supports only interval-like patterns for now.
 * Full 5-field cron parsing is complex; we handle common cases. */
static u64 cron_next_from_expr(const char* expr) {
    /* Handle simple patterns:
     *   every-minute:  * * * * *
     *   every-5-min:   star-slash-5 * * * *
     *   hourly:        0 * * * *
     *   daily:         0 0 * * *
     */
    if (!expr) return 60;

    /* Check for star-slash-N pattern in minutes field */
    if (expr[0] == '*' && expr[1] == '/') {
        u64 mins = (u64)atoi(expr + 2);
        return mins > 0 ? mins * 60 : 60;
    }
    /* "* * * * *" → every minute */
    if (strcmp(expr, "* * * * *") == 0) return 60;
    /* "0 * * * *" → every hour */
    if (strncmp(expr, "0 *", 3) == 0) return 3600;
    /* "0 0 * * *" → daily */
    if (strncmp(expr, "0 0 *", 5) == 0) return 86400;

    /* Default: every 60 seconds */
    return 60;
}

SeaError sea_cron_parse_schedule(const char* schedule, SeaSchedType* out_type,
                                  u64* out_interval, u64* out_next_run) {
    if (!schedule || !out_type || !out_interval || !out_next_run) {
        return SEA_ERR_INVALID_INPUT;
    }

    u64 now = now_epoch();

    if (strncmp(schedule, "@every ", 7) == 0) {
        *out_type = SEA_SCHED_INTERVAL;
        *out_interval = parse_duration(schedule + 7);
        if (*out_interval == 0) return SEA_ERR_INVALID_INPUT;
        *out_next_run = now + *out_interval;
        return SEA_OK;
    }

    if (strncmp(schedule, "@once ", 6) == 0) {
        *out_type = SEA_SCHED_ONCE;
        *out_interval = parse_duration(schedule + 6);
        if (*out_interval == 0) return SEA_ERR_INVALID_INPUT;
        *out_next_run = now + *out_interval;
        return SEA_OK;
    }

    /* Standard cron expression */
    *out_type = SEA_SCHED_CRON;
    *out_interval = cron_next_from_expr(schedule);
    *out_next_run = now + *out_interval;
    return SEA_OK;
}

/* ── Init / Destroy ───────────────────────────────────────── */

SeaError sea_cron_init(SeaCronScheduler* sched, SeaDb* db, SeaBus* bus) {
    if (!sched) return SEA_ERR_INVALID_INPUT;

    memset(sched, 0, sizeof(SeaCronScheduler));
    sched->db = db;
    sched->bus = bus;
    sched->running = true;

    SeaError err = sea_arena_create(&sched->arena, 64 * 1024);
    if (err != SEA_OK) return err;

    /* Create DB tables */
    if (db) {
        sea_db_exec(db,
            "CREATE TABLE IF NOT EXISTS cron_jobs ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  name TEXT NOT NULL,"
            "  type INTEGER NOT NULL,"
            "  state INTEGER DEFAULT 0,"
            "  sched_type INTEGER NOT NULL,"
            "  schedule TEXT NOT NULL,"
            "  command TEXT NOT NULL,"
            "  args TEXT DEFAULT '',"
            "  interval_sec INTEGER DEFAULT 0,"
            "  next_run INTEGER DEFAULT 0,"
            "  last_run INTEGER DEFAULT 0,"
            "  run_count INTEGER DEFAULT 0,"
            "  fail_count INTEGER DEFAULT 0,"
            "  created_at INTEGER NOT NULL"
            ");"
            "CREATE TABLE IF NOT EXISTS cron_log ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  job_id INTEGER NOT NULL,"
            "  job_name TEXT,"
            "  status TEXT NOT NULL,"
            "  output TEXT,"
            "  executed_at INTEGER NOT NULL,"
            "  duration_ms INTEGER DEFAULT 0"
            ");"
        );
    }

    SEA_LOG_INFO("CRON", "Scheduler initialized");
    return SEA_OK;
}

void sea_cron_destroy(SeaCronScheduler* sched) {
    if (!sched) return;
    sched->running = false;
    sea_cron_save(sched);
    sea_arena_destroy(&sched->arena);
    SEA_LOG_INFO("CRON", "Scheduler destroyed (%u jobs, %llu ticks)",
                 sched->count, (unsigned long long)sched->tick_count);
}

/* ── Add Job ──────────────────────────────────────────────── */

i32 sea_cron_add(SeaCronScheduler* sched, const char* name,
                  SeaCronJobType type, const char* schedule,
                  const char* command, const char* args) {
    if (!sched || !name || !schedule || !command) return -1;
    if (sched->count >= SEA_MAX_CRON_JOBS) return -1;

    SeaSchedType stype;
    u64 interval, next_run;
    if (sea_cron_parse_schedule(schedule, &stype, &interval, &next_run) != SEA_OK) {
        return -1;
    }

    SeaCronJob* job = &sched->jobs[sched->count];
    memset(job, 0, sizeof(SeaCronJob));

    job->id = (i32)sched->count + 1;
    strncpy(job->name, name, SEA_CRON_NAME_MAX - 1);
    job->type = type;
    job->state = SEA_CRON_ACTIVE;
    job->sched_type = stype;
    strncpy(job->schedule, schedule, SEA_CRON_EXPR_MAX - 1);
    strncpy(job->command, command, SEA_CRON_CMD_MAX - 1);
    if (args) strncpy(job->args, args, SEA_CRON_CMD_MAX - 1);
    job->interval_sec = interval;
    job->next_run = next_run;
    job->created_at = now_epoch();

    sched->count++;

    SEA_LOG_INFO("CRON", "Added job #%d '%s' [%s] next=%llu",
                 job->id, job->name, job->schedule,
                 (unsigned long long)job->next_run);

    /* Persist to DB */
    if (sched->db) {
        char sql[2048];
        snprintf(sql, sizeof(sql),
            "INSERT INTO cron_jobs (name, type, state, sched_type, schedule, "
            "command, args, interval_sec, next_run, last_run, run_count, "
            "fail_count, created_at) VALUES "
            "('%s', %d, %d, %d, '%s', '%s', '%s', %llu, %llu, 0, 0, 0, %llu);",
            job->name, (int)job->type, (int)job->state, (int)job->sched_type,
            job->schedule, job->command, job->args,
            (unsigned long long)job->interval_sec,
            (unsigned long long)job->next_run,
            (unsigned long long)job->created_at);
        sea_db_exec(sched->db, sql);
    }

    return job->id;
}

/* ── Remove Job ───────────────────────────────────────────── */

SeaError sea_cron_remove(SeaCronScheduler* sched, i32 job_id) {
    if (!sched) return SEA_ERR_INVALID_INPUT;

    for (u32 i = 0; i < sched->count; i++) {
        if (sched->jobs[i].id == job_id) {
            SEA_LOG_INFO("CRON", "Removed job #%d '%s'",
                         job_id, sched->jobs[i].name);
            /* Shift remaining */
            for (u32 j = i; j < sched->count - 1; j++) {
                sched->jobs[j] = sched->jobs[j + 1];
            }
            sched->count--;

            if (sched->db) {
                char sql[128];
                snprintf(sql, sizeof(sql),
                    "DELETE FROM cron_jobs WHERE id = %d;", job_id);
                sea_db_exec(sched->db, sql);
            }
            return SEA_OK;
        }
    }
    return SEA_ERR_NOT_FOUND;
}

/* ── Pause / Resume ───────────────────────────────────────── */

SeaError sea_cron_pause(SeaCronScheduler* sched, i32 job_id) {
    SeaCronJob* job = sea_cron_get(sched, job_id);
    if (!job) return SEA_ERR_NOT_FOUND;
    job->state = SEA_CRON_PAUSED;
    SEA_LOG_INFO("CRON", "Paused job #%d '%s'", job_id, job->name);
    return SEA_OK;
}

SeaError sea_cron_resume(SeaCronScheduler* sched, i32 job_id) {
    SeaCronJob* job = sea_cron_get(sched, job_id);
    if (!job) return SEA_ERR_NOT_FOUND;
    job->state = SEA_CRON_ACTIVE;
    job->next_run = now_epoch() + job->interval_sec;
    SEA_LOG_INFO("CRON", "Resumed job #%d '%s'", job_id, job->name);
    return SEA_OK;
}

/* ── Execute a single job ─────────────────────────────────── */

static void execute_job(SeaCronScheduler* sched, SeaCronJob* job) {
    u64 start = now_epoch();
    bool success = true;
    const char* output = "ok";

    SEA_LOG_INFO("CRON", "Executing job #%d '%s' [%s]",
                 job->id, job->name, job->command);

    switch (job->type) {
        case SEA_CRON_SHELL: {
            /* Execute shell command and capture exit code */
            int rc = system(job->command);
            if (rc != 0) {
                success = false;
                output = "non-zero exit";
            }
            break;
        }
        case SEA_CRON_TOOL: {
            /* Call a registered tool */
            SeaSlice args_slice = {
                .data = (const u8*)job->args,
                .len = (u32)strlen(job->args)
            };
            SeaSlice result;
            sea_arena_reset(&sched->arena);
            SeaError err = sea_tool_exec(job->command, args_slice,
                                          &sched->arena, &result);
            if (err != SEA_OK) {
                success = false;
                output = "tool exec failed";
            }
            break;
        }
        case SEA_CRON_BUS_MSG: {
            /* Publish a message to the bus */
            if (sched->bus) {
                /* args format: "channel:chat_id" */
                i64 chat_id = 0;
                const char* colon = strchr(job->args, ':');
                char channel[32] = "system";
                if (colon) {
                    u32 clen = (u32)(colon - job->args);
                    if (clen > 0 && clen < sizeof(channel)) {
                        memcpy(channel, job->args, clen);
                        channel[clen] = '\0';
                    }
                    chat_id = atoll(colon + 1);
                }
                sea_bus_publish_inbound(sched->bus, SEA_MSG_SYSTEM,
                                         channel, "cron", chat_id,
                                         job->command, (u32)strlen(job->command));
            } else {
                success = false;
                output = "no bus";
            }
            break;
        }
        case SEA_CRON_AGENT: {
            /* Inject prompt into agent loop via bus */
            if (sched->bus) {
                char prompt[SEA_CRON_CMD_MAX + 64];
                snprintf(prompt, sizeof(prompt),
                         "[Cron:%.63s] %.500s", job->name, job->command);
                sea_bus_publish_inbound(sched->bus, SEA_MSG_SYSTEM,
                                         "cron-agent", "cron", 0,
                                         prompt, (u32)strlen(prompt));
                SEA_LOG_INFO("CRON", "Agent prompt injected: %.80s", job->command);
            } else {
                success = false;
                output = "no bus";
            }
            break;
        }
    }

    u64 duration = now_epoch() - start;
    job->last_run = now_epoch();
    job->run_count++;
    if (!success) job->fail_count++;

    /* Schedule next run */
    if (job->sched_type == SEA_SCHED_ONCE) {
        job->state = SEA_CRON_COMPLETED;
    } else {
        job->next_run = now_epoch() + job->interval_sec;
    }

    /* Log execution */
    if (sched->db) {
        char sql[1024];
        snprintf(sql, sizeof(sql),
            "INSERT INTO cron_log (job_id, job_name, status, output, "
            "executed_at, duration_ms) VALUES (%d, '%s', '%s', '%s', %llu, %llu);",
            job->id, job->name, success ? "ok" : "error", output,
            (unsigned long long)job->last_run,
            (unsigned long long)(duration * 1000));
        sea_db_exec(sched->db, sql);
    }

    SEA_LOG_INFO("CRON", "Job #%d '%s' %s (run #%u, %llus)",
                 job->id, job->name, success ? "OK" : "FAILED",
                 job->run_count, (unsigned long long)duration);
}

/* ── Tick ─────────────────────────────────────────────────── */

u32 sea_cron_tick(SeaCronScheduler* sched) {
    if (!sched || !sched->running) return 0;

    sched->tick_count++;
    u64 now = now_epoch();
    u32 executed = 0;

    for (u32 i = 0; i < sched->count; i++) {
        SeaCronJob* job = &sched->jobs[i];
        if (job->state != SEA_CRON_ACTIVE) continue;
        if (now < job->next_run) continue;

        execute_job(sched, job);
        executed++;
    }

    return executed;
}

/* ── Lookup ───────────────────────────────────────────────── */

SeaCronJob* sea_cron_get(SeaCronScheduler* sched, i32 job_id) {
    if (!sched) return NULL;
    for (u32 i = 0; i < sched->count; i++) {
        if (sched->jobs[i].id == job_id) return &sched->jobs[i];
    }
    return NULL;
}

u32 sea_cron_list(SeaCronScheduler* sched, SeaCronJob** out, u32 max_count) {
    if (!sched || !out) return 0;
    u32 count = sched->count < max_count ? sched->count : max_count;
    for (u32 i = 0; i < count; i++) {
        out[i] = &sched->jobs[i];
    }
    return count;
}

u32 sea_cron_count(SeaCronScheduler* sched) {
    return sched ? sched->count : 0;
}

/* ── Save / Load ──────────────────────────────────────────── */

SeaError sea_cron_save(SeaCronScheduler* sched) {
    if (!sched || !sched->db) return SEA_ERR_CONFIG;

    for (u32 i = 0; i < sched->count; i++) {
        SeaCronJob* j = &sched->jobs[i];
        char sql[2048];
        snprintf(sql, sizeof(sql),
            "INSERT OR REPLACE INTO cron_jobs (id, name, type, state, sched_type, "
            "schedule, command, args, interval_sec, next_run, last_run, "
            "run_count, fail_count, created_at) VALUES "
            "(%d, '%s', %d, %d, %d, '%s', '%s', '%s', %llu, %llu, %llu, %u, %u, %llu);",
            j->id, j->name, (int)j->type, (int)j->state, (int)j->sched_type,
            j->schedule, j->command, j->args,
            (unsigned long long)j->interval_sec,
            (unsigned long long)j->next_run,
            (unsigned long long)j->last_run,
            j->run_count, j->fail_count,
            (unsigned long long)j->created_at);
        sea_db_exec(sched->db, sql);
    }

    SEA_LOG_INFO("CRON", "Saved %u jobs to DB", sched->count);
    return SEA_OK;
}

/* Callback context for loading cron jobs */
typedef struct {
    SeaCronScheduler* sched;
    u32 loaded;
} CronLoadCtx;

static int cron_row_cb(void* ctx, int ncols, char** vals, char** cols) {
    CronLoadCtx* lc = (CronLoadCtx*)ctx;
    if (lc->sched->count >= SEA_MAX_CRON_JOBS) return 0;

    SeaCronJob* j = &lc->sched->jobs[lc->sched->count];
    memset(j, 0, sizeof(*j));

    for (int i = 0; i < ncols; i++) {
        if (!vals[i]) continue;
        if (strcmp(cols[i], "id") == 0)            j->id = atoi(vals[i]);
        else if (strcmp(cols[i], "name") == 0)     strncpy(j->name, vals[i], SEA_CRON_NAME_MAX - 1);
        else if (strcmp(cols[i], "type") == 0)     j->type = (SeaCronJobType)atoi(vals[i]);
        else if (strcmp(cols[i], "state") == 0)    j->state = (SeaCronJobState)atoi(vals[i]);
        else if (strcmp(cols[i], "sched_type") == 0) j->sched_type = (SeaSchedType)atoi(vals[i]);
        else if (strcmp(cols[i], "schedule") == 0) strncpy(j->schedule, vals[i], SEA_CRON_EXPR_MAX - 1);
        else if (strcmp(cols[i], "command") == 0)  strncpy(j->command, vals[i], SEA_CRON_CMD_MAX - 1);
        else if (strcmp(cols[i], "args") == 0)     strncpy(j->args, vals[i], SEA_CRON_CMD_MAX - 1);
        else if (strcmp(cols[i], "interval_sec") == 0) j->interval_sec = (u64)atoll(vals[i]);
        else if (strcmp(cols[i], "next_run") == 0)     j->next_run = (u64)atoll(vals[i]);
        else if (strcmp(cols[i], "last_run") == 0)     j->last_run = (u64)atoll(vals[i]);
        else if (strcmp(cols[i], "run_count") == 0)    j->run_count = (u32)atoi(vals[i]);
        else if (strcmp(cols[i], "fail_count") == 0)   j->fail_count = (u32)atoi(vals[i]);
        else if (strcmp(cols[i], "created_at") == 0)   j->created_at = (u64)atoll(vals[i]);
    }

    lc->sched->count++;
    lc->loaded++;
    return 0;
}

SeaError sea_cron_load(SeaCronScheduler* sched) {
    if (!sched || !sched->db) return SEA_ERR_CONFIG;

    sched->count = 0; /* Reset before loading */
    CronLoadCtx ctx = { .sched = sched, .loaded = 0 };
    sqlite3_exec(sched->db->handle,
        "SELECT * FROM cron_jobs ORDER BY id ASC;",
        cron_row_cb, &ctx, NULL);

    SEA_LOG_INFO("CRON", "Loaded %u jobs from DB", ctx.loaded);
    return SEA_OK;
}
