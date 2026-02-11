/*
 * test_memory.c — Long-Term Memory Tests
 *
 * Tests workspace creation, bootstrap files, daily notes,
 * memory read/write/append, and context building.
 */

#include "seaclaw/sea_memory.h"
#include "seaclaw/sea_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

static int s_pass = 0;
static int s_fail = 0;

#define TEST(name) printf("  [TEST] %s ... ", name)
#define PASS() do { printf("\033[32mPASS\033[0m\n"); s_pass++; } while(0)
#define FAIL(msg) do { printf("\033[31mFAIL: %s\033[0m\n", msg); s_fail++; } while(0)

#define TEST_WORKSPACE "/tmp/test_seaclaw_memory"

/* Clean up test workspace */
static void cleanup(void) {
    /* Remove test workspace recursively */
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", TEST_WORKSPACE);
    (void)system(cmd);
}

/* ── Test: Init creates workspace ─────────────────────────── */

static void test_init(void) {
    TEST("init_creates_workspace");
    cleanup();

    SeaMemory mem;
    SeaError err = sea_memory_init(&mem, TEST_WORKSPACE, 64 * 1024);
    if (err != SEA_OK) { FAIL("init failed"); return; }

    struct stat st;
    if (stat(TEST_WORKSPACE, &st) != 0) { FAIL("workspace not created"); sea_memory_destroy(&mem); return; }
    if (!S_ISDIR(st.st_mode)) { FAIL("workspace not a dir"); sea_memory_destroy(&mem); return; }

    /* Check notes subdir */
    char notes[256];
    snprintf(notes, sizeof(notes), "%s/notes", TEST_WORKSPACE);
    if (stat(notes, &st) != 0) { FAIL("notes dir not created"); sea_memory_destroy(&mem); return; }

    if (strcmp(sea_memory_workspace(&mem), TEST_WORKSPACE) != 0) {
        FAIL("wrong workspace path");
        sea_memory_destroy(&mem);
        return;
    }

    sea_memory_destroy(&mem);
    PASS();
}

/* ── Test: Create default bootstrap files ─────────────────── */

static void test_defaults(void) {
    TEST("create_defaults");
    cleanup();

    SeaMemory mem;
    sea_memory_init(&mem, TEST_WORKSPACE, 64 * 1024);
    sea_memory_create_defaults(&mem);

    struct stat st;
    char path[256];

    snprintf(path, sizeof(path), "%s/IDENTITY.md", TEST_WORKSPACE);
    if (stat(path, &st) != 0) { FAIL("IDENTITY.md not created"); sea_memory_destroy(&mem); return; }

    snprintf(path, sizeof(path), "%s/SOUL.md", TEST_WORKSPACE);
    if (stat(path, &st) != 0) { FAIL("SOUL.md not created"); sea_memory_destroy(&mem); return; }

    snprintf(path, sizeof(path), "%s/USER.md", TEST_WORKSPACE);
    if (stat(path, &st) != 0) { FAIL("USER.md not created"); sea_memory_destroy(&mem); return; }

    snprintf(path, sizeof(path), "%s/AGENTS.md", TEST_WORKSPACE);
    if (stat(path, &st) != 0) { FAIL("AGENTS.md not created"); sea_memory_destroy(&mem); return; }

    snprintf(path, sizeof(path), "%s/MEMORY.md", TEST_WORKSPACE);
    if (stat(path, &st) != 0) { FAIL("MEMORY.md not created"); sea_memory_destroy(&mem); return; }

    sea_memory_destroy(&mem);
    PASS();
}

/* ── Test: Read bootstrap file ────────────────────────────── */

static void test_read_bootstrap(void) {
    TEST("read_bootstrap");
    cleanup();

    SeaMemory mem;
    sea_memory_init(&mem, TEST_WORKSPACE, 64 * 1024);
    sea_memory_create_defaults(&mem);

    const char* identity = sea_memory_read_bootstrap(&mem, "IDENTITY.md");
    if (!identity) { FAIL("identity null"); sea_memory_destroy(&mem); return; }
    if (strstr(identity, "Sea-Claw") == NULL) { FAIL("identity missing Sea-Claw"); sea_memory_destroy(&mem); return; }

    sea_memory_destroy(&mem);
    PASS();
}

/* ── Test: Write and read memory ──────────────────────────── */

