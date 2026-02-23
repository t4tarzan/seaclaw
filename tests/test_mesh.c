/*
 * test_mesh.c — Tests for Distributed Agent Mesh
 *
 * Tests: sea_mesh.h (init, node register/remove, route, heartbeat,
 *        token generate/validate, status, healthy nodes)
 */

#include "seaclaw/sea_types.h"
#include "seaclaw/sea_mesh.h"
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

/* ── Tests ────────────────────────────────────────────────── */

static void test_mesh_init_captain(void) {
    TEST("mesh_init_captain");
    SeaMesh mesh;
    SeaMeshConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.role = SEA_MESH_CAPTAIN;
    strncpy(cfg.node_name, "captain-1", SEA_MESH_NODE_NAME_MAX - 1);
    cfg.port = 9100;
    SeaError err = sea_mesh_init(&mesh, &cfg, NULL);
    if (err != SEA_OK) { FAIL("init failed"); return; }
    if (!mesh.initialized) { FAIL("not initialized"); return; }
    if (!mesh.running) { FAIL("not running"); return; }
    if (mesh.config.role != SEA_MESH_CAPTAIN) { FAIL("wrong role"); return; }
    sea_mesh_destroy(&mesh);
    PASS();
}

static void test_mesh_init_crew(void) {
    TEST("mesh_init_crew");
    SeaMesh mesh;
    SeaMeshConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.role = SEA_MESH_CREW;
    strncpy(cfg.node_name, "crew-1", SEA_MESH_NODE_NAME_MAX - 1);
    cfg.port = 9101;
    SeaError err = sea_mesh_init(&mesh, &cfg, NULL);
    if (err != SEA_OK) { FAIL("init failed"); return; }
    if (mesh.config.role != SEA_MESH_CREW) { FAIL("wrong role"); return; }
    sea_mesh_destroy(&mesh);
    PASS();
}

static void test_mesh_init_null(void) {
    TEST("mesh_init_null_returns_error");
    SeaError err = sea_mesh_init(NULL, NULL, NULL);
    if (err == SEA_OK) { FAIL("should fail"); return; }
    PASS();
}

static void test_mesh_register_node(void) {
    TEST("mesh_register_node");
    SeaMesh mesh;
    SeaMeshConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.role = SEA_MESH_CAPTAIN;
    strncpy(cfg.node_name, "captain", SEA_MESH_NODE_NAME_MAX - 1);
    sea_mesh_init(&mesh, &cfg, NULL);

    const char* caps[] = { "file_read", "shell_exec" };
    SeaError err = sea_mesh_register_node(&mesh, "worker-1",
                                           "http://192.168.1.10:9101",
                                           caps, 2);
    if (err != SEA_OK) { FAIL("register failed"); sea_mesh_destroy(&mesh); return; }
    if (mesh.node_count != 1) { FAIL("count not 1"); sea_mesh_destroy(&mesh); return; }
    sea_mesh_destroy(&mesh);
    PASS();
}

static void test_mesh_register_reregister(void) {
    TEST("mesh_register_same_node_updates");
    SeaMesh mesh;
    SeaMeshConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.role = SEA_MESH_CAPTAIN;
    strncpy(cfg.node_name, "captain", SEA_MESH_NODE_NAME_MAX - 1);
    sea_mesh_init(&mesh, &cfg, NULL);

    const char* caps1[] = { "echo" };
    sea_mesh_register_node(&mesh, "w1", "http://10.0.0.1:9101", caps1, 1);
    const char* caps2[] = { "echo", "file_read" };
    sea_mesh_register_node(&mesh, "w1", "http://10.0.0.1:9101", caps2, 2);
    if (mesh.node_count != 1) { FAIL("should still be 1"); sea_mesh_destroy(&mesh); return; }
    sea_mesh_destroy(&mesh);
    PASS();
}

static void test_mesh_remove_node(void) {
    TEST("mesh_remove_node");
    SeaMesh mesh;
    SeaMeshConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.role = SEA_MESH_CAPTAIN;
    strncpy(cfg.node_name, "captain", SEA_MESH_NODE_NAME_MAX - 1);
    sea_mesh_init(&mesh, &cfg, NULL);

    const char* caps[] = { "echo" };
    sea_mesh_register_node(&mesh, "w1", "http://10.0.0.1:9101", caps, 1);
    SeaError err = sea_mesh_remove_node(&mesh, "w1");
    if (err != SEA_OK) { FAIL("remove failed"); sea_mesh_destroy(&mesh); return; }
    if (mesh.node_count != 0) { FAIL("count not 0"); sea_mesh_destroy(&mesh); return; }
    sea_mesh_destroy(&mesh);
    PASS();
}

