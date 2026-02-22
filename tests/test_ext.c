/*
 * test_ext.c — Tests for Extension Registry, CLI Dispatch, and Dynamic Tools
 *
 * Tests: sea_ext.h, sea_cli.h, sea_tools.h (v2 dynamic registration)
 */

#include "seaclaw/sea_types.h"
#include "seaclaw/sea_ext.h"
#include "seaclaw/sea_cli.h"
#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_arena.h"
#include "seaclaw/sea_log.h"

#include "seaclaw/sea_db.h"
#include "seaclaw/sea_agent.h"
#include "seaclaw/sea_bus.h"
#include "seaclaw/sea_cron.h"
#include "seaclaw/sea_memory.h"
#include "seaclaw/sea_recall.h"
#include "seaclaw/sea_usage.h"
#include "seaclaw/sea_mesh.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

/* Stub globals referenced by tool implementations and sea_agent */
SeaDb*               s_db = NULL;
SeaBus*              s_bus_ptr = NULL;
SeaCronScheduler*    s_cron = NULL;
SeaMemory*           s_memory = NULL;
SeaUsageTracker*     s_usage = NULL;
SeaRecall*           s_recall = NULL;
SeaMesh*             s_mesh = NULL;
SeaAgentConfig       s_agent_cfg;

static u32 s_pass = 0;
static u32 s_fail = 0;

#define TEST(name) \
    do { printf("  %-40s ", name); } while(0)

#define PASS() \
    do { printf("\033[32mPASS\033[0m\n"); s_pass++; } while(0)

#define FAIL(msg) \
    do { printf("\033[31mFAIL\033[0m (%s)\n", msg); s_fail++; } while(0)

/* ── Mock extension ───────────────────────────────────────── */

static SeaError mock_ext_init(SeaExtension* self, SeaArena* arena) {
    (void)arena;
    self->data = (void*)42;
    return SEA_OK;
}

static void mock_ext_destroy(SeaExtension* self) {
    self->data = NULL;
}

static i32 mock_ext_health(const SeaExtension* self) {
    (void)self;
    return 85;
}

/* ── Mock tool ────────────────────────────────────────────── */

static SeaError mock_tool_func(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    (void)args;
    const char* msg = "mock_tool_output";
    u32 len = (u32)strlen(msg);
    u8* dst = (u8*)sea_arena_alloc(arena, len, 1);
    if (!dst) return SEA_ERR_OOM;
    memcpy(dst, msg, len);
    output->data = dst;
    output->len = len;
    return SEA_OK;
}

/* ── Mock CLI subcommand ──────────────────────────────────── */

static int s_mock_cmd_called = 0;

static int mock_cmd(int argc, char** argv) {
    (void)argc; (void)argv;
    s_mock_cmd_called = 1;
    return 42;
}

/* ── Extension Registry Tests ─────────────────────────────── */

static void test_ext_init(void) {
    TEST("ext_init");
    SeaExtRegistry reg;
    sea_ext_init(&reg);
    if (sea_ext_count(&reg) != 0) { FAIL("count not 0"); return; }
    PASS();
}

static void test_ext_register(void) {
    TEST("ext_register");
    SeaExtRegistry reg;
    sea_ext_init(&reg);

    SeaExtension ext = {0};
    strncpy(ext.name, "test_ext", SEA_EXT_NAME_MAX - 1);
    strncpy(ext.version, "1.0.0", SEA_EXT_VERSION_MAX - 1);
    ext.type = SEA_EXT_TYPE_TOOL;
    ext.init = mock_ext_init;
    ext.destroy = mock_ext_destroy;
    ext.health = mock_ext_health;

    SeaError err = sea_ext_register(&reg, &ext);
    if (err != SEA_OK) { FAIL("register failed"); return; }
    if (sea_ext_count(&reg) != 1) { FAIL("count not 1"); return; }
    PASS();
}

static void test_ext_find(void) {
    TEST("ext_find");
    SeaExtRegistry reg;
    sea_ext_init(&reg);

    SeaExtension ext = {0};
    strncpy(ext.name, "findme", SEA_EXT_NAME_MAX - 1);
    strncpy(ext.version, "2.0", SEA_EXT_VERSION_MAX - 1);
    ext.type = SEA_EXT_TYPE_MEMORY;
    sea_ext_register(&reg, &ext);

    SeaExtension* found = sea_ext_find(&reg, "findme");
    if (!found) { FAIL("not found"); return; }
    if (found->type != SEA_EXT_TYPE_MEMORY) { FAIL("wrong type"); return; }

    SeaExtension* missing = sea_ext_find(&reg, "nope");
    if (missing) { FAIL("should be NULL"); return; }
    PASS();
}

static void test_ext_duplicate(void) {
    TEST("ext_duplicate_rejected");
    SeaExtRegistry reg;
    sea_ext_init(&reg);

    SeaExtension ext = {0};
    strncpy(ext.name, "dup", SEA_EXT_NAME_MAX - 1);
    sea_ext_register(&reg, &ext);

    SeaError err = sea_ext_register(&reg, &ext);
    if (err != SEA_ERR_ALREADY_EXISTS) { FAIL("should reject duplicate"); return; }
    PASS();
}

