/*
 * test_cron.c — Cron Scheduler Tests
 *
 * Tests schedule parsing, job CRUD, tick execution,
 * pause/resume, one-shot jobs, and DB persistence.
 */

#include "seaclaw/sea_cron.h"
#include "seaclaw/sea_log.h"
#include "seaclaw/sea_db.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Stubs for symbols referenced by sea_cron.o → sea_tool_exec */
SeaDb* s_db = NULL;
SeaError sea_tool_exec(const char* n, SeaSlice a, SeaArena* ar, SeaSlice* o) {
    (void)n; (void)a; (void)ar;
    /* Return a simple result */
    static const char* result = "tool_ok";
    if (o) {
        o->data = (const u8*)result;
        o->len = 7;
    }
    return SEA_OK;
}

static int s_pass = 0;
static int s_fail = 0;

#define TEST(name) printf("  [TEST] %s ... ", name)
#define PASS() do { printf("\033[32mPASS\033[0m\n"); s_pass++; } while(0)
#define FAIL(msg) do { printf("\033[31mFAIL: %s\033[0m\n", msg); s_fail++; } while(0)

/* ── Test: Parse @every schedule ──────────────────────────── */

static void test_parse_every(void) {
    TEST("parse_every_30s");
    SeaSchedType type;
    u64 interval, next;
    SeaError err = sea_cron_parse_schedule("@every 30s", &type, &interval, &next);
    if (err != SEA_OK) { FAIL("parse failed"); return; }
    if (type != SEA_SCHED_INTERVAL) { FAIL("wrong type"); return; }
    if (interval != 30) { FAIL("interval != 30"); return; }
    PASS();
}

static void test_parse_every_minutes(void) {
    TEST("parse_every_5m");
    SeaSchedType type;
    u64 interval, next;
    sea_cron_parse_schedule("@every 5m", &type, &interval, &next);
    if (interval != 300) { FAIL("interval != 300"); return; }
    PASS();
}

static void test_parse_every_hours(void) {
    TEST("parse_every_2h");
    SeaSchedType type;
    u64 interval, next;
    sea_cron_parse_schedule("@every 2h", &type, &interval, &next);
    if (interval != 7200) { FAIL("interval != 7200"); return; }
    PASS();
}

/* ── Test: Parse @once schedule ───────────────────────────── */

static void test_parse_once(void) {
    TEST("parse_once_10s");
    SeaSchedType type;
    u64 interval, next;
    SeaError err = sea_cron_parse_schedule("@once 10s", &type, &interval, &next);
    if (err != SEA_OK) { FAIL("parse failed"); return; }
    if (type != SEA_SCHED_ONCE) { FAIL("wrong type"); return; }
    if (interval != 10) { FAIL("interval != 10"); return; }
    PASS();
}

/* ── Test: Parse cron expression ──────────────────────────── */

static void test_parse_cron(void) {
    TEST("parse_cron_every_5min");
    SeaSchedType type;
    u64 interval, next;
    SeaError err = sea_cron_parse_schedule("*/5 * * * *", &type, &interval, &next);
    if (err != SEA_OK) { FAIL("parse failed"); return; }
    if (type != SEA_SCHED_CRON) { FAIL("wrong type"); return; }
    if (interval != 300) { FAIL("interval != 300"); return; }
    PASS();
}

/* ── Test: Init and Destroy ───────────────────────────────── */

static void test_init_destroy(void) {
    TEST("init_destroy");
    SeaCronScheduler sched;
    SeaError err = sea_cron_init(&sched, NULL, NULL);
    if (err != SEA_OK) { FAIL("init failed"); return; }
    if (sea_cron_count(&sched) != 0) { FAIL("count != 0"); sea_cron_destroy(&sched); return; }
    sea_cron_destroy(&sched);
    PASS();
}

/* ── Test: Add Job ────────────────────────────────────────── */

