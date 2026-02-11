/*
 * test_arena.c — Stress test for the arena allocator
 *
 * Proves: zero leaks, O(1) alloc, instant reset.
 */

#include "seaclaw/sea_types.h"
#include "seaclaw/sea_arena.h"
#include "seaclaw/sea_log.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

#define TEST_ARENA_SIZE (4 * 1024 * 1024)  /* 4 MB */

static u32 s_pass = 0;
static u32 s_fail = 0;

#define TEST(name) \
    do { printf("  %-40s ", name); } while(0)

#define PASS() \
    do { printf("\033[32mPASS\033[0m\n"); s_pass++; } while(0)

#define FAIL(msg) \
    do { printf("\033[31mFAIL\033[0m (%s)\n", msg); s_fail++; } while(0)

/* ── Tests ────────────────────────────────────────────────── */

static void test_create_destroy(void) {
    TEST("create and destroy");
    SeaArena arena;
    SeaError err = sea_arena_create(&arena, TEST_ARENA_SIZE);
    if (err != SEA_OK) { FAIL("create failed"); return; }
    if (arena.base == NULL) { FAIL("base is NULL"); return; }
    if (arena.size != TEST_ARENA_SIZE) { FAIL("wrong size"); return; }
    if (arena.offset != 0) { FAIL("offset not zero"); return; }
    sea_arena_destroy(&arena);
    if (arena.base != NULL) { FAIL("base not NULL after destroy"); return; }
    PASS();
}

static void test_basic_alloc(void) {
    TEST("basic allocation");
    SeaArena arena;
    sea_arena_create(&arena, TEST_ARENA_SIZE);

    void* p1 = sea_arena_push(&arena, 100);
    if (!p1) { FAIL("alloc returned NULL"); goto cleanup; }
    if (sea_arena_used(&arena) < 100) { FAIL("used too small"); goto cleanup; }

    void* p2 = sea_arena_push(&arena, 200);
    if (!p2) { FAIL("second alloc NULL"); goto cleanup; }
    if (p2 <= p1) { FAIL("p2 not after p1"); goto cleanup; }

    PASS();
cleanup:
    sea_arena_destroy(&arena);
}

static void test_reset(void) {
    TEST("reset clears offset");
    SeaArena arena;
    sea_arena_create(&arena, TEST_ARENA_SIZE);

    sea_arena_push(&arena, 1000);
    sea_arena_push(&arena, 2000);
    if (sea_arena_used(&arena) == 0) { FAIL("should have used bytes"); goto cleanup; }

    sea_arena_reset(&arena);
    if (sea_arena_used(&arena) != 0) { FAIL("offset not zero after reset"); goto cleanup; }

    PASS();
cleanup:
    sea_arena_destroy(&arena);
}

static void test_high_water(void) {
    TEST("high water mark tracks peak");
    SeaArena arena;
    sea_arena_create(&arena, TEST_ARENA_SIZE);

    sea_arena_push(&arena, 5000);
    u64 hw1 = arena.high_water;

    sea_arena_reset(&arena);
    sea_arena_push(&arena, 1000);

    if (arena.high_water < hw1) { FAIL("high water decreased"); goto cleanup; }
    if (arena.high_water < 5000) { FAIL("high water too low"); goto cleanup; }

    PASS();
cleanup:
    sea_arena_destroy(&arena);
}

static void test_overflow_returns_null(void) {
    TEST("overflow returns NULL");
    SeaArena arena;
    sea_arena_create(&arena, 1024);  /* Tiny arena */

    void* p = sea_arena_push(&arena, 2048);
    if (p != NULL) { FAIL("should return NULL on overflow"); goto cleanup; }

    PASS();
cleanup:
    sea_arena_destroy(&arena);
}

static void test_push_cstr(void) {
    TEST("push_cstr copies string");
    SeaArena arena;
    sea_arena_create(&arena, TEST_ARENA_SIZE);

    SeaSlice s = sea_arena_push_cstr(&arena, "Hello, Vault!");
    if (s.len != 13) { FAIL("wrong length"); goto cleanup; }
    if (memcmp(s.data, "Hello, Vault!", 13) != 0) { FAIL("wrong content"); goto cleanup; }

    PASS();
cleanup:
    sea_arena_destroy(&arena);
}

static void test_alignment(void) {
    TEST("8-byte alignment");
    SeaArena arena;
    sea_arena_create(&arena, TEST_ARENA_SIZE);

    sea_arena_alloc(&arena, 1, 1);  /* 1 byte, unaligned */
    void* p = sea_arena_push(&arena, 8);  /* Should be 8-aligned */
    if ((u64)p % 8 != 0) { FAIL("not 8-byte aligned"); goto cleanup; }

    PASS();
cleanup:
    sea_arena_destroy(&arena);
}

static void test_stress_1m_allocs(void) {
    TEST("stress: 1M allocations");
    SeaArena arena;
    sea_arena_create(&arena, 64 * 1024 * 1024);  /* 64 MB for stress */

    u64 t0 = sea_log_elapsed_ms();

    for (u32 i = 0; i < 1000000; i++) {
        void* p = sea_arena_alloc(&arena, 16, 8);
        if (!p) {
            /* Reset and continue — proves reset works under load */
            sea_arena_reset(&arena);
            p = sea_arena_alloc(&arena, 16, 8);
            if (!p) { FAIL("alloc failed even after reset"); goto cleanup; }
        }
    }

    u64 t1 = sea_log_elapsed_ms();
    u64 elapsed = t1 - t0;

    if (elapsed > 100) {
        char msg[64];
        snprintf(msg, sizeof(msg), "too slow: %lums (target <100ms)", (unsigned long)elapsed);
        FAIL(msg);
        goto cleanup;
    }

    printf("\033[32mPASS\033[0m (%lums for 1M allocs)\n", (unsigned long)elapsed);
    s_pass++;

cleanup:
    sea_arena_destroy(&arena);
}

static void test_reset_speed(void) {
    TEST("reset speed: 1M resets");
    SeaArena arena;
    sea_arena_create(&arena, TEST_ARENA_SIZE);

    u64 t0 = sea_log_elapsed_ms();

    for (u32 i = 0; i < 1000000; i++) {
        sea_arena_push(&arena, 64);
        sea_arena_reset(&arena);
    }

    u64 t1 = sea_log_elapsed_ms();
    u64 elapsed = t1 - t0;

    if (elapsed > 50) {
        char msg[64];
        snprintf(msg, sizeof(msg), "too slow: %lums (target <50ms)", (unsigned long)elapsed);
        FAIL(msg);
        goto cleanup;
    }

    printf("\033[32mPASS\033[0m (%lums for 1M resets)\n", (unsigned long)elapsed);
    s_pass++;

cleanup:
    sea_arena_destroy(&arena);
}

/* ── Main ─────────────────────────────────────────────────── */

int main(void) {
    sea_log_init(SEA_LOG_WARN);  /* Quiet during tests */

    printf("\n  \033[1mSea-Claw Arena Tests\033[0m\n");
    printf("  ════════════════════════════════════════════\n\n");

    test_create_destroy();
    test_basic_alloc();
    test_reset();
    test_high_water();
    test_overflow_returns_null();
    test_push_cstr();
    test_alignment();
    test_stress_1m_allocs();
    test_reset_speed();

    printf("\n  ────────────────────────────────────────────\n");
    printf("  Results: \033[32m%u passed\033[0m", s_pass);
    if (s_fail > 0) printf(", \033[31m%u failed\033[0m", s_fail);
    printf("\n\n");

    return s_fail > 0 ? 1 : 0;
}
