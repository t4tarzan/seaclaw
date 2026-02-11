/*
 * test_recall.c — Tests for SQLite-backed memory index
 */

#include "seaclaw/sea_recall.h"
#include "seaclaw/sea_db.h"
#include "seaclaw/sea_arena.h"
#include "seaclaw/sea_log.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define TEST_DB "/tmp/test_recall.db"

static int s_pass = 0;
static int s_fail = 0;

#define TEST(name) printf("  [TEST] %s ... ", name)
#define PASS() do { printf("\033[32mPASS\033[0m\n"); s_pass++; } while(0)
#define FAIL(msg) do { printf("\033[31mFAIL: %s\033[0m\n", msg); s_fail++; } while(0)

/* ── Test: Init creates table ────────────────────────────── */

static void test_init(void) {
    TEST("recall_init");
    unlink(TEST_DB);

    SeaDb* db = NULL;
    if (sea_db_open(&db, TEST_DB) != SEA_OK) { FAIL("db open"); return; }

    SeaRecall rc;
    SeaError err = sea_recall_init(&rc, db, 800);
    if (err != SEA_OK) { FAIL("init failed"); sea_db_close(db); return; }

    if (!rc.initialized) { FAIL("not initialized"); sea_recall_destroy(&rc); sea_db_close(db); return; }
    if (sea_recall_count(&rc) != 0) { FAIL("count not 0"); sea_recall_destroy(&rc); sea_db_close(db); return; }

    sea_recall_destroy(&rc);
    sea_db_close(db);
    PASS();
}

/* ── Test: Store and count ───────────────────────────────── */

static void test_store(void) {
    TEST("recall_store");
    unlink(TEST_DB);

    SeaDb* db = NULL;
    sea_db_open(&db, TEST_DB);
    SeaRecall rc;
    sea_recall_init(&rc, db, 800);

    sea_recall_store(&rc, "user", "The user's name is Alice", NULL, 9);
    sea_recall_store(&rc, "preference", "User prefers dark mode", NULL, 7);
    sea_recall_store(&rc, "fact", "Project uses C11 with arena allocation", NULL, 5);

    if (sea_recall_count(&rc) != 3) { FAIL("count != 3"); sea_recall_destroy(&rc); sea_db_close(db); return; }
    if (sea_recall_count_category(&rc, "user") != 1) { FAIL("user count != 1"); sea_recall_destroy(&rc); sea_db_close(db); return; }

    sea_recall_destroy(&rc);
    sea_db_close(db);
    PASS();
}

/* ── Test: Duplicate detection ───────────────────────────── */

static void test_dedup(void) {
    TEST("recall_dedup");
    unlink(TEST_DB);

    SeaDb* db = NULL;
    sea_db_open(&db, TEST_DB);
    SeaRecall rc;
    sea_recall_init(&rc, db, 800);

    sea_recall_store(&rc, "user", "The user's name is Alice", NULL, 9);
    sea_recall_store(&rc, "user", "The user's name is Alice", NULL, 9); /* dup */

    if (sea_recall_count(&rc) != 1) { FAIL("dup not detected"); sea_recall_destroy(&rc); sea_db_close(db); return; }

    sea_recall_destroy(&rc);
    sea_db_close(db);
    PASS();
}

/* ── Test: Query with keyword scoring ────────────────────── */

static void test_query(void) {
    TEST("recall_query");
    unlink(TEST_DB);

    SeaDb* db = NULL;
    sea_db_open(&db, TEST_DB);
    SeaRecall rc;
    sea_recall_init(&rc, db, 800);

    sea_recall_store(&rc, "user", "The user's name is Alice", NULL, 9);
    sea_recall_store(&rc, "preference", "User prefers dark mode", NULL, 7);
    sea_recall_store(&rc, "fact", "Project uses C11 with arena allocation", NULL, 5);
    sea_recall_store(&rc, "fact", "The weather today is sunny", NULL, 3);

    SeaArena arena;
    sea_arena_create(&arena, 64 * 1024);

    SeaRecallFact facts[10];
    i32 count = sea_recall_query(&rc, "what is the user's name", facts, 10, &arena);

    if (count == 0) { FAIL("no results"); sea_arena_destroy(&arena); sea_recall_destroy(&rc); sea_db_close(db); return; }

    /* "Alice" fact should score highest (keyword "user" + "name" + category "user") */
    if (strstr(facts[0].content, "Alice") == NULL) {
        FAIL("Alice not top result");
        sea_arena_destroy(&arena);
        sea_recall_destroy(&rc);
        sea_db_close(db);
        return;
    }

    sea_arena_destroy(&arena);
    sea_recall_destroy(&rc);
    sea_db_close(db);
    PASS();
}