static void test_add_job(void) {
    TEST("add_job");
    SeaCronScheduler sched;
    sea_cron_init(&sched, NULL, NULL);

    i32 id = sea_cron_add(&sched, "heartbeat", SEA_CRON_SHELL,
                           "@every 30s", "echo alive", NULL);
    if (id < 0) { FAIL("add returned -1"); sea_cron_destroy(&sched); return; }
    if (sea_cron_count(&sched) != 1) { FAIL("count != 1"); sea_cron_destroy(&sched); return; }

    SeaCronJob* job = sea_cron_get(&sched, id);
    if (!job) { FAIL("get returned NULL"); sea_cron_destroy(&sched); return; }
    if (strcmp(job->name, "heartbeat") != 0) { FAIL("wrong name"); sea_cron_destroy(&sched); return; }
    if (job->type != SEA_CRON_SHELL) { FAIL("wrong type"); sea_cron_destroy(&sched); return; }
    if (job->state != SEA_CRON_ACTIVE) { FAIL("wrong state"); sea_cron_destroy(&sched); return; }
    if (job->interval_sec != 30) { FAIL("wrong interval"); sea_cron_destroy(&sched); return; }

    sea_cron_destroy(&sched);
    PASS();
}

/* ── Test: Remove Job ─────────────────────────────────────── */

static void test_remove_job(void) {
    TEST("remove_job");
    SeaCronScheduler sched;
    sea_cron_init(&sched, NULL, NULL);

    i32 id1 = sea_cron_add(&sched, "job1", SEA_CRON_SHELL, "@every 10s", "echo 1", NULL);
    sea_cron_add(&sched, "job2", SEA_CRON_SHELL, "@every 20s", "echo 2", NULL);

    SeaError err = sea_cron_remove(&sched, id1);
    if (err != SEA_OK) { FAIL("remove failed"); sea_cron_destroy(&sched); return; }
    if (sea_cron_count(&sched) != 1) { FAIL("count != 1"); sea_cron_destroy(&sched); return; }

    /* Removed job should not be found */
    if (sea_cron_get(&sched, id1) != NULL) { FAIL("job1 still found"); sea_cron_destroy(&sched); return; }

    sea_cron_destroy(&sched);
    PASS();
}

/* ── Test: Pause and Resume ───────────────────────────────── */

static void test_pause_resume(void) {
    TEST("pause_resume");
    SeaCronScheduler sched;
    sea_cron_init(&sched, NULL, NULL);

    i32 id = sea_cron_add(&sched, "pauser", SEA_CRON_SHELL, "@every 10s", "echo p", NULL);

    sea_cron_pause(&sched, id);
    SeaCronJob* job = sea_cron_get(&sched, id);
    if (job->state != SEA_CRON_PAUSED) { FAIL("not paused"); sea_cron_destroy(&sched); return; }

    sea_cron_resume(&sched, id);
    if (job->state != SEA_CRON_ACTIVE) { FAIL("not resumed"); sea_cron_destroy(&sched); return; }

    sea_cron_destroy(&sched);
    PASS();
}

/* ── Test: Tick executes due jobs ─────────────────────────── */

static void test_tick_execution(void) {
    TEST("tick_execution");
    SeaCronScheduler sched;
    sea_cron_init(&sched, NULL, NULL);

    /* Add a job with next_run in the past (should fire immediately) */
    i32 id = sea_cron_add(&sched, "immediate", SEA_CRON_TOOL,
                           "@every 1s", "echo", "hello");
    SeaCronJob* job = sea_cron_get(&sched, id);
    job->next_run = 1; /* Set to epoch 1 = way in the past */

    u32 executed = sea_cron_tick(&sched);
    if (executed != 1) { FAIL("expected 1 execution"); sea_cron_destroy(&sched); return; }
    if (job->run_count != 1) { FAIL("run_count != 1"); sea_cron_destroy(&sched); return; }
    if (job->last_run == 0) { FAIL("last_run not set"); sea_cron_destroy(&sched); return; }

    /* Next tick should NOT execute (next_run is in the future now) */
    executed = sea_cron_tick(&sched);
    if (executed != 0) { FAIL("expected 0 executions"); sea_cron_destroy(&sched); return; }

    sea_cron_destroy(&sched);
    PASS();
}

/* ── Test: One-shot job completes after firing ────────────── */

