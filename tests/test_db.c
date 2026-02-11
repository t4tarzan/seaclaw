/*
 * test_db.c — SQLite database tests
 */

#include "seaclaw/sea_types.h"
#include "seaclaw/sea_db.h"
#include "seaclaw/sea_arena.h"
#include "seaclaw/sea_log.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static u32 s_pass = 0;
static u32 s_fail = 0;

#define TEST(name) \
    do { printf("  %-44s ", name); } while(0)

#define PASS() \
    do { printf("\033[32mPASS\033[0m\n"); s_pass++; } while(0)

#define FAIL(msg) \
    do { printf("\033[31mFAIL\033[0m (%s)\n", msg); s_fail++; } while(0)

#define TEST_DB_PATH "/tmp/seaclaw_test.db"

static void test_open_close(void) {
    TEST("open and close database");
    SeaDb* db = NULL;
    SeaError err = sea_db_open(&db, TEST_DB_PATH);
    if (err != SEA_OK) { FAIL("open failed"); return; }
    if (!db) { FAIL("null handle"); return; }
    sea_db_close(db);
    PASS();
}

static void test_config_set_get(void) {
    TEST("config set and get");
    SeaDb* db = NULL;
    sea_db_open(&db, TEST_DB_PATH);

    SeaArena arena;
    sea_arena_create(&arena, 4096);

    sea_db_config_set(db, "bot_token", "123456:ABC");
    sea_db_config_set(db, "chat_id", "99887766");

    const char* token = sea_db_config_get(db, "bot_token", &arena);
    const char* chat  = sea_db_config_get(db, "chat_id", &arena);

    if (!token || strcmp(token, "123456:ABC") != 0) { FAIL("token mismatch"); goto cleanup; }
    if (!chat || strcmp(chat, "99887766") != 0) { FAIL("chat mismatch"); goto cleanup; }
    PASS();

cleanup:
    sea_arena_destroy(&arena);
    sea_db_close(db);
}

static void test_config_upsert(void) {
    TEST("config upsert overwrites");
    SeaDb* db = NULL;
    sea_db_open(&db, TEST_DB_PATH);

    SeaArena arena;
    sea_arena_create(&arena, 4096);

    sea_db_config_set(db, "version", "1.0");
    sea_db_config_set(db, "version", "2.0");

    const char* val = sea_db_config_get(db, "version", &arena);
    if (!val || strcmp(val, "2.0") != 0) { FAIL("upsert failed"); goto cleanup; }
    PASS();

cleanup:
    sea_arena_destroy(&arena);
    sea_db_close(db);
}

static void test_config_missing_key(void) {
    TEST("config get missing key returns NULL");
    SeaDb* db = NULL;
    sea_db_open(&db, TEST_DB_PATH);

    SeaArena arena;
    sea_arena_create(&arena, 4096);

    const char* val = sea_db_config_get(db, "nonexistent_key_xyz", &arena);
    if (val != NULL) { FAIL("expected NULL"); goto cleanup; }
    PASS();

cleanup:
    sea_arena_destroy(&arena);
    sea_db_close(db);
}

static void test_trajectory_log(void) {
    TEST("trajectory log event");
    SeaDb* db = NULL;
    sea_db_open(&db, TEST_DB_PATH);

    SeaError err = sea_db_log_event(db, "milestone", "Test milestone", "This is a test entry");
    if (err != SEA_OK) { FAIL("log failed"); sea_db_close(db); return; }
    PASS();
    sea_db_close(db);
}

static void test_task_create_and_list(void) {
    TEST("task create and list");
    SeaDb* db = NULL;
    sea_db_open(&db, TEST_DB_PATH);

    SeaArena arena;
    sea_arena_create(&arena, 16384);

    sea_db_task_create(db, "Build JSON parser", "high", "Zero-copy implementation");
    sea_db_task_create(db, "Write tests", "medium", NULL);

    SeaDbTask tasks[32];
    i32 count = sea_db_task_list(db, NULL, tasks, 32, &arena);

    if (count < 2) { FAIL("expected at least 2 tasks"); goto cleanup; }

    /* Find our tasks */
    bool found_json = false;
    bool found_tests = false;
    for (i32 i = 0; i < count; i++) {
        if (strcmp(tasks[i].title, "Build JSON parser") == 0) found_json = true;
        if (strcmp(tasks[i].title, "Write tests") == 0) found_tests = true;
    }
    if (!found_json || !found_tests) { FAIL("tasks not found"); goto cleanup; }
    PASS();

cleanup:
    sea_arena_destroy(&arena);
    sea_db_close(db);
}