static void test_mesh_remove_missing(void) {
    TEST("mesh_remove_missing_returns_error");
    SeaMesh mesh;
    SeaMeshConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.role = SEA_MESH_CAPTAIN;
    strncpy(cfg.node_name, "captain", SEA_MESH_NODE_NAME_MAX - 1);
    sea_mesh_init(&mesh, &cfg, NULL);

    SeaError err = sea_mesh_remove_node(&mesh, "nonexistent");
    if (err == SEA_OK) { FAIL("should fail"); sea_mesh_destroy(&mesh); return; }
    sea_mesh_destroy(&mesh);
    PASS();
}

static void test_mesh_route_tool(void) {
    TEST("mesh_route_tool_finds_capable_node");
    SeaMesh mesh;
    SeaMeshConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.role = SEA_MESH_CAPTAIN;
    strncpy(cfg.node_name, "captain", SEA_MESH_NODE_NAME_MAX - 1);
    sea_mesh_init(&mesh, &cfg, NULL);

    const char* caps[] = { "file_read", "shell_exec" };
    sea_mesh_register_node(&mesh, "w1", "http://10.0.0.1:9101", caps, 2);

    const SeaMeshNode* node = sea_mesh_route_tool(&mesh, "shell_exec");
    if (!node) { FAIL("not found"); sea_mesh_destroy(&mesh); return; }
    if (strcmp(node->name, "w1") != 0) { FAIL("wrong node"); sea_mesh_destroy(&mesh); return; }
    sea_mesh_destroy(&mesh);
    PASS();
}

static void test_mesh_route_tool_missing(void) {
    TEST("mesh_route_tool_returns_null_for_missing");
    SeaMesh mesh;
    SeaMeshConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.role = SEA_MESH_CAPTAIN;
    strncpy(cfg.node_name, "captain", SEA_MESH_NODE_NAME_MAX - 1);
    sea_mesh_init(&mesh, &cfg, NULL);

    const char* caps[] = { "echo" };
    sea_mesh_register_node(&mesh, "w1", "http://10.0.0.1:9101", caps, 1);

    const SeaMeshNode* node = sea_mesh_route_tool(&mesh, "no_such_tool");
    if (node != NULL) { FAIL("should be NULL"); sea_mesh_destroy(&mesh); return; }
    sea_mesh_destroy(&mesh);
    PASS();
}

static void test_mesh_node_count(void) {
    TEST("mesh_node_count");
    SeaMesh mesh;
    SeaMeshConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.role = SEA_MESH_CAPTAIN;
    strncpy(cfg.node_name, "captain", SEA_MESH_NODE_NAME_MAX - 1);
    sea_mesh_init(&mesh, &cfg, NULL);

    if (sea_mesh_node_count(&mesh) != 0) { FAIL("should be 0"); sea_mesh_destroy(&mesh); return; }
    const char* caps[] = { "echo" };
    sea_mesh_register_node(&mesh, "w1", "http://10.0.0.1:9101", caps, 1);
    sea_mesh_register_node(&mesh, "w2", "http://10.0.0.2:9101", caps, 1);
    if (sea_mesh_node_count(&mesh) != 2) { FAIL("should be 2"); sea_mesh_destroy(&mesh); return; }
    sea_mesh_destroy(&mesh);
    PASS();
}

static void test_mesh_process_heartbeat(void) {
    TEST("mesh_process_heartbeat_updates_node");
    SeaMesh mesh;
    SeaMeshConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.role = SEA_MESH_CAPTAIN;
    strncpy(cfg.node_name, "captain", SEA_MESH_NODE_NAME_MAX - 1);
    sea_mesh_init(&mesh, &cfg, NULL);

    const char* caps[] = { "echo" };
    sea_mesh_register_node(&mesh, "w1", "http://10.0.0.1:9101", caps, 1);
    SeaError err = sea_mesh_process_heartbeat(&mesh, "w1");
    if (err != SEA_OK) { FAIL("heartbeat failed"); sea_mesh_destroy(&mesh); return; }
    sea_mesh_destroy(&mesh);
    PASS();
}

