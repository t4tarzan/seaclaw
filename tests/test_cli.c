/*
 * test_cli.c — Tests for CLI Subcommand Dispatch
 *
 * Tests: sea_cli.h (init, register, find, dispatch)
 */

#include "seaclaw/sea_types.h"
#include "seaclaw/sea_cli.h"
#include "seaclaw/sea_log.h"

#include <stdio.h>
#include <string.h>

static u32 s_pass = 0;
static u32 s_fail = 0;

#define TEST(name) \
    do { printf("  %-48s ", name); } while(0)

#define PASS() \
    do { printf("\033[32mPASS\033[0m\n"); s_pass++; } while(0)

#define FAIL(msg) \
    do { printf("\033[31mFAIL\033[0m (%s)\n", msg); s_fail++; } while(0)

/* ── Dummy subcommand for testing ────────────────────────── */

static int s_dummy_called = 0;
static int s_dummy_argc = 0;

static int dummy_cmd(int argc, char** argv) {
    (void)argv;
    s_dummy_called = 1;
    s_dummy_argc = argc;
    return 0;
}

static int failing_cmd(int argc, char** argv) {
    (void)argc; (void)argv;
    return 42;
}

/* ── Tests ────────────────────────────────────────────────── */

static void test_cli_init(void) {
    TEST("cli_init_has_builtins");
    SeaCli cli;
    sea_cli_init(&cli);
    /* Should have at least version and help */
    if (cli.count == 0) { FAIL("no builtins"); return; }
    PASS();
}

static void test_cli_register(void) {
    TEST("cli_register_adds_command");
    SeaCli cli;
    sea_cli_init(&cli);
    u32 before = cli.count;
    SeaError err = sea_cli_register(&cli, "test", "A test command",
                                     "sea_claw test", dummy_cmd);
    if (err != SEA_OK) { FAIL("register failed"); return; }
    if (cli.count != before + 1) { FAIL("count not incremented"); return; }
    PASS();
}

static void test_cli_register_null(void) {
    TEST("cli_register_null_func_fails");
    SeaCli cli;
    sea_cli_init(&cli);
    SeaError err = sea_cli_register(&cli, "bad", "desc", "usage", NULL);
    if (err == SEA_OK) { FAIL("should reject NULL func"); return; }
    PASS();
}

static void test_cli_find(void) {
    TEST("cli_find_registered_command");
    SeaCli cli;
    sea_cli_init(&cli);
    sea_cli_register(&cli, "mytest", "desc", "usage", dummy_cmd);
    const SeaCliCmd* cmd = sea_cli_find(&cli, "mytest");
    if (!cmd) { FAIL("not found"); return; }
    if (strcmp(cmd->name, "mytest") != 0) { FAIL("name mismatch"); return; }
    PASS();
}

static void test_cli_find_missing(void) {
    TEST("cli_find_missing_returns_null");
    SeaCli cli;
    sea_cli_init(&cli);
    const SeaCliCmd* cmd = sea_cli_find(&cli, "nonexistent");
    if (cmd != NULL) { FAIL("should be NULL"); return; }
    PASS();
}

static void test_cli_find_builtin_version(void) {
    TEST("cli_find_builtin_version");
    SeaCli cli;
    sea_cli_init(&cli);
    const SeaCliCmd* cmd = sea_cli_find(&cli, "version");
    if (!cmd) { FAIL("version not found"); return; }
    PASS();
}

static void test_cli_dispatch(void) {
    TEST("cli_dispatch_calls_handler");
    SeaCli cli;
    sea_cli_init(&cli);
    sea_cli_register(&cli, "run", "Run test", "usage", dummy_cmd);

    s_dummy_called = 0;
    s_dummy_argc = 0;
    char* argv[] = { "sea_claw", "run", "arg1" };
    int rc = sea_cli_dispatch(&cli, 3, argv);
    if (rc != 0) { FAIL("dispatch returned error"); return; }
    if (!s_dummy_called) { FAIL("handler not called"); return; }
    if (s_dummy_argc != 2) { FAIL("wrong argc passed"); return; }
    PASS();
}

static void test_cli_dispatch_missing(void) {
    TEST("cli_dispatch_missing_returns_neg1");
    SeaCli cli;
    sea_cli_init(&cli);
    char* argv[] = { "sea_claw", "bogus" };
    int rc = sea_cli_dispatch(&cli, 2, argv);
    if (rc != -1) { FAIL("should return -1"); return; }
    PASS();
}

static void test_cli_dispatch_no_args(void) {
    TEST("cli_dispatch_no_subcommand_returns_neg1");
    SeaCli cli;
    sea_cli_init(&cli);
    char* argv[] = { "sea_claw" };
    int rc = sea_cli_dispatch(&cli, 1, argv);
    if (rc != -1) { FAIL("should return -1 with no subcommand"); return; }
    PASS();
}

static void test_cli_dispatch_propagates_return(void) {
    TEST("cli_dispatch_propagates_return_code");
    SeaCli cli;
    sea_cli_init(&cli);
    sea_cli_register(&cli, "fail", "Failing cmd", "usage", failing_cmd);
    char* argv[] = { "sea_claw", "fail" };
    int rc = sea_cli_dispatch(&cli, 2, argv);
    if (rc != 42) { FAIL("expected 42"); return; }
    PASS();
}

static void test_cli_max_commands(void) {
    TEST("cli_max_is_32");
    if (SEA_CLI_MAX != 32) { FAIL("wrong max"); return; }
    PASS();
}

/* ── Main ─────────────────────────────────────────────────── */

int main(void) {
    sea_log_init(SEA_LOG_WARN);

    printf("\n  \033[1mtest_cli\033[0m\n");

    test_cli_init();
    test_cli_register();
    test_cli_register_null();
    test_cli_find();
    test_cli_find_missing();
    test_cli_find_builtin_version();
    test_cli_dispatch();
    test_cli_dispatch_missing();
    test_cli_dispatch_no_args();
    test_cli_dispatch_propagates_return();
    test_cli_max_commands();

    printf("  Results: %u passed, %u failed\n\n", s_pass, s_fail);
    return s_fail > 0 ? 1 : 0;
}