static void test_task_update_status(void) {
    TEST("task update status");
    SeaDb* db = NULL;
    sea_db_open(&db, TEST_DB_PATH);

    SeaArena arena;
    sea_arena_create(&arena, 16384);

    /* List pending tasks, update first one */
    SeaDbTask tasks[32];
    i32 count = sea_db_task_list(db, "pending", tasks, 32, &arena);
    if (count == 0) { FAIL("no pending tasks"); goto cleanup; }

    SeaError err = sea_db_task_update_status(db, tasks[0].id, "completed");
    if (err != SEA_OK) { FAIL("update failed"); goto cleanup; }

    /* Verify it's now completed */
    sea_arena_reset(&arena);
    SeaDbTask completed[32];
    i32 ccount = sea_db_task_list(db, "completed", completed, 32, &arena);
    bool found = false;
    for (i32 i = 0; i < ccount; i++) {
        if (completed[i].id == tasks[0].id) { found = true; break; }
    }
    if (!found) { FAIL("task not in completed list"); goto cleanup; }
    PASS();

cleanup:
    sea_arena_destroy(&arena);
    sea_db_close(db);
}

static void test_chat_log(void) {
    TEST("chat history log");
    SeaDb* db = NULL;
    sea_db_open(&db, TEST_DB_PATH);

    SeaError e1 = sea_db_chat_log(db, 12345, "user", "Hello Sea-Claw");
    SeaError e2 = sea_db_chat_log(db, 12345, "assistant", "Hello! How can I help?");
    if (e1 != SEA_OK || e2 != SEA_OK) { FAIL("chat log failed"); sea_db_close(db); return; }
    PASS();
    sea_db_close(db);
}

static void test_persistence(void) {
    TEST("data persists across open/close");

    /* Write */
    SeaDb* db = NULL;
    sea_db_open(&db, TEST_DB_PATH);
    sea_db_config_set(db, "persist_test", "survived");
    sea_db_close(db);

    /* Read back */
    db = NULL;
    sea_db_open(&db, TEST_DB_PATH);
    SeaArena arena;
    sea_arena_create(&arena, 4096);

    const char* val = sea_db_config_get(db, "persist_test", &arena);
    if (!val || strcmp(val, "survived") != 0) { FAIL("data lost"); }
    else { PASS(); }

    sea_arena_destroy(&arena);
    sea_db_close(db);
}

static void test_raw_exec(void) {
    TEST("raw SQL exec");
    SeaDb* db = NULL;
    sea_db_open(&db, TEST_DB_PATH);

    SeaError err = sea_db_exec(db, "CREATE TABLE IF NOT EXISTS test_raw (x INTEGER)");
    if (err != SEA_OK) { FAIL("exec failed"); sea_db_close(db); return; }

    err = sea_db_exec(db, "INSERT INTO test_raw VALUES (42)");
    if (err != SEA_OK) { FAIL("insert failed"); sea_db_close(db); return; }

    PASS();
    sea_db_close(db);
}

int main(void) {
    sea_log_init(SEA_LOG_WARN);

    /* Clean slate */
    unlink(TEST_DB_PATH);

    printf("\n  \033[1mSea-Claw Database Tests\033[0m\n");
    printf("  ════════════════════════════════════════════════\n\n");

    test_open_close();
    test_config_set_get();
    test_config_upsert();
    test_config_missing_key();
    test_trajectory_log();
    test_task_create_and_list();
    test_task_update_status();
    test_chat_log();
    test_persistence();
    test_raw_exec();

    printf("\n  ────────────────────────────────────────────────\n");
    printf("  Results: \033[32m%u passed\033[0m", s_pass);
    if (s_fail > 0) printf(", \033[31m%u failed\033[0m", s_fail);
    printf("\n\n");

    /* Cleanup */
    unlink(TEST_DB_PATH);

    return s_fail > 0 ? 1 : 0;
}
