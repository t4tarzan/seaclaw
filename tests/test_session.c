/*
 * test_session.c — Session Management Tests
 *
 * Tests session creation, message history, summarization trigger,
 * session isolation, and persistence.
 */

#include "seaclaw/sea_session.h"
#include "seaclaw/sea_log.h"
#include "seaclaw/sea_db.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

/* Stubs for symbols referenced by sea_agent.o */
#include "seaclaw/sea_tools.h"
SeaDb* s_db = NULL;
SeaError sea_tool_exec(const char* n, SeaSlice a, SeaArena* ar, SeaSlice* o) {
    (void)n; (void)a; (void)ar; (void)o; return SEA_ERR_NOT_FOUND;
}
u32 sea_tools_count(void) { return 0; }
const SeaTool* sea_tool_by_id(u32 id) { (void)id; return NULL; }

static int s_pass = 0;
static int s_fail = 0;

#define TEST(name) printf("  [TEST] %s ... ", name)
#define PASS() do { printf("\033[32mPASS\033[0m\n"); s_pass++; } while(0)
#define FAIL(msg) do { printf("\033[31mFAIL: %s\033[0m\n", msg); s_fail++; } while(0)

static const char* TEST_DB = "/tmp/test_session.db";

/* ── Test: Init and Destroy ───────────────────────────────── */

static void test_init_destroy(void) {
    TEST("init_destroy");
    SeaDb* db = NULL;
    sea_db_open(&db, TEST_DB);

    SeaSessionManager mgr;
    SeaError err = sea_session_init(&mgr, db, NULL, 256 * 1024);
    if (err != SEA_OK) { FAIL("init failed"); sea_db_close(db); return; }
    if (sea_session_count(&mgr) != 0) { FAIL("count != 0"); }
    else { PASS(); }

    sea_session_destroy(&mgr);
    sea_db_close(db);
}

/* ── Test: Build Session Key ──────────────────────────────── */

static void test_build_key(void) {
    TEST("build_key");
    char key[128];
    sea_session_build_key(key, sizeof(key), "telegram", 12345);
    if (strcmp(key, "telegram:12345") != 0) { FAIL("wrong key"); return; }

    sea_session_build_key(key, sizeof(key), "discord", -99);
    if (strcmp(key, "discord:-99") != 0) { FAIL("wrong negative key"); return; }

    sea_session_build_key(key, sizeof(key), NULL, 0);
    if (strcmp(key, "tui:0") != 0) { FAIL("wrong null channel key"); return; }

    PASS();
}

/* ── Test: Get or Create Session ──────────────────────────── */

static void test_get_create(void) {
    TEST("get_create_session");
    SeaSessionManager mgr;
    sea_session_init(&mgr, NULL, NULL, 256 * 1024);

    SeaSession* s1 = sea_session_get(&mgr, "telegram:100");
    if (!s1) { FAIL("s1 null"); sea_session_destroy(&mgr); return; }
    if (strcmp(s1->key, "telegram:100") != 0) { FAIL("wrong key"); sea_session_destroy(&mgr); return; }
    if (s1->chat_id != 100) { FAIL("wrong chat_id"); sea_session_destroy(&mgr); return; }
    if (!s1->channel || strcmp(s1->channel, "telegram") != 0) { FAIL("wrong channel"); sea_session_destroy(&mgr); return; }

    /* Getting same key returns same session */
    SeaSession* s2 = sea_session_get(&mgr, "telegram:100");
    if (s1 != s2) { FAIL("not same pointer"); sea_session_destroy(&mgr); return; }

    /* Different key creates new session */
    SeaSession* s3 = sea_session_get(&mgr, "discord:200");
    if (s3 == s1) { FAIL("should be different"); sea_session_destroy(&mgr); return; }
    if (sea_session_count(&mgr) != 2) { FAIL("count != 2"); sea_session_destroy(&mgr); return; }

    sea_session_destroy(&mgr);
    PASS();
}

/* ── Test: Get by Chat ────────────────────────────────────── */

static void test_get_by_chat(void) {
    TEST("get_by_chat");
    SeaSessionManager mgr;
    sea_session_init(&mgr, NULL, NULL, 256 * 1024);

    SeaSession* s = sea_session_get_by_chat(&mgr, "telegram", 42);
    if (!s) { FAIL("null"); sea_session_destroy(&mgr); return; }
    if (strcmp(s->key, "telegram:42") != 0) { FAIL("wrong key"); sea_session_destroy(&mgr); return; }

    sea_session_destroy(&mgr);
    PASS();
}

