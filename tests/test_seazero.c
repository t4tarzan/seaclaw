/*
 * test_seazero.c — SeaZero v3 database + bridge tests
 *
 * Tests the v3 schema tables (agents, tasks, llm_usage, audit)
 * and the bridge config/tool registration.
 */

#include "seaclaw/sea_types.h"
#include "seaclaw/sea_db.h"
#include "seaclaw/sea_arena.h"
#include "seaclaw/sea_log.h"
#include "sea_zero.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static u32 s_pass = 0;
static u32 s_fail = 0;

#define TEST(name) \
    do { printf("  %-48s ", name); } while(0)

#define PASS() \
    do { printf("\033[32mPASS\033[0m\n"); s_pass++; } while(0)

#define FAIL(msg) \
    do { printf("\033[31mFAIL\033[0m (%s)\n", msg); s_fail++; } while(0)

#define TEST_DB_PATH "/tmp/seazero_test.db"

/* ── Agent Management Tests ──────────────────────────────── */

static void test_agent_register(void) {
    TEST("agent register");
    SeaDb* db = NULL;
    sea_db_open(&db, TEST_DB_PATH);

    SeaError err = sea_db_sz_agent_register(db, "agent-0",
        "seazero-agent-0", 8080, "openrouter", "kimi-k2.5");
    if (err != SEA_OK) { FAIL("register failed"); sea_db_close(db); return; }
    PASS();
    sea_db_close(db);
}

static void test_agent_register_upsert(void) {
    TEST("agent register upsert (same id, new port)");
    SeaDb* db = NULL;
    sea_db_open(&db, TEST_DB_PATH);

    SeaArena arena;
    sea_arena_create(&arena, 8192);

    /* Register same agent with different port */
    sea_db_sz_agent_register(db, "agent-0",
        "seazero-agent-0", 8081, "zai", "glm-5");

    SeaDbAgent agents[8];
    i32 count = sea_db_sz_agent_list(db, agents, 8, &arena);

    /* Should still be 1 agent (upserted, not duplicated) */
    i32 agent0_count = 0;
    for (i32 i = 0; i < count; i++) {
        if (strcmp(agents[i].agent_id, "agent-0") == 0) {
            agent0_count++;
            if (agents[i].port != 8081) { FAIL("port not updated"); goto cleanup; }
            if (strcmp(agents[i].provider, "zai") != 0) { FAIL("provider not updated"); goto cleanup; }
        }
    }
    if (agent0_count != 1) { FAIL("expected exactly 1 agent-0"); goto cleanup; }
    PASS();

cleanup:
    sea_arena_destroy(&arena);
    sea_db_close(db);
}

static void test_agent_update_status(void) {
    TEST("agent update status");
    SeaDb* db = NULL;
    sea_db_open(&db, TEST_DB_PATH);

    SeaArena arena;
    sea_arena_create(&arena, 8192);

    sea_db_sz_agent_update_status(db, "agent-0", "busy");

    SeaDbAgent agents[8];
    i32 count = sea_db_sz_agent_list(db, agents, 8, &arena);

    bool found = false;
    for (i32 i = 0; i < count; i++) {
        if (strcmp(agents[i].agent_id, "agent-0") == 0) {
            found = true;
            if (strcmp(agents[i].status, "busy") != 0) {
                FAIL("status not updated"); goto cleanup;
            }
        }
    }
    if (!found) { FAIL("agent not found"); goto cleanup; }
    PASS();

cleanup:
    sea_arena_destroy(&arena);
    sea_db_close(db);
}

static void test_agent_heartbeat(void) {
    TEST("agent heartbeat updates last_seen");
    SeaDb* db = NULL;
    sea_db_open(&db, TEST_DB_PATH);

    SeaError err = sea_db_sz_agent_heartbeat(db, "agent-0");
    if (err != SEA_OK) { FAIL("heartbeat failed"); sea_db_close(db); return; }
    PASS();
    sea_db_close(db);
}