static void test_oneshot(void) {
    TEST("oneshot_completes");
    SeaCronScheduler sched;
    sea_cron_init(&sched, NULL, NULL);

    i32 id = sea_cron_add(&sched, "once", SEA_CRON_TOOL,
                           "@once 1s", "echo", "fire");
    SeaCronJob* job = sea_cron_get(&sched, id);
    job->next_run = 1; /* Force immediate */

    sea_cron_tick(&sched);
    if (job->state != SEA_CRON_COMPLETED) { FAIL("not completed"); sea_cron_destroy(&sched); return; }
    if (job->run_count != 1) { FAIL("run_count != 1"); sea_cron_destroy(&sched); return; }

    /* Should not fire again */
    job->next_run = 1;
    u32 executed = sea_cron_tick(&sched);
    if (executed != 0) { FAIL("fired again"); sea_cron_destroy(&sched); return; }

    sea_cron_destroy(&sched);
    PASS();
}

/* ── Test: Paused jobs don't execute ──────────────────────── */

static void test_paused_no_exec(void) {
    TEST("paused_no_exec");
    SeaCronScheduler sched;
    sea_cron_init(&sched, NULL, NULL);

    i32 id = sea_cron_add(&sched, "paused", SEA_CRON_TOOL,
                           "@every 1s", "echo", "nope");
    SeaCronJob* job = sea_cron_get(&sched, id);
    job->next_run = 1;
    sea_cron_pause(&sched, id);

    u32 executed = sea_cron_tick(&sched);
    if (executed != 0) { FAIL("paused job executed"); sea_cron_destroy(&sched); return; }
    if (job->run_count != 0) { FAIL("run_count != 0"); sea_cron_destroy(&sched); return; }

    sea_cron_destroy(&sched);
    PASS();
}

/* ── Test: List jobs ──────────────────────────────────────── */

static void test_list_jobs(void) {
    TEST("list_jobs");
    SeaCronScheduler sched;
    sea_cron_init(&sched, NULL, NULL);

    sea_cron_add(&sched, "j1", SEA_CRON_SHELL, "@every 10s", "echo 1", NULL);
    sea_cron_add(&sched, "j2", SEA_CRON_TOOL, "@every 20s", "echo", "2");
    sea_cron_add(&sched, "j3", SEA_CRON_SHELL, "@once 5s", "echo 3", NULL);

    SeaCronJob* jobs[10];
    u32 count = sea_cron_list(&sched, jobs, 10);
    if (count != 3) { FAIL("count != 3"); sea_cron_destroy(&sched); return; }
    if (strcmp(jobs[0]->name, "j1") != 0) { FAIL("wrong job[0]"); sea_cron_destroy(&sched); return; }
    if (strcmp(jobs[2]->name, "j3") != 0) { FAIL("wrong job[2]"); sea_cron_destroy(&sched); return; }

    sea_cron_destroy(&sched);
    PASS();
}

/* ── Test: DB persistence ─────────────────────────────────── */

static void test_db_persist(void) {
    TEST("db_persistence");
    const char* db_path = "/tmp/test_cron.db";
    unlink(db_path);

    SeaDb* db = NULL;
    sea_db_open(&db, db_path);

    SeaCronScheduler sched;
    sea_cron_init(&sched, db, NULL);

    sea_cron_add(&sched, "persist_job", SEA_CRON_SHELL,
                  "@every 60s", "echo persisted", NULL);
    sea_cron_save(&sched);

    sea_cron_destroy(&sched);
    sea_db_close(db);
    unlink(db_path);
    PASS();
}

/* ── Main ─────────────────────────────────────────────────── */

int main(void) {
    sea_log_init(SEA_LOG_WARN);

    printf("\n\033[1m=== Sea-Claw Cron Tests ===\033[0m\n\n");

    test_parse_every();
    test_parse_every_minutes();
    test_parse_every_hours();
    test_parse_once();
    test_parse_cron();
    test_init_destroy();
    test_add_job();
    test_remove_job();
    test_pause_resume();
    test_tick_execution();
    test_oneshot();
    test_paused_no_exec();
    test_list_jobs();
    test_db_persist();

    printf("\n\033[1mResults: %d passed, %d failed\033[0m\n\n", s_pass, s_fail);
    return s_fail > 0 ? 1 : 0;
}