static void test_mesh_heartbeat_missing_node(void) {
    TEST("mesh_heartbeat_missing_node_returns_error");
    SeaMesh mesh;
    SeaMeshConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.role = SEA_MESH_CAPTAIN;
    strncpy(cfg.node_name, "captain", SEA_MESH_NODE_NAME_MAX - 1);
    sea_mesh_init(&mesh, &cfg, NULL);

    SeaError err = sea_mesh_process_heartbeat(&mesh, "ghost");
    if (err == SEA_OK) { FAIL("should fail for unknown node"); sea_mesh_destroy(&mesh); return; }
    sea_mesh_destroy(&mesh);
    PASS();
}

static void test_mesh_token_generate_validate(void) {
    TEST("mesh_token_generate_and_validate");
    SeaMesh mesh;
    SeaMeshConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.role = SEA_MESH_CAPTAIN;
    strncpy(cfg.node_name, "captain", SEA_MESH_NODE_NAME_MAX - 1);
    strncpy(cfg.shared_secret, "test-secret-123", sizeof(cfg.shared_secret) - 1);
    sea_mesh_init(&mesh, &cfg, NULL);

    const char* token = sea_mesh_generate_token(&mesh, &s_arena);
    if (!token) { FAIL("generate returned NULL"); sea_mesh_destroy(&mesh); return; }
    if (!sea_mesh_validate_token(&mesh, token)) { FAIL("validate failed"); sea_mesh_destroy(&mesh); return; }
    sea_arena_reset(&s_arena);
    sea_mesh_destroy(&mesh);
    PASS();
}

static void test_mesh_token_invalid(void) {
    TEST("mesh_token_invalid_rejected");
    SeaMesh mesh;
    SeaMeshConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.role = SEA_MESH_CAPTAIN;
    strncpy(cfg.node_name, "captain", SEA_MESH_NODE_NAME_MAX - 1);
    strncpy(cfg.shared_secret, "secret", sizeof(cfg.shared_secret) - 1);
    sea_mesh_init(&mesh, &cfg, NULL);

    if (sea_mesh_validate_token(&mesh, "bogus")) { FAIL("should reject"); sea_mesh_destroy(&mesh); return; }
    if (sea_mesh_validate_token(&mesh, "123:deadbeef")) { FAIL("should reject bad hash"); sea_mesh_destroy(&mesh); return; }
    sea_mesh_destroy(&mesh);
    PASS();
}

static void test_mesh_status(void) {
    TEST("mesh_status_returns_string");
    SeaMesh mesh;
    SeaMeshConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.role = SEA_MESH_CAPTAIN;
    strncpy(cfg.node_name, "test-cap", SEA_MESH_NODE_NAME_MAX - 1);
    cfg.port = 9100;
    sea_mesh_init(&mesh, &cfg, NULL);

    const char* status = sea_mesh_status(&mesh, &s_arena);
    if (!status) { FAIL("NULL status"); sea_mesh_destroy(&mesh); return; }
    if (strlen(status) < 10) { FAIL("status too short"); sea_mesh_destroy(&mesh); return; }
    sea_arena_reset(&s_arena);
    sea_mesh_destroy(&mesh);
    PASS();
}

static void test_mesh_destroy_safe(void) {
    TEST("mesh_destroy_null_safe");
    sea_mesh_destroy(NULL); /* Should not crash */
    PASS();
}

/* ── Main ─────────────────────────────────────────────────── */

int main(void) {
    sea_log_init(SEA_LOG_WARN);
    sea_arena_create(&s_arena, 16 * 1024);

    printf("\n  \033[1mtest_mesh\033[0m\n");

    test_mesh_init_captain();
    test_mesh_init_crew();
    test_mesh_init_null();
    test_mesh_register_node();
    test_mesh_register_reregister();
    test_mesh_remove_node();
    test_mesh_remove_missing();
    test_mesh_route_tool();
    test_mesh_route_tool_missing();
    test_mesh_node_count();
    test_mesh_process_heartbeat();
    test_mesh_heartbeat_missing_node();
    test_mesh_token_generate_validate();
    test_mesh_token_invalid();
    test_mesh_status();
    test_mesh_destroy_safe();

    sea_arena_destroy(&s_arena);

    printf("  Results: %u passed, %u failed\n\n", s_pass, s_fail);
    return s_fail > 0 ? 1 : 0;
}