static void test_agent_list(void) {
    TEST("agent list");
    SeaDb* db = NULL;
    sea_db_open(&db, TEST_DB_PATH);

    SeaArena arena;
    sea_arena_create(&arena, 8192);

    /* Register a second agent */
    sea_db_sz_agent_register(db, "agent-abc123",
        "seazero-agent-abc123", 8082, "openai", "gpt-4o-mini");

    SeaDbAgent agents[8];
    i32 count = sea_db_sz_agent_list(db, agents, 8, &arena);

    if (count < 2) { FAIL("expected at least 2 agents"); goto cleanup; }
    PASS();

cleanup:
    sea_arena_destroy(&arena);
    sea_db_close(db);
}

/* ── Task Tracking Tests ─────────────────────────────────── */

static void test_task_create(void) {
    TEST("task create");
    SeaDb* db = NULL;
    sea_db_open(&db, TEST_DB_PATH);

    SeaError err = sea_db_sz_task_create(db, "task-001", "agent-0",
        12345, "Scan network for open ports", "User asked for security audit");
    if (err != SEA_OK) { FAIL("create failed"); sea_db_close(db); return; }
    PASS();
    sea_db_close(db);
}

static void test_task_start(void) {
    TEST("task start");
    SeaDb* db = NULL;
    sea_db_open(&db, TEST_DB_PATH);

    SeaArena arena;
    sea_arena_create(&arena, 8192);

    SeaError err = sea_db_sz_task_start(db, "task-001");
    if (err != SEA_OK) { FAIL("start failed"); goto cleanup; }

    SeaDbSzTask tasks[8];
    i32 count = sea_db_sz_task_list(db, "running", tasks, 8, &arena);
    if (count < 1) { FAIL("no running tasks"); goto cleanup; }

    bool found = false;
    for (i32 i = 0; i < count; i++) {
        if (strcmp(tasks[i].task_id, "task-001") == 0) {
            found = true;
            if (strcmp(tasks[i].status, "running") != 0) {
                FAIL("status not running"); goto cleanup;
            }
        }
    }
    if (!found) { FAIL("task not found"); goto cleanup; }
    PASS();

cleanup:
    sea_arena_destroy(&arena);
    sea_db_close(db);
}

static void test_task_complete(void) {
    TEST("task complete with result");
    SeaDb* db = NULL;
    sea_db_open(&db, TEST_DB_PATH);

    SeaArena arena;
    sea_arena_create(&arena, 8192);

    SeaError err = sea_db_sz_task_complete(db, "task-001",
        "Found 12 hosts, 3 with open ports", "[\"report.txt\"]", 8, 42.5);
    if (err != SEA_OK) { FAIL("complete failed"); goto cleanup; }

    SeaDbSzTask tasks[8];
    i32 count = sea_db_sz_task_list(db, "completed", tasks, 8, &arena);
    if (count < 1) { FAIL("no completed tasks"); goto cleanup; }

    bool found = false;
    for (i32 i = 0; i < count; i++) {
        if (strcmp(tasks[i].task_id, "task-001") == 0) {
            found = true;
            if (strcmp(tasks[i].status, "completed") != 0) {
                FAIL("status not completed"); goto cleanup;
            }
            if (tasks[i].steps_taken != 8) {
                FAIL("steps_taken mismatch"); goto cleanup;
            }
            if (tasks[i].elapsed_sec < 42.0 || tasks[i].elapsed_sec > 43.0) {
                FAIL("elapsed_sec mismatch"); goto cleanup;
            }
        }
    }
    if (!found) { FAIL("task not found"); goto cleanup; }
    PASS();

cleanup:
    sea_arena_destroy(&arena);
    sea_db_close(db);
}