static void test_write_read_memory(void) {
    TEST("write_read_memory");
    cleanup();

    SeaMemory mem;
    sea_memory_init(&mem, TEST_WORKSPACE, 64 * 1024);

    sea_memory_write(&mem, "# Facts\n- User likes C\n- Project is Sea-Claw\n");

    const char* content = sea_memory_read(&mem);
    if (!content) { FAIL("read null"); sea_memory_destroy(&mem); return; }
    if (strstr(content, "User likes C") == NULL) { FAIL("content wrong"); sea_memory_destroy(&mem); return; }

    sea_memory_destroy(&mem);
    PASS();
}

/* ── Test: Append to memory ───────────────────────────────── */

static void test_append_memory(void) {
    TEST("append_memory");
    cleanup();

    SeaMemory mem;
    sea_memory_init(&mem, TEST_WORKSPACE, 64 * 1024);

    sea_memory_write(&mem, "Line 1\n");
    sea_memory_append(&mem, "Line 2\n");

    const char* content = sea_memory_read(&mem);
    if (!content) { FAIL("read null"); sea_memory_destroy(&mem); return; }
    if (strstr(content, "Line 1") == NULL) { FAIL("missing line 1"); sea_memory_destroy(&mem); return; }
    if (strstr(content, "Line 2") == NULL) { FAIL("missing line 2"); sea_memory_destroy(&mem); return; }

    sea_memory_destroy(&mem);
    PASS();
}

/* ── Test: Daily notes ────────────────────────────────────── */

static void test_daily_notes(void) {
    TEST("daily_notes");
    cleanup();

    SeaMemory mem;
    sea_memory_init(&mem, TEST_WORKSPACE, 64 * 1024);

    sea_memory_append_daily(&mem, "Worked on Phase 10 today.");
    sea_memory_append_daily(&mem, "Bus tests all passing.");

    const char* daily = sea_memory_read_daily(&mem);
    if (!daily) { FAIL("daily null"); sea_memory_destroy(&mem); return; }
    if (strstr(daily, "Phase 10") == NULL) { FAIL("missing Phase 10"); sea_memory_destroy(&mem); return; }
    if (strstr(daily, "Bus tests") == NULL) { FAIL("missing Bus tests"); sea_memory_destroy(&mem); return; }

    sea_memory_destroy(&mem);
    PASS();
}

/* ── Test: Build context ──────────────────────────────────── */

static void test_build_context(void) {
    TEST("build_context");
    cleanup();

    SeaMemory mem;
    sea_memory_init(&mem, TEST_WORKSPACE, 64 * 1024);
    sea_memory_create_defaults(&mem);
    sea_memory_write(&mem, "- User prefers concise answers\n");
    sea_memory_append_daily(&mem, "Started v2 development.");

    SeaArena arena;
    sea_arena_create(&arena, 64 * 1024);

    const char* ctx = sea_memory_build_context(&mem, &arena);
    if (!ctx) { FAIL("context null"); }
    else if (strstr(ctx, "Identity") == NULL) { FAIL("missing Identity section"); }
    else if (strstr(ctx, "Sea-Claw") == NULL) { FAIL("missing Sea-Claw in identity"); }
    else if (strstr(ctx, "Long-Term Memory") == NULL) { FAIL("missing memory section"); }
    else if (strstr(ctx, "concise answers") == NULL) { FAIL("missing memory content"); }
    else if (strstr(ctx, "Daily Notes") == NULL) { FAIL("missing daily notes"); }
    else { PASS(); }

    sea_arena_destroy(&arena);
    sea_memory_destroy(&mem);
}

/* ── Test: Write bootstrap file ───────────────────────────── */

static void test_write_bootstrap(void) {
    TEST("write_bootstrap");
    cleanup();

    SeaMemory mem;
    sea_memory_init(&mem, TEST_WORKSPACE, 64 * 1024);

    sea_memory_write_bootstrap(&mem, "CUSTOM.md", "# Custom\nHello world\n");

    const char* content = sea_memory_read_bootstrap(&mem, "CUSTOM.md");
    if (!content) { FAIL("read null"); sea_memory_destroy(&mem); return; }
    if (strstr(content, "Hello world") == NULL) { FAIL("wrong content"); sea_memory_destroy(&mem); return; }

    sea_memory_destroy(&mem);
    PASS();
}

/* ── Main ─────────────────────────────────────────────────── */

int main(void) {
    sea_log_init(SEA_LOG_WARN);

    printf("\n\033[1m=== Sea-Claw Memory Tests ===\033[0m\n\n");

    test_init();
    test_defaults();
    test_read_bootstrap();
    test_write_read_memory();
    test_append_memory();
    test_daily_notes();
    test_build_context();
    test_write_bootstrap();

    cleanup();

    printf("\n\033[1mResults: %d passed, %d failed\033[0m\n\n", s_pass, s_fail);
    return s_fail > 0 ? 1 : 0;
}