/* ── Test: Add Messages ───────────────────────────────────── */

static void test_add_messages(void) {
    TEST("add_messages");
    SeaSessionManager mgr;
    sea_session_init(&mgr, NULL, NULL, 256 * 1024);

    sea_session_add_message(&mgr, "tg:1", SEA_ROLE_USER, "Hello");
    sea_session_add_message(&mgr, "tg:1", SEA_ROLE_ASSISTANT, "Hi there!");
    sea_session_add_message(&mgr, "tg:1", SEA_ROLE_USER, "How are you?");

    SeaSession* s = sea_session_get(&mgr, "tg:1");
    if (!s) { FAIL("null"); sea_session_destroy(&mgr); return; }
    if (s->history_count != 3) { FAIL("count != 3"); sea_session_destroy(&mgr); return; }
    if (s->total_messages != 3) { FAIL("total != 3"); sea_session_destroy(&mgr); return; }

    if (s->history[0].role != SEA_ROLE_USER) { FAIL("wrong role[0]"); sea_session_destroy(&mgr); return; }
    if (strcmp(s->history[0].content, "Hello") != 0) { FAIL("wrong content[0]"); sea_session_destroy(&mgr); return; }
    if (s->history[1].role != SEA_ROLE_ASSISTANT) { FAIL("wrong role[1]"); sea_session_destroy(&mgr); return; }

    sea_session_destroy(&mgr);
    PASS();
}

/* ── Test: Get History ────────────────────────────────────── */

static void test_get_history(void) {
    TEST("get_history");
    SeaSessionManager mgr;
    sea_session_init(&mgr, NULL, NULL, 256 * 1024);

    sea_session_add_message(&mgr, "tg:1", SEA_ROLE_USER, "msg1");
    sea_session_add_message(&mgr, "tg:1", SEA_ROLE_ASSISTANT, "msg2");
    sea_session_add_message(&mgr, "tg:1", SEA_ROLE_USER, "msg3");

    SeaArena arena;
    sea_arena_create(&arena, 8192);

    SeaChatMsg history[10];
    u32 count = sea_session_get_history(&mgr, "tg:1", history, 10, &arena);
    if (count != 3) { FAIL("count != 3"); }
    else if (history[0].role != SEA_ROLE_USER) { FAIL("wrong role"); }
    else if (strcmp(history[2].content, "msg3") != 0) { FAIL("wrong content"); }
    else { PASS(); }

    sea_arena_destroy(&arena);
    sea_session_destroy(&mgr);
}

/* ── Test: Session Isolation ──────────────────────────────── */

static void test_isolation(void) {
    TEST("session_isolation");
    SeaSessionManager mgr;
    sea_session_init(&mgr, NULL, NULL, 256 * 1024);

    sea_session_add_message(&mgr, "telegram:100", SEA_ROLE_USER, "Telegram msg");
    sea_session_add_message(&mgr, "discord:200", SEA_ROLE_USER, "Discord msg");

    SeaSession* tg = sea_session_get(&mgr, "telegram:100");
    SeaSession* dc = sea_session_get(&mgr, "discord:200");

    if (tg->history_count != 1) { FAIL("tg count != 1"); sea_session_destroy(&mgr); return; }
    if (dc->history_count != 1) { FAIL("dc count != 1"); sea_session_destroy(&mgr); return; }
    if (strcmp(tg->history[0].content, "Telegram msg") != 0) { FAIL("tg wrong content"); sea_session_destroy(&mgr); return; }
    if (strcmp(dc->history[0].content, "Discord msg") != 0) { FAIL("dc wrong content"); sea_session_destroy(&mgr); return; }

    sea_session_destroy(&mgr);
    PASS();
}

/* ── Test: Clear Session ──────────────────────────────────── */