static void test_task_fail(void) {
    TEST("task fail with error");
    SeaDb* db = NULL;
    sea_db_open(&db, TEST_DB_PATH);

    SeaArena arena;
    sea_arena_create(&arena, 8192);

    /* Create and fail a new task */
    sea_db_sz_task_create(db, "task-002", "agent-0", 12345,
        "Write a Python script", NULL);
    sea_db_sz_task_start(db, "task-002");

    SeaError err = sea_db_sz_task_fail(db, "task-002",
        "Agent Zero timed out", 120.0);
    if (err != SEA_OK) { FAIL("fail failed"); goto cleanup; }

    SeaDbSzTask tasks[8];
    i32 count = sea_db_sz_task_list(db, "failed", tasks, 8, &arena);
    if (count < 1) { FAIL("no failed tasks"); goto cleanup; }

    bool found = false;
    for (i32 i = 0; i < count; i++) {
        if (strcmp(tasks[i].task_id, "task-002") == 0) {
            found = true;
            if (!tasks[i].error || strcmp(tasks[i].error, "Agent Zero timed out") != 0) {
                FAIL("error message mismatch"); goto cleanup;
            }
        }
    }
    if (!found) { FAIL("task not found"); goto cleanup; }
    PASS();

cleanup:
    sea_arena_destroy(&arena);
    sea_db_close(db);
}

static void test_task_list_all(void) {
    TEST("task list all (no filter)");
    SeaDb* db = NULL;
    sea_db_open(&db, TEST_DB_PATH);

    SeaArena arena;
    sea_arena_create(&arena, 16384);

    SeaDbSzTask tasks[16];
    i32 count = sea_db_sz_task_list(db, NULL, tasks, 16, &arena);
    if (count < 2) { FAIL("expected at least 2 tasks"); goto cleanup; }
    PASS();

cleanup:
    sea_arena_destroy(&arena);
    sea_db_close(db);
}

/* ── LLM Usage Tests ─────────────────────────────────────── */

static void test_llm_log(void) {
    TEST("llm usage log");
    SeaDb* db = NULL;
    sea_db_open(&db, TEST_DB_PATH);

    SeaError err = sea_db_sz_llm_log(db, "seaclaw",
        "openrouter", "kimi-k2.5", 1500, 800, 0.002, 3200, "ok", NULL);
    if (err != SEA_OK) { FAIL("log failed"); sea_db_close(db); return; }

    err = sea_db_sz_llm_log(db, "agent-0",
        "openrouter", "kimi-k2.5", 2000, 1200, 0.003, 4500, "ok", "task-001");
    if (err != SEA_OK) { FAIL("agent log failed"); sea_db_close(db); return; }

    PASS();
    sea_db_close(db);
}

static void test_llm_total_tokens(void) {
    TEST("llm total tokens per caller");
    SeaDb* db = NULL;
    sea_db_open(&db, TEST_DB_PATH);

    i64 seaclaw_tokens = sea_db_sz_llm_total_tokens(db, "seaclaw");
    i64 agent_tokens   = sea_db_sz_llm_total_tokens(db, "agent-0");

    /* seaclaw: 1500 + 800 = 2300 */
    if (seaclaw_tokens != 2300) { FAIL("seaclaw tokens mismatch"); sea_db_close(db); return; }

    /* agent-0: 2000 + 1200 = 3200 */
    if (agent_tokens != 3200) { FAIL("agent tokens mismatch"); sea_db_close(db); return; }

    PASS();
    sea_db_close(db);
}

/* ── Audit Tests ─────────────────────────────────────────── */

static void test_audit_log(void) {
    TEST("audit log events");
    SeaDb* db = NULL;
    sea_db_open(&db, TEST_DB_PATH);

    SeaError e1 = sea_db_sz_audit(db, "delegation", "seaclaw", "agent-0",
        "{\"task\":\"scan network\"}", "info");
    SeaError e2 = sea_db_sz_audit(db, "shield_block", "agent-0", NULL,
        "Output contained injection attempt", "warn");
    SeaError e3 = sea_db_sz_audit(db, "agent_spawn", "seaclaw", "agent-abc123",
        "{\"port\":8082}", "info");

    if (e1 != SEA_OK || e2 != SEA_OK || e3 != SEA_OK) {
        FAIL("audit log failed"); sea_db_close(db); return;
    }
    PASS();
    sea_db_close(db);
}

