/*
 * test_config.c — JSON config loader tests
 */

#include "seaclaw/sea_types.h"
#include "seaclaw/sea_config.h"
#include "seaclaw/sea_arena.h"
#include "seaclaw/sea_log.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static u32 s_pass = 0;
static u32 s_fail = 0;

#define TEST(name) \
    do { printf("  %-44s ", name); } while(0)

#define PASS() \
    do { printf("\033[32mPASS\033[0m\n"); s_pass++; } while(0)

#define FAIL(msg) \
    do { printf("\033[31mFAIL\033[0m (%s)\n", msg); s_fail++; } while(0)

#define TEST_CFG_PATH "/tmp/seaclaw_test_config.json"

static void write_file(const char* path, const char* content) {
    FILE* f = fopen(path, "w");
    if (f) {
        fputs(content, f);
        fclose(f);
    }
}

static void test_load_full_config(void) {
    TEST("load full config");
    write_file(TEST_CFG_PATH,
        "{\n"
        "  \"telegram_token\": \"123456:ABCDEF\",\n"
        "  \"telegram_chat_id\": 99887766,\n"
        "  \"db_path\": \"/data/my.db\",\n"
        "  \"log_level\": \"debug\",\n"
        "  \"arena_size_mb\": 32\n"
        "}\n");

    SeaArena arena;
    sea_arena_create(&arena, 8192);
    SeaConfig cfg;

    SeaError err = sea_config_load(&cfg, TEST_CFG_PATH, &arena);
    if (err != SEA_OK) { FAIL("load failed"); goto cleanup; }
    if (!cfg.loaded) { FAIL("not marked loaded"); goto cleanup; }
    if (!cfg.telegram_token || strcmp(cfg.telegram_token, "123456:ABCDEF") != 0) {
        FAIL("token mismatch"); goto cleanup;
    }
    if (cfg.telegram_chat_id != 99887766) { FAIL("chat_id mismatch"); goto cleanup; }
    if (!cfg.db_path || strcmp(cfg.db_path, "/data/my.db") != 0) {
        FAIL("db_path mismatch"); goto cleanup;
    }
    if (!cfg.log_level || strcmp(cfg.log_level, "debug") != 0) {
        FAIL("log_level mismatch"); goto cleanup;
    }
    if (cfg.arena_size_mb != 32) { FAIL("arena_size mismatch"); goto cleanup; }
    PASS();

cleanup:
    sea_arena_destroy(&arena);
}

static void test_defaults_on_missing_file(void) {
    TEST("defaults when file missing");
    unlink(TEST_CFG_PATH);

    SeaArena arena;
    sea_arena_create(&arena, 4096);
    SeaConfig cfg;

    SeaError err = sea_config_load(&cfg, "/tmp/nonexistent_config_xyz.json", &arena);
    if (err != SEA_ERR_IO) { FAIL("expected IO error"); goto cleanup; }
    if (!cfg.db_path || strcmp(cfg.db_path, "seaclaw.db") != 0) {
        FAIL("default db_path wrong"); goto cleanup;
    }
    if (!cfg.log_level || strcmp(cfg.log_level, "info") != 0) {
        FAIL("default log_level wrong"); goto cleanup;
    }
    if (cfg.arena_size_mb != 16) { FAIL("default arena_size wrong"); goto cleanup; }
    PASS();

cleanup:
    sea_arena_destroy(&arena);
}

static void test_partial_config(void) {
    TEST("partial config fills defaults");
    write_file(TEST_CFG_PATH,
        "{ \"telegram_token\": \"tok123\" }\n");

    SeaArena arena;
    sea_arena_create(&arena, 4096);
    SeaConfig cfg;

    SeaError err = sea_config_load(&cfg, TEST_CFG_PATH, &arena);
    if (err != SEA_OK) { FAIL("load failed"); goto cleanup; }
    if (!cfg.telegram_token || strcmp(cfg.telegram_token, "tok123") != 0) {
        FAIL("token mismatch"); goto cleanup;
    }
    /* Defaults should fill in */
    if (strcmp(cfg.db_path, "seaclaw.db") != 0) { FAIL("default db_path"); goto cleanup; }
    if (cfg.arena_size_mb != 16) { FAIL("default arena_size"); goto cleanup; }
    PASS();

cleanup:
    sea_arena_destroy(&arena);
}

static void test_empty_object(void) {
    TEST("empty object uses all defaults");
    write_file(TEST_CFG_PATH, "{}");

    SeaArena arena;
    sea_arena_create(&arena, 4096);
    SeaConfig cfg;

    SeaError err = sea_config_load(&cfg, TEST_CFG_PATH, &arena);
    if (err != SEA_OK) { FAIL("load failed"); goto cleanup; }
    if (cfg.telegram_token != NULL) { FAIL("token should be NULL"); goto cleanup; }
    if (cfg.telegram_chat_id != 0) { FAIL("chat_id should be 0"); goto cleanup; }
    if (strcmp(cfg.db_path, "seaclaw.db") != 0) { FAIL("default db_path"); goto cleanup; }
    if (strcmp(cfg.log_level, "info") != 0) { FAIL("default log_level"); goto cleanup; }
    if (cfg.arena_size_mb != 16) { FAIL("default arena_size"); goto cleanup; }
    PASS();

cleanup:
    sea_arena_destroy(&arena);
}

static void test_config_print(void) {
    TEST("config print does not crash");
    SeaConfig cfg = {
        .telegram_token = "secret",
        .telegram_chat_id = 12345,
        .db_path = "test.db",
        .log_level = "info",
        .arena_size_mb = 16,
        .loaded = true
    };
    /* Just verify it doesn't crash */
    sea_config_print(&cfg);
    PASS();
}

static void test_defaults_function(void) {
    TEST("sea_config_defaults fills zeroed struct");
    SeaConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    sea_config_defaults(&cfg);

    if (strcmp(cfg.db_path, "seaclaw.db") != 0) { FAIL("db_path"); return; }
    if (strcmp(cfg.log_level, "info") != 0) { FAIL("log_level"); return; }
    if (cfg.arena_size_mb != 16) { FAIL("arena_size"); return; }
    PASS();
}

int main(void) {
    sea_log_init(SEA_LOG_WARN);

    printf("\n  \033[1mSea-Claw Config Tests\033[0m\n");
    printf("  ════════════════════════════════════════════════\n\n");

    test_load_full_config();
    test_defaults_on_missing_file();
    test_partial_config();
    test_empty_object();
    test_config_print();
    test_defaults_function();

    printf("\n  ────────────────────────────────────────────────\n");
    printf("  Results: \033[32m%u passed\033[0m", s_pass);
    if (s_fail > 0) printf(", \033[31m%u failed\033[0m", s_fail);
    printf("\n\n");

    unlink(TEST_CFG_PATH);
    return s_fail > 0 ? 1 : 0;
}