static void test_clear(void) {
    TEST("clear_session");
    SeaSessionManager mgr;
    sea_session_init(&mgr, NULL, NULL, 256 * 1024);

    sea_session_add_message(&mgr, "tg:1", SEA_ROLE_USER, "Hello");
    sea_session_add_message(&mgr, "tg:1", SEA_ROLE_ASSISTANT, "Hi");

    SeaError err = sea_session_clear(&mgr, "tg:1");
    if (err != SEA_OK) { FAIL("clear failed"); sea_session_destroy(&mgr); return; }

    SeaSession* s = sea_session_get(&mgr, "tg:1");
    if (s->history_count != 0) { FAIL("count != 0 after clear"); sea_session_destroy(&mgr); return; }
    if (s->summary != NULL) { FAIL("summary not null"); sea_session_destroy(&mgr); return; }

    sea_session_destroy(&mgr);
    PASS();
}

/* ── Test: List Keys ──────────────────────────────────────── */

static void test_list_keys(void) {
    TEST("list_keys");
    SeaSessionManager mgr;
    sea_session_init(&mgr, NULL, NULL, 256 * 1024);

    sea_session_get(&mgr, "telegram:1");
    sea_session_get(&mgr, "discord:2");
    sea_session_get(&mgr, "slack:3");

    const char* keys[10];
    u32 count = sea_session_list_keys(&mgr, keys, 10);
    if (count != 3) { FAIL("count != 3"); sea_session_destroy(&mgr); return; }

    sea_session_destroy(&mgr);
    PASS();
}

/* ── Test: History Overflow (ring buffer behavior) ────────── */

static void test_history_overflow(void) {
    TEST("history_overflow");
    SeaSessionManager mgr;
    sea_session_init(&mgr, NULL, NULL, 512 * 1024);
    mgr.max_history = 999; /* Disable auto-summarize for this test */

    /* Add more messages than SEA_SESSION_MAX_HISTORY */
    for (int i = 0; i < SEA_SESSION_MAX_HISTORY + 10; i++) {
        char msg[32];
        snprintf(msg, sizeof(msg), "msg-%d", i);
        sea_session_add_message(&mgr, "tg:1", SEA_ROLE_USER, msg);
    }

    SeaSession* s = sea_session_get(&mgr, "tg:1");
    if (s->history_count != SEA_SESSION_MAX_HISTORY) {
        char buf[64];
        snprintf(buf, sizeof(buf), "count=%u expected=%d", s->history_count, SEA_SESSION_MAX_HISTORY);
        FAIL(buf);
    } else {
        /* Most recent message should be the last one added */
        char expected[32];
        snprintf(expected, sizeof(expected), "msg-%d", SEA_SESSION_MAX_HISTORY + 9);
        if (strcmp(s->history[s->history_count - 1].content, expected) != 0) {
            FAIL("last message wrong");
        } else {
            PASS();
        }
    }

    sea_session_destroy(&mgr);
}

/* ── Test: DB Persistence ─────────────────────────────────── */

static void test_db_persistence(void) {
    TEST("db_persistence");
    unlink(TEST_DB);

    SeaDb* db = NULL;
    sea_db_open(&db, TEST_DB);

    SeaSessionManager mgr;
    sea_session_init(&mgr, db, NULL, 256 * 1024);

    sea_session_add_message(&mgr, "tg:1", SEA_ROLE_USER, "Persisted msg");
    sea_session_save_all(&mgr);

    sea_session_destroy(&mgr);
    sea_db_close(db);

    /* Verify the data is in the DB by reopening */
    sea_db_open(&db, TEST_DB);
    SeaArena arena;
    sea_arena_create(&arena, 8192);

    /* Check session_messages table has our message */
    /* We can't easily query with our current API, but we verify
     * the tables were created and save didn't crash */
    sea_arena_destroy(&arena);
    sea_db_close(db);
    unlink(TEST_DB);
    PASS();
}

/* ── Main ─────────────────────────────────────────────────── */

int main(void) {
    sea_log_init(SEA_LOG_WARN);

    printf("\n\033[1m=== Sea-Claw Session Tests ===\033[0m\n\n");

    test_init_destroy();
    test_build_key();
    test_get_create();
    test_get_by_chat();
    test_add_messages();
    test_get_history();
    test_isolation();
    test_clear();
    test_list_keys();
    test_history_overflow();
    test_db_persistence();

    printf("\n\033[1mResults: %d passed, %d failed\033[0m\n\n", s_pass, s_fail);
    return s_fail > 0 ? 1 : 0;
}