static void test_audit_null_target(void) {
    TEST("audit log with NULL target");
    SeaDb* db = NULL;
    sea_db_open(&db, TEST_DB_PATH);

    SeaError err = sea_db_sz_audit(db, "startup", "seaclaw", NULL, NULL, "info");
    if (err != SEA_OK) { FAIL("null target failed"); sea_db_close(db); return; }
    PASS();
    sea_db_close(db);
}

/* ── Bridge Config Tests ─────────────────────────────────── */

static void test_bridge_init(void) {
    TEST("bridge init with defaults");
    SeaZeroConfig cfg = {0};
    bool ok = sea_zero_init(&cfg, NULL);
    if (!ok) { FAIL("init returned false"); return; }
    if (!cfg.enabled) { FAIL("not enabled"); return; }
    if (strcmp(cfg.agent_url, "http://localhost:8080") != 0) {
        FAIL("wrong default URL"); return;
    }
    if (cfg.timeout_sec != 120) { FAIL("wrong default timeout"); return; }
    PASS();
}

static void test_bridge_init_custom(void) {
    TEST("bridge init with custom URL");
    SeaZeroConfig cfg = {0};
    sea_zero_init(&cfg, "http://10.0.0.5:9090");
    if (strcmp(cfg.agent_url, "http://10.0.0.5:9090") != 0) {
        FAIL("custom URL not set"); return;
    }
    PASS();
}

static void test_bridge_delegate_disabled(void) {
    TEST("bridge delegate when disabled returns error");
    SeaZeroConfig cfg = {0};
    cfg.enabled = false;

    SeaArena arena;
    sea_arena_create(&arena, 4096);

    SeaZeroTask task = { .task = "test", .max_steps = 5 };
    SeaZeroResult res = sea_zero_delegate(&cfg, &task, &arena);

    if (res.success) { FAIL("should not succeed when disabled"); }
    else if (!res.error) { FAIL("expected error message"); }
    else { PASS(); }

    sea_arena_destroy(&arena);
}

static void test_bridge_delegate_empty_task(void) {
    TEST("bridge delegate with empty task returns error");
    SeaZeroConfig cfg = {0};
    sea_zero_init(&cfg, NULL);

    SeaArena arena;
    sea_arena_create(&arena, 4096);

    SeaZeroTask task = { .task = "", .max_steps = 5 };
    SeaZeroResult res = sea_zero_delegate(&cfg, &task, &arena);

    if (res.success) { FAIL("should not succeed with empty task"); }
    else { PASS(); }

    sea_arena_destroy(&arena);
}


/* ── Main ────────────────────────────────────────────────── */

int main(void) {
    sea_log_init(SEA_LOG_WARN);

    /* Clean slate */
    unlink(TEST_DB_PATH);

    printf("\n  \033[1mSeaZero v3 Tests\033[0m\n");
    printf("  ════════════════════════════════════════════════════\n\n");

    /* Agent management */
    test_agent_register();
    test_agent_register_upsert();
    test_agent_update_status();
    test_agent_heartbeat();
    test_agent_list();

    /* Task tracking */
    test_task_create();
    test_task_start();
    test_task_complete();
    test_task_fail();
    test_task_list_all();

    /* LLM usage */
    test_llm_log();
    test_llm_total_tokens();

    /* Audit */
    test_audit_log();
    test_audit_null_target();

    /* Bridge config */
    test_bridge_init();
    test_bridge_init_custom();
    test_bridge_delegate_disabled();
    test_bridge_delegate_empty_task();


    printf("\n  ────────────────────────────────────────────────────\n");
    printf("  Results: \033[32m%u passed\033[0m", s_pass);
    if (s_fail > 0) printf(", \033[31m%u failed\033[0m", s_fail);
    printf("\n\n");

    /* Cleanup */
    unlink(TEST_DB_PATH);

    return s_fail > 0 ? 1 : 0;
}