/* ── Test: Build context ─────────────────────────────────── */

static void test_build_context(void) {
    TEST("recall_build_context");
    unlink(TEST_DB);

    SeaDb* db = NULL;
    sea_db_open(&db, TEST_DB);
    SeaRecall rc;
    sea_recall_init(&rc, db, 800);

    sea_recall_store(&rc, "user", "The user's name is Bob", NULL, 9);
    sea_recall_store(&rc, "preference", "Bob likes Python and C", NULL, 7);

    SeaArena arena;
    sea_arena_create(&arena, 64 * 1024);

    const char* ctx = sea_recall_build_context(&rc, "hello Bob", &arena);
    if (!ctx) { FAIL("context null"); sea_arena_destroy(&arena); sea_recall_destroy(&rc); sea_db_close(db); return; }
    if (strstr(ctx, "Bob") == NULL) { FAIL("Bob not in context"); sea_arena_destroy(&arena); sea_recall_destroy(&rc); sea_db_close(db); return; }
    if (strstr(ctx, "Memory") == NULL) { FAIL("header missing"); sea_arena_destroy(&arena); sea_recall_destroy(&rc); sea_db_close(db); return; }

    sea_arena_destroy(&arena);
    sea_recall_destroy(&rc);
    sea_db_close(db);
    PASS();
}

/* ── Test: Forget ────────────────────────────────────────── */

static void test_forget(void) {
    TEST("recall_forget");
    unlink(TEST_DB);

    SeaDb* db = NULL;
    sea_db_open(&db, TEST_DB);
    SeaRecall rc;
    sea_recall_init(&rc, db, 800);

    sea_recall_store(&rc, "fact", "Fact one", NULL, 5);
    sea_recall_store(&rc, "fact", "Fact two", NULL, 5);
    sea_recall_store(&rc, "user", "User fact", NULL, 8);

    if (sea_recall_count(&rc) != 3) { FAIL("count != 3"); sea_recall_destroy(&rc); sea_db_close(db); return; }

    sea_recall_forget(&rc, 1);
    if (sea_recall_count(&rc) != 2) { FAIL("count != 2 after forget"); sea_recall_destroy(&rc); sea_db_close(db); return; }

    sea_recall_forget_category(&rc, "fact");
    if (sea_recall_count(&rc) != 1) { FAIL("count != 1 after forget_category"); sea_recall_destroy(&rc); sea_db_close(db); return; }

    sea_recall_destroy(&rc);
    sea_db_close(db);
    PASS();
}

/* ── Test: Empty query returns high-importance facts ─────── */

static void test_empty_query(void) {
    TEST("recall_empty_query");
    unlink(TEST_DB);

    SeaDb* db = NULL;
    sea_db_open(&db, TEST_DB);
    SeaRecall rc;
    sea_recall_init(&rc, db, 800);

    sea_recall_store(&rc, "user", "User is a developer", NULL, 9);
    sea_recall_store(&rc, "fact", "Low importance fact", NULL, 2);

    SeaArena arena;
    sea_arena_create(&arena, 64 * 1024);

    SeaRecallFact facts[10];
    i32 count = sea_recall_query(&rc, "", facts, 10, &arena);

    /* High-importance user facts should still appear */
    if (count == 0) { FAIL("no results for empty query"); sea_arena_destroy(&arena); sea_recall_destroy(&rc); sea_db_close(db); return; }

    sea_arena_destroy(&arena);
    sea_recall_destroy(&rc);
    sea_db_close(db);
    PASS();
}

/* ── Main ─────────────────────────────────────────────────── */

int main(void) {
    printf("\n  ═══ test_recall ═══\n\n");

    test_init();
    test_store();
    test_dedup();
    test_query();
    test_build_context();
    test_forget();
    test_empty_query();

    printf("\n  Results: %d passed, %d failed\n\n", s_pass, s_fail);

    unlink(TEST_DB);
    return s_fail > 0 ? 1 : 0;
}
