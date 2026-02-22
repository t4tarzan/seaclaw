/*
 * test_heartbeat.c — Tests for Heartbeat, Tool Allowlist, and Cron Agent
 *
 * Tests: sea_heartbeat.h, sea_auth.h (tool allowlist), sea_cron.h (agent type)
 */

#include "seaclaw/sea_types.h"
#include "seaclaw/sea_heartbeat.h"
#include "seaclaw/sea_auth.h"
#include "seaclaw/sea_memory.h"
#include "seaclaw/sea_bus.h"
#include "seaclaw/sea_log.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

static u32 s_pass = 0;
static u32 s_fail = 0;

#define TEST(name) \
    do { printf("  %-48s ", name); } while(0)

#define PASS() \
    do { printf("\033[32mPASS\033[0m\n"); s_pass++; } while(0)

#define FAIL(msg) \
    do { printf("\033[31mFAIL\033[0m (%s)\n", msg); s_fail++; } while(0)

/* ── Helpers ──────────────────────────────────────────────── */

static void write_file(const char* path, const char* content) {
    FILE* f = fopen(path, "w");
    if (f) { fputs(content, f); fclose(f); }
}

/* ── Heartbeat Tests ──────────────────────────────────────── */

static void test_heartbeat_parse(void) {
    TEST("heartbeat_parse_pending_tasks");

    char tmpdir[] = "/tmp/seabot_hb_XXXXXX";
    if (!mkdtemp(tmpdir)) { FAIL("mkdtemp"); return; }

    /* Init memory system pointing at tmpdir */
    SeaMemory mem;
    sea_memory_init(&mem, tmpdir, 16 * 1024);

    /* Write HEARTBEAT.md */
    char hb_path[512];
    snprintf(hb_path, sizeof(hb_path), "%s/HEARTBEAT.md", tmpdir);
    write_file(hb_path,
        "# Heartbeat Tasks\n"
        "- [ ] Check inbox and draft replies\n"
        "- [x] Already done item\n"
        "- [ ] Summarize today's meetings\n"
        "- [ ] Review PR #42\n"
    );

    SeaHeartbeat hb;
    sea_heartbeat_init(&hb, &mem, NULL, 60);

    SeaHeartbeatTask tasks[8];
    u32 count = sea_heartbeat_parse(&hb, tasks, 8);

    if (count != 4) { FAIL("expected 4 tasks"); }
    else {
        u32 pending = 0;
        for (u32 i = 0; i < count; i++) {
            if (!tasks[i].completed) pending++;
        }
        if (pending != 3) { FAIL("expected 3 pending"); }
        else { PASS(); }
    }

    unlink(hb_path);
    rmdir(tmpdir);
    sea_memory_destroy(&mem);
}

static void test_heartbeat_parse_empty(void) {
    TEST("heartbeat_parse_no_file_returns_0");

    char tmpdir[] = "/tmp/seabot_hb_empty_XXXXXX";
    if (!mkdtemp(tmpdir)) { FAIL("mkdtemp"); return; }

    SeaMemory mem;
    sea_memory_init(&mem, tmpdir, 16 * 1024);

    SeaHeartbeat hb;
    sea_heartbeat_init(&hb, &mem, NULL, 60);

    SeaHeartbeatTask tasks[8];
    u32 count = sea_heartbeat_parse(&hb, tasks, 8);
    if (count != 0) { FAIL("expected 0"); }
    else { PASS(); }

    rmdir(tmpdir);
    sea_memory_destroy(&mem);
}