static void test_ext_init_all(void) {
    TEST("ext_init_all");
    SeaExtRegistry reg;
    sea_ext_init(&reg);

    SeaExtension ext = {0};
    strncpy(ext.name, "initable", SEA_EXT_NAME_MAX - 1);
    ext.init = mock_ext_init;
    ext.destroy = mock_ext_destroy;
    sea_ext_register(&reg, &ext);

    SeaArena arena;
    sea_arena_create(&arena, 4096);
    SeaError err = sea_ext_init_all(&reg, &arena);
    if (err != SEA_OK) { FAIL("init_all failed"); sea_arena_destroy(&arena); return; }
    if (!ext.enabled) { FAIL("not enabled"); sea_arena_destroy(&arena); return; }
    sea_ext_destroy_all(&reg);
    sea_arena_destroy(&arena);
    PASS();
}

static void test_ext_health(void) {
    TEST("ext_health_score");
    SeaExtRegistry reg;
    sea_ext_init(&reg);

    SeaExtension ext = {0};
    strncpy(ext.name, "healthy", SEA_EXT_NAME_MAX - 1);
    ext.health = mock_ext_health;
    ext.enabled = true;
    sea_ext_register(&reg, &ext);

    i32 score = sea_ext_health(&reg);
    if (score != 85) { FAIL("expected 85"); return; }
    PASS();
}

static void test_ext_count_by_type(void) {
    TEST("ext_count_by_type");
    SeaExtRegistry reg;
    sea_ext_init(&reg);

    SeaExtension e1 = {0}; strncpy(e1.name, "t1", SEA_EXT_NAME_MAX - 1); e1.type = SEA_EXT_TYPE_TOOL;
    SeaExtension e2 = {0}; strncpy(e2.name, "t2", SEA_EXT_NAME_MAX - 1); e2.type = SEA_EXT_TYPE_TOOL;
    SeaExtension e3 = {0}; strncpy(e3.name, "c1", SEA_EXT_NAME_MAX - 1); e3.type = SEA_EXT_TYPE_CHANNEL;
    sea_ext_register(&reg, &e1);
    sea_ext_register(&reg, &e2);
    sea_ext_register(&reg, &e3);

    if (sea_ext_count_by_type(&reg, SEA_EXT_TYPE_TOOL) != 2) { FAIL("expected 2 tools"); return; }
    if (sea_ext_count_by_type(&reg, SEA_EXT_TYPE_CHANNEL) != 1) { FAIL("expected 1 channel"); return; }
    if (sea_ext_count_by_type(&reg, SEA_EXT_TYPE_MEMORY) != 0) { FAIL("expected 0 memory"); return; }
    PASS();
}

/* ── CLI Dispatch Tests ───────────────────────────────────── */

static void test_cli_init(void) {
    TEST("cli_init");
    SeaCli cli;
    sea_cli_init(&cli);
    /* Should have 4 built-in commands: doctor, onboard, version, help */
    if (cli.count != 4) { FAIL("expected 4 built-in commands"); return; }
    PASS();
}

static void test_cli_register(void) {
    TEST("cli_register");
    SeaCli cli;
    sea_cli_init(&cli);
    u32 before = cli.count;

    SeaError err = sea_cli_register(&cli, "test_cmd", "A test", "sea_claw test_cmd", mock_cmd);
    if (err != SEA_OK) { FAIL("register failed"); return; }
    if (cli.count != before + 1) { FAIL("count not incremented"); return; }
    PASS();
}

static void test_cli_find(void) {
    TEST("cli_find");
    SeaCli cli;
    sea_cli_init(&cli);

    const SeaCliCmd* cmd = sea_cli_find(&cli, "doctor");
    if (!cmd) { FAIL("doctor not found"); return; }
    if (strcmp(cmd->name, "doctor") != 0) { FAIL("wrong name"); return; }

    const SeaCliCmd* missing = sea_cli_find(&cli, "nonexistent");
    if (missing) { FAIL("should be NULL"); return; }
    PASS();
}

static void test_cli_dispatch(void) {
    TEST("cli_dispatch");
    SeaCli cli;
    sea_cli_init(&cli);
    sea_cli_register(&cli, "mockcmd", "Mock", "sea_claw mockcmd", mock_cmd);

    s_mock_cmd_called = 0;
    char* argv[] = {"sea_claw", "mockcmd"};
    int ret = sea_cli_dispatch(&cli, 2, argv);
    if (ret != 42) { FAIL("expected return 42"); return; }
    if (!s_mock_cmd_called) { FAIL("mock not called"); return; }
    PASS();
}

static void test_cli_dispatch_unknown(void) {
    TEST("cli_dispatch_unknown");
    SeaCli cli;
    sea_cli_init(&cli);

    char* argv[] = {"sea_claw", "bogus"};
    int ret = sea_cli_dispatch(&cli, 2, argv);
    if (ret != -1) { FAIL("expected -1 for unknown"); return; }
    PASS();
}

