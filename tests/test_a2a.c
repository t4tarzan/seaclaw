/*
 * test_a2a.c — Tests for Agent-to-Agent Communication
 *
 * Tests: sea_a2a.h (struct validation, delegate error paths, discover edge cases)
 * Note: Actual HTTP delegation requires a running peer, so we test
 * error paths, null handling, and struct constants.
 */

#include "seaclaw/sea_types.h"
#include "seaclaw/sea_a2a.h"
#include "seaclaw/sea_arena.h"
#include "seaclaw/sea_log.h"
#include "seaclaw/sea_db.h"

#include <stdio.h>
#include <string.h>

/* Stub global referenced by sea_a2a.c */
SeaDb* s_db = NULL;

static u32 s_pass = 0;
static u32 s_fail = 0;

#define TEST(name) \
    do { printf("  %-48s ", name); } while(0)

#define PASS() \
    do { printf("\033[32mPASS\033[0m\n"); s_pass++; } while(0)

#define FAIL(msg) \
    do { printf("\033[31mFAIL\033[0m (%s)\n", msg); s_fail++; } while(0)

static SeaArena s_arena;

/* ── Tests ────────────────────────────────────────────────── */

static void test_a2a_types(void) {
    TEST("a2a_message_types_defined");
    if (SEA_A2A_DELEGATE != 0) { FAIL("DELEGATE != 0"); return; }
    if (SEA_A2A_RESULT != 1) { FAIL("RESULT != 1"); return; }
    if (SEA_A2A_HEARTBEAT != 2) { FAIL("HEARTBEAT != 2"); return; }
    if (SEA_A2A_DISCOVER != 3) { FAIL("DISCOVER != 3"); return; }
    if (SEA_A2A_CANCEL != 4) { FAIL("CANCEL != 4"); return; }
    PASS();
}

static void test_a2a_peer_struct(void) {
    TEST("a2a_peer_struct_initializes");
    SeaA2aPeer peer = {
        .name = "test-agent",
        .endpoint = "http://localhost:8080/a2a",
        .api_key = "secret",
        .healthy = false,
        .last_seen = 0,
    };
    if (strcmp(peer.name, "test-agent") != 0) { FAIL("name"); return; }
    if (strcmp(peer.endpoint, "http://localhost:8080/a2a") != 0) { FAIL("endpoint"); return; }
    if (peer.healthy) { FAIL("should not be healthy"); return; }
    PASS();
}

static void test_a2a_request_struct(void) {
    TEST("a2a_request_struct_initializes");
    SeaA2aRequest req = {
        .task_id = "task-001",
        .task_desc = "Summarize this document",
        .context = "The document is about AI safety.",
        .timeout_ms = 5000,
    };
    if (strcmp(req.task_id, "task-001") != 0) { FAIL("task_id"); return; }
    if (req.timeout_ms != 5000) { FAIL("timeout"); return; }
    PASS();
}

static void test_a2a_delegate_null_peer(void) {
    TEST("a2a_delegate_null_peer_returns_error");
    SeaA2aRequest req = { .task_desc = "test" };
    SeaA2aResult res = sea_a2a_delegate(NULL, &req, &s_arena);
    if (res.success) { FAIL("should not succeed"); return; }
    if (!res.error) { FAIL("should have error msg"); return; }
    sea_arena_reset(&s_arena);
    PASS();
}

static void test_a2a_delegate_null_request(void) {
    TEST("a2a_delegate_null_request_returns_error");
    SeaA2aPeer peer = { .name = "p1", .endpoint = "http://x" };
    SeaA2aResult res = sea_a2a_delegate(&peer, NULL, &s_arena);
    if (res.success) { FAIL("should not succeed"); return; }
    if (!res.error) { FAIL("should have error msg"); return; }
    sea_arena_reset(&s_arena);
    PASS();
}

static void test_a2a_delegate_no_endpoint(void) {
    TEST("a2a_delegate_no_endpoint_returns_error");
    SeaA2aPeer peer = { .name = "p1", .endpoint = NULL };
    SeaA2aRequest req = { .task_desc = "test" };
    SeaA2aResult res = sea_a2a_delegate(&peer, &req, &s_arena);
    if (res.success) { FAIL("should not succeed"); return; }
    sea_arena_reset(&s_arena);
    PASS();
}

static void test_a2a_delegate_unreachable(void) {
    TEST("a2a_delegate_unreachable_peer_fails");
    SeaA2aPeer peer = {
        .name = "ghost",
        .endpoint = "http://192.0.2.1:1/a2a", /* RFC 5737 TEST-NET */
    };
    SeaA2aRequest req = {
        .task_desc = "ping",
        .timeout_ms = 1000,
    };
    SeaA2aResult res = sea_a2a_delegate(&peer, &req, &s_arena);
    if (res.success) { FAIL("should not succeed"); return; }
    if (!res.error) { FAIL("should have error"); return; }
    sea_arena_reset(&s_arena);
    PASS();
}

static void test_a2a_heartbeat_null(void) {
    TEST("a2a_heartbeat_null_returns_false");
    bool alive = sea_a2a_heartbeat(NULL, &s_arena);
    if (alive) { FAIL("should be false"); return; }
    sea_arena_reset(&s_arena);
    PASS();
}

static void test_a2a_heartbeat_no_endpoint(void) {
    TEST("a2a_heartbeat_no_endpoint_returns_false");
    SeaA2aPeer peer = { .name = "p1", .endpoint = NULL };
    bool alive = sea_a2a_heartbeat(&peer, &s_arena);
    if (alive) { FAIL("should be false"); return; }
    sea_arena_reset(&s_arena);
    PASS();
}

static void test_a2a_discover_null(void) {
    TEST("a2a_discover_null_returns_0");
    i32 count = sea_a2a_discover(NULL, NULL, 0, &s_arena);
    if (count != 0) { FAIL("should be 0"); return; }
    sea_arena_reset(&s_arena);
    PASS();
}

static void test_a2a_discover_zero_max(void) {
    TEST("a2a_discover_zero_max_returns_0");
    SeaA2aPeer peers[4];
    i32 count = sea_a2a_discover("http://example.com/agents", peers, 0, &s_arena);
    if (count != 0) { FAIL("should be 0"); return; }
    sea_arena_reset(&s_arena);
    PASS();
}

static void test_a2a_result_struct(void) {
    TEST("a2a_result_struct_defaults");
    SeaA2aResult res = {0};
    if (res.success) { FAIL("default should be false"); return; }
    if (res.verified) { FAIL("default should be false"); return; }
    if (res.latency_ms != 0) { FAIL("default should be 0"); return; }
    PASS();
}

/* ── Main ─────────────────────────────────────────────────── */

int main(void) {
    sea_log_init(SEA_LOG_WARN);
    sea_arena_create(&s_arena, 16 * 1024);

    printf("\n  \033[1mtest_a2a\033[0m\n");

    test_a2a_types();
    test_a2a_peer_struct();
    test_a2a_request_struct();
    test_a2a_delegate_null_peer();
    test_a2a_delegate_null_request();
    test_a2a_delegate_no_endpoint();
    test_a2a_delegate_unreachable();
    test_a2a_heartbeat_null();
    test_a2a_heartbeat_no_endpoint();
    test_a2a_discover_null();
    test_a2a_discover_zero_max();
    test_a2a_result_struct();

    sea_arena_destroy(&s_arena);

    printf("  Results: %u passed, %u failed\n\n", s_pass, s_fail);
    return s_fail > 0 ? 1 : 0;
}