static void test_heartbeat_tick_interval(void) {
    TEST("heartbeat_tick_respects_interval");

    char tmpdir[] = "/tmp/seabot_hb_tick_XXXXXX";
    if (!mkdtemp(tmpdir)) { FAIL("mkdtemp"); return; }

    SeaMemory mem;
    sea_memory_init(&mem, tmpdir, 16 * 1024);

    SeaBus bus;
    sea_bus_init(&bus, 4096);

    char hb_path[512];
    snprintf(hb_path, sizeof(hb_path), "%s/HEARTBEAT.md", tmpdir);
    write_file(hb_path, "- [ ] Test task\n");

    SeaHeartbeat hb;
    sea_heartbeat_init(&hb, &mem, &bus, 3600); /* 1 hour interval */

    /* First tick should fire (last_check == 0) */
    u32 injected = sea_heartbeat_tick(&hb);
    if (injected != 1) { FAIL("first tick should inject 1"); goto cleanup; }

    /* Second tick immediately should NOT fire */
    u32 injected2 = sea_heartbeat_tick(&hb);
    if (injected2 != 0) { FAIL("second tick should inject 0"); goto cleanup; }

    if (sea_heartbeat_check_count(&hb) != 1) { FAIL("check count should be 1"); goto cleanup; }
    if (sea_heartbeat_injected_count(&hb) != 1) { FAIL("injected count should be 1"); goto cleanup; }

    PASS();

cleanup:
    unlink(hb_path);
    rmdir(tmpdir);
    sea_bus_destroy(&bus);
    sea_memory_destroy(&mem);
}

static void test_heartbeat_trigger(void) {
    TEST("heartbeat_trigger_forces_immediate");

    char tmpdir[] = "/tmp/seabot_hb_trig_XXXXXX";
    if (!mkdtemp(tmpdir)) { FAIL("mkdtemp"); return; }

    SeaMemory mem;
    sea_memory_init(&mem, tmpdir, 16 * 1024);

    SeaBus bus;
    sea_bus_init(&bus, 4096);

    char hb_path[512];
    snprintf(hb_path, sizeof(hb_path), "%s/HEARTBEAT.md", tmpdir);
    write_file(hb_path, "- [ ] Urgent task\n- [ ] Another task\n");

    SeaHeartbeat hb;
    sea_heartbeat_init(&hb, &mem, &bus, 3600);

    u32 injected = sea_heartbeat_trigger(&hb);
    if (injected != 2) { FAIL("should inject 2 tasks"); }
    else { PASS(); }

    unlink(hb_path);
    rmdir(tmpdir);
    sea_bus_destroy(&bus);
    sea_memory_destroy(&mem);
}

static void test_heartbeat_complete(void) {
    TEST("heartbeat_complete_marks_done");

    char tmpdir[] = "/tmp/seabot_hb_comp_XXXXXX";
    if (!mkdtemp(tmpdir)) { FAIL("mkdtemp"); return; }

    SeaMemory mem;
    sea_memory_init(&mem, tmpdir, 16 * 1024);

    char hb_path[512];
    snprintf(hb_path, sizeof(hb_path), "%s/HEARTBEAT.md", tmpdir);
    write_file(hb_path,
        "- [ ] Task one\n"
        "- [ ] Task two\n"
    );

    SeaHeartbeat hb;
    sea_heartbeat_init(&hb, &mem, NULL, 60);

    /* Complete task at line 1 */
    SeaError err = sea_heartbeat_complete(&hb, 1);
    if (err != SEA_OK) { FAIL("complete failed"); goto cleanup; }

    /* Re-parse: should have 1 pending, 1 completed */
    SeaHeartbeatTask tasks[4];
    u32 count = sea_heartbeat_parse(&hb, tasks, 4);
    if (count != 2) { FAIL("expected 2 tasks"); goto cleanup; }

    u32 pending = 0;
    for (u32 i = 0; i < count; i++) {
        if (!tasks[i].completed) pending++;
    }
    if (pending != 1) { FAIL("expected 1 pending after complete"); goto cleanup; }

    PASS();

cleanup:
    unlink(hb_path);
    rmdir(tmpdir);
    sea_memory_destroy(&mem);
}