static void test_cli_dispatch_flag(void) {
    TEST("cli_dispatch_skips_flags");
    SeaCli cli;
    sea_cli_init(&cli);

    char* argv[] = {"sea_claw", "--doctor"};
    int ret = sea_cli_dispatch(&cli, 2, argv);
    if (ret != -1) { FAIL("should skip --flags"); return; }
    PASS();
}

/* ── Dynamic Tool Registration Tests ──────────────────────── */

static void test_tool_hash_lookup(void) {
    TEST("tool_hash_lookup");
    sea_tools_init();

    const SeaTool* echo = sea_tool_by_name("echo");
    if (!echo) { FAIL("echo not found"); return; }
    if (echo->id != 1) { FAIL("wrong id"); return; }

    const SeaTool* agent = sea_tool_by_name("agent_zero");
    if (!agent) { FAIL("agent_zero not found"); return; }
    if (agent->id != 58) { FAIL("wrong id for agent_zero"); return; }

    const SeaTool* missing = sea_tool_by_name("nonexistent_tool");
    if (missing) { FAIL("should be NULL"); return; }
    PASS();
}

static void test_tool_dynamic_register(void) {
    TEST("tool_dynamic_register");
    sea_tools_init();

    u32 before = sea_tools_count();
    SeaError err = sea_tool_register("mock_dynamic", "A mock dynamic tool", mock_tool_func);
    if (err != SEA_OK) { FAIL("register failed"); return; }
    if (sea_tools_count() != before + 1) { FAIL("count not incremented"); return; }
    if (sea_tools_dynamic_count() < 1) { FAIL("dynamic count wrong"); return; }

    const SeaTool* t = sea_tool_by_name("mock_dynamic");
    if (!t) { FAIL("not found by name"); return; }
    if (t->id != before + 1) { FAIL("wrong id"); return; }
    PASS();
}

static void test_tool_dynamic_exec(void) {
    TEST("tool_dynamic_exec");
    /* mock_dynamic should still be registered from previous test */
    SeaArena arena;
    sea_arena_create(&arena, 4096);

    SeaSlice output;
    SeaError err = sea_tool_exec("mock_dynamic", SEA_SLICE_EMPTY, &arena, &output);
    if (err != SEA_OK) { FAIL("exec failed"); sea_arena_destroy(&arena); return; }
    if (output.len == 0) { FAIL("empty output"); sea_arena_destroy(&arena); return; }
    if (memcmp(output.data, "mock_tool_output", 16) != 0) { FAIL("wrong output"); sea_arena_destroy(&arena); return; }

    sea_arena_destroy(&arena);
    PASS();
}

static void test_tool_dynamic_duplicate(void) {
    TEST("tool_dynamic_duplicate_rejected");
    SeaError err = sea_tool_register("mock_dynamic", "dup", mock_tool_func);
    if (err != SEA_ERR_ALREADY_EXISTS) { FAIL("should reject duplicate"); return; }
    PASS();
}

static void test_tool_dynamic_unregister(void) {
    TEST("tool_dynamic_unregister");
    u32 before = sea_tools_count();
    SeaError err = sea_tool_unregister("mock_dynamic");
    if (err != SEA_OK) { FAIL("unregister failed"); return; }
    if (sea_tools_count() != before - 1) { FAIL("count not decremented"); return; }

    const SeaTool* t = sea_tool_by_name("mock_dynamic");
    if (t) { FAIL("should be NULL after unregister"); return; }

    /* Static tools should still work */
    const SeaTool* echo = sea_tool_by_name("echo");
    if (!echo) { FAIL("echo lost after unregister"); return; }
    PASS();
}

static void test_tool_unregister_missing(void) {
    TEST("tool_unregister_missing");
    SeaError err = sea_tool_unregister("never_existed");
    if (err != SEA_ERR_NOT_FOUND) { FAIL("should return NOT_FOUND"); return; }
    PASS();
}

/* ── Main ─────────────────────────────────────────────────── */

int main(void) {
    sea_log_init(SEA_LOG_WARN); /* Suppress INFO during tests */

    printf("\n\033[1m  test_ext — Extension, CLI & Dynamic Tools\033[0m\n\n");

    /* Extension Registry */
    test_ext_init();
    test_ext_register();
    test_ext_find();
    test_ext_duplicate();
    test_ext_init_all();
    test_ext_health();
    test_ext_count_by_type();

    /* CLI Dispatch */
    test_cli_init();
    test_cli_register();
    test_cli_find();
    test_cli_dispatch();
    test_cli_dispatch_unknown();
    test_cli_dispatch_flag();

    /* Dynamic Tool Registration */
    test_tool_hash_lookup();
    test_tool_dynamic_register();
    test_tool_dynamic_exec();
    test_tool_dynamic_duplicate();
    test_tool_dynamic_unregister();
    test_tool_unregister_missing();

    printf("\n  ────────────────────────────────────────\n");
    printf("  \033[1mResults:\033[0m %u passed, %u failed\n\n", s_pass, s_fail);

    return s_fail > 0 ? 1 : 0;
}
