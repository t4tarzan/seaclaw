/*
 * test_tools.c — Tests for Static Tool Registry
 *
 * Tests: sea_tools.h (init, lookup, exec, dynamic register/unregister, count)
 */

#include "seaclaw/sea_types.h"
#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_arena.h"
#include "seaclaw/sea_log.h"

#include <stdio.h>
#include <string.h>

/* Stub globals referenced by tool implementations (defined in main.c) */
#include "seaclaw/sea_recall.h"
#include "seaclaw/sea_memory.h"
#include "seaclaw/sea_cron.h"
#include "seaclaw/sea_db.h"
#include "seaclaw/sea_bus.h"
#include "seaclaw/sea_agent.h"
SeaRecall*          s_recall = NULL;
SeaMemory*          s_memory = NULL;
SeaCronScheduler*   s_cron = NULL;
SeaDb*              s_db = NULL;
SeaBus*             s_bus_ptr = NULL;
SeaAgentConfig      s_agent_cfg = {0};

static u32 s_pass = 0;
static u32 s_fail = 0;

#define TEST(name) \
    do { printf("  %-48s ", name); } while(0)

#define PASS() \
    do { printf("\033[32mPASS\033[0m\n"); s_pass++; } while(0)

#define FAIL(msg) \
    do { printf("\033[31mFAIL\033[0m (%s)\n", msg); s_fail++; } while(0)

static SeaArena s_arena;

/* ── Dummy tool for dynamic registration ─────────────────── */

static SeaError dummy_tool(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    (void)args;
    char* buf = (char*)sea_arena_alloc(arena, 16, 1);
    if (!buf) return SEA_ERR_ARENA_FULL;
    memcpy(buf, "dummy_output", 13);
    output->data = (const u8*)buf;
    output->len = 12;
    return SEA_OK;
}

/* ── Tests ────────────────────────────────────────────────── */

static void test_tools_init(void) {
    TEST("tools_init_populates_registry");
    sea_tools_init();
    u32 count = sea_tools_count();
    if (count == 0) { FAIL("count is 0 after init"); return; }
    PASS();
}

static void test_tools_count(void) {
    TEST("tools_count_returns_static_count");
    u32 count = sea_tools_count();
    /* We know there are 63 static tools */
    if (count < 60) { FAIL("expected at least 60 tools"); return; }
    PASS();
}

static void test_lookup_by_name(void) {
    TEST("tool_by_name_finds_echo");
    const SeaTool* t = sea_tool_by_name("echo");
    if (!t) { FAIL("echo not found"); return; }
    if (strcmp(t->name, "echo") != 0) { FAIL("name mismatch"); return; }
    if (t->id != 1) { FAIL("id mismatch"); return; }
    PASS();
}

static void test_lookup_by_name_missing(void) {
    TEST("tool_by_name_returns_null_for_missing");
    const SeaTool* t = sea_tool_by_name("nonexistent_tool_xyz");
    if (t != NULL) { FAIL("should be NULL"); return; }
    PASS();
}

static void test_lookup_by_name_null(void) {
    TEST("tool_by_name_handles_null");
    const SeaTool* t = sea_tool_by_name(NULL);
    if (t != NULL) { FAIL("should be NULL for NULL input"); return; }
    PASS();
}

static void test_lookup_by_id(void) {
    TEST("tool_by_id_finds_shell_exec");
    const SeaTool* t = sea_tool_by_id(5);
    if (!t) { FAIL("id 5 not found"); return; }
    if (strcmp(t->name, "shell_exec") != 0) { FAIL("name mismatch"); return; }
    PASS();
}

static void test_lookup_by_id_zero(void) {
    TEST("tool_by_id_zero_returns_null");
    const SeaTool* t = sea_tool_by_id(0);
    if (t != NULL) { FAIL("id 0 should return NULL"); return; }
    PASS();
}

static void test_lookup_by_id_out_of_range(void) {
    TEST("tool_by_id_999_returns_null");
    const SeaTool* t = sea_tool_by_id(999);
    if (t != NULL) { FAIL("should return NULL"); return; }
    PASS();
}

static void test_exec_echo(void) {
    TEST("tool_exec_echo_returns_input");
    SeaSlice args = { .data = (const u8*)"hello", .len = 5 };
    SeaSlice output;
    SeaError err = sea_tool_exec("echo", args, &s_arena, &output);
    if (err != SEA_OK) { FAIL("exec failed"); sea_arena_reset(&s_arena); return; }
    if (output.len == 0) { FAIL("empty output"); sea_arena_reset(&s_arena); return; }
    PASS();
    sea_arena_reset(&s_arena);
}