static void test_heartbeat_disabled(void) {
    TEST("heartbeat_disabled_returns_0");

    SeaHeartbeat hb;
    memset(&hb, 0, sizeof(hb));
    hb.enabled = false;

    u32 injected = sea_heartbeat_tick(&hb);
    if (injected != 0) { FAIL("should return 0 when disabled"); return; }
    PASS();
}

/* ── Tool Allowlist Tests ─────────────────────────────────── */

static void test_auth_allow_tool(void) {
    TEST("auth_allow_tool_restricts_access");

    SeaAuth auth;
    sea_auth_init(&auth, true);

    char token[SEA_TOKEN_LEN + 1];
    sea_auth_create_token(&auth, "restricted", SEA_PERM_TOOLS, 0, token);

    /* No allowlist = all tools allowed */
    if (!sea_auth_can_call_tool(&auth, token, "echo")) { FAIL("should allow all initially"); return; }

    /* Add allowlist */
    sea_auth_allow_tool(&auth, token, "echo");
    sea_auth_allow_tool(&auth, token, "file_read");

    /* Allowed tools should work */
    if (!sea_auth_can_call_tool(&auth, token, "echo")) { FAIL("echo should be allowed"); return; }
    if (!sea_auth_can_call_tool(&auth, token, "file_read")) { FAIL("file_read should be allowed"); return; }

    /* Non-listed tool should be blocked */
    if (sea_auth_can_call_tool(&auth, token, "shell_exec")) { FAIL("shell_exec should be blocked"); return; }

    PASS();
}

static void test_auth_allow_tool_no_perm(void) {
    TEST("auth_allow_tool_requires_tools_perm");

    SeaAuth auth;
    sea_auth_init(&auth, true);

    char token[SEA_TOKEN_LEN + 1];
    sea_auth_create_token(&auth, "chat-only", SEA_PERM_CHAT, 0, token);

    /* Even with no allowlist, should fail without TOOLS perm */
    if (sea_auth_can_call_tool(&auth, token, "echo")) { FAIL("should deny without TOOLS perm"); return; }

    PASS();
}

static void test_auth_allow_tool_duplicate(void) {
    TEST("auth_allow_tool_rejects_duplicate");

    SeaAuth auth;
    sea_auth_init(&auth, true);

    char token[SEA_TOKEN_LEN + 1];
    sea_auth_create_token(&auth, "dup-test", SEA_PERM_TOOLS, 0, token);

    sea_auth_allow_tool(&auth, token, "echo");
    SeaError err = sea_auth_allow_tool(&auth, token, "echo");
    if (err != SEA_ERR_ALREADY_EXISTS) { FAIL("should reject duplicate"); return; }

    PASS();
}

static void test_auth_allow_tool_disabled(void) {
    TEST("auth_can_call_tool_allows_all_when_disabled");

    SeaAuth auth;
    sea_auth_init(&auth, false);

    if (!sea_auth_can_call_tool(&auth, "any", "any_tool")) { FAIL("should allow all when disabled"); return; }

    PASS();
}

/* ── Main ─────────────────────────────────────────────────── */

int main(void) {
    sea_log_init(SEA_LOG_WARN);

    printf("\n\033[1m  test_heartbeat — Heartbeat, Tool Allowlist, Cron Agent\033[0m\n\n");

    /* Heartbeat */
    test_heartbeat_parse();
    test_heartbeat_parse_empty();
    test_heartbeat_tick_interval();
    test_heartbeat_trigger();
    test_heartbeat_complete();
    test_heartbeat_disabled();

    /* Tool Allowlist */
    test_auth_allow_tool();
    test_auth_allow_tool_no_perm();
    test_auth_allow_tool_duplicate();
    test_auth_allow_tool_disabled();

    printf("\n  ────────────────────────────────────────\n");
    printf("  \033[1mResults:\033[0m %u passed, %u failed\n\n", s_pass, s_fail);

    return s_fail > 0 ? 1 : 0;
}