static void test_exec_missing_tool(void) {
    TEST("tool_exec_missing_returns_not_found");
    SeaSlice args = SEA_SLICE_EMPTY;
    SeaSlice output;
    SeaError err = sea_tool_exec("no_such_tool", args, &s_arena, &output);
    if (err != SEA_ERR_TOOL_NOT_FOUND) { FAIL("expected TOOL_NOT_FOUND"); sea_arena_reset(&s_arena); return; }
    PASS();
    sea_arena_reset(&s_arena);
}

static void test_dynamic_register(void) {
    TEST("tool_register_dynamic_tool");
    u32 before = sea_tools_count();
    SeaError err = sea_tool_register("test_dummy", "A test tool", dummy_tool);
    if (err != SEA_OK) { FAIL("register failed"); return; }
    u32 after = sea_tools_count();
    if (after != before + 1) { FAIL("count not incremented"); return; }
    PASS();
}

static void test_dynamic_lookup(void) {
    TEST("dynamic_tool_findable_by_name");
    const SeaTool* t = sea_tool_by_name("test_dummy");
    if (!t) { FAIL("not found"); return; }
    if (strcmp(t->name, "test_dummy") != 0) { FAIL("name mismatch"); return; }
    PASS();
}

static void test_dynamic_exec(void) {
    TEST("dynamic_tool_executable");
    SeaSlice args = SEA_SLICE_EMPTY;
    SeaSlice output;
    SeaError err = sea_tool_exec("test_dummy", args, &s_arena, &output);
    if (err != SEA_OK) { FAIL("exec failed"); sea_arena_reset(&s_arena); return; }
    if (output.len != 12) { FAIL("wrong output length"); sea_arena_reset(&s_arena); return; }
    PASS();
    sea_arena_reset(&s_arena);
}

static void test_dynamic_count(void) {
    TEST("tools_dynamic_count_is_1");
    u32 dc = sea_tools_dynamic_count();
    if (dc != 1) { FAIL("expected 1"); return; }
    PASS();
}

static void test_dynamic_unregister(void) {
    TEST("tool_unregister_removes_tool");
    SeaError err = sea_tool_unregister("test_dummy");
    if (err != SEA_OK) { FAIL("unregister failed"); return; }
    const SeaTool* t = sea_tool_by_name("test_dummy");
    if (t != NULL) { FAIL("still found after unregister"); return; }
    u32 dc = sea_tools_dynamic_count();
    if (dc != 0) { FAIL("dynamic count not 0"); return; }
    PASS();
}

static void test_unregister_missing(void) {
    TEST("tool_unregister_missing_returns_not_found");
    SeaError err = sea_tool_unregister("no_such_tool");
    if (err != SEA_ERR_NOT_FOUND) { FAIL("expected NOT_FOUND"); return; }
    PASS();
}

static void test_duplicate_register(void) {
    TEST("tool_register_duplicate_rejected");
    /* "echo" is a static tool — should reject */
    SeaError err = sea_tool_register("echo", "dup", dummy_tool);
    if (err != SEA_ERR_ALREADY_EXISTS) { FAIL("expected ALREADY_EXISTS"); return; }
    PASS();
}

/* ── Main ─────────────────────────────────────────────────── */

int main(void) {
    sea_log_init(SEA_LOG_WARN);
    sea_arena_create(&s_arena, 16 * 1024);

    printf("\n  \033[1mtest_tools\033[0m\n");

    test_tools_init();
    test_tools_count();
    test_lookup_by_name();
    test_lookup_by_name_missing();
    test_lookup_by_name_null();
    test_lookup_by_id();
    test_lookup_by_id_zero();
    test_lookup_by_id_out_of_range();
    test_exec_echo();
    test_exec_missing_tool();
    test_dynamic_register();
    test_dynamic_lookup();
    test_dynamic_exec();
    test_dynamic_count();
    test_dynamic_unregister();
    test_unregister_missing();
    test_duplicate_register();

    sea_arena_destroy(&s_arena);

    printf("  Results: %u passed, %u failed\n\n", s_pass, s_fail);
    return s_fail > 0 ? 1 : 0;
}
