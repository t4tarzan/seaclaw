/*
 * test_auth.c — Tests for Auth Framework, Skill Install, and AGENT.md Discovery
 *
 * Tests: sea_auth.h, sea_skill.h (v2 install + agent discovery)
 */

#include "seaclaw/sea_types.h"
#include "seaclaw/sea_auth.h"
#include "seaclaw/sea_skill.h"
#include "seaclaw/sea_arena.h"
#include "seaclaw/sea_log.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

static u32 s_pass = 0;
static u32 s_fail = 0;

#define TEST(name) \
    do { printf("  %-44s ", name); } while(0)

#define PASS() \
    do { printf("\033[32mPASS\033[0m\n"); s_pass++; } while(0)

#define FAIL(msg) \
    do { printf("\033[31mFAIL\033[0m (%s)\n", msg); s_fail++; } while(0)

/* ── Auth Tests ───────────────────────────────────────────── */

static void test_auth_init(void) {
    TEST("auth_init_enabled");
    SeaAuth auth;
    sea_auth_init(&auth, true);
    if (!auth.enabled) { FAIL("not enabled"); return; }
    if (auth.count != 0) { FAIL("count not 0"); return; }
    PASS();
}

static void test_auth_init_disabled(void) {
    TEST("auth_init_disabled_grants_all");
    SeaAuth auth;
    sea_auth_init(&auth, false);
    u32 perms = sea_auth_validate(&auth, "anything");
    if (perms != SEA_PERM_ALL) { FAIL("should grant all when disabled"); return; }
    PASS();
}

static void test_auth_create_token(void) {
    TEST("auth_create_token");
    SeaAuth auth;
    sea_auth_init(&auth, true);

    char token[SEA_TOKEN_LEN + 1];
    SeaError err = sea_auth_create_token(&auth, "test-token",
                                          SEA_PERM_CHAT | SEA_PERM_TOOLS,
                                          0, token);
    if (err != SEA_OK) { FAIL("create failed"); return; }
    if (strlen(token) != SEA_TOKEN_LEN) { FAIL("wrong token length"); return; }
    if (auth.count != 1) { FAIL("count not 1"); return; }
    PASS();
}

static void test_auth_validate(void) {
    TEST("auth_validate_token");
    SeaAuth auth;
    sea_auth_init(&auth, true);

    char token[SEA_TOKEN_LEN + 1];
    sea_auth_create_token(&auth, "val-test",
                           SEA_PERM_CHAT | SEA_PERM_FILES, 0, token);

    u32 perms = sea_auth_validate(&auth, token);
    if (perms != (SEA_PERM_CHAT | SEA_PERM_FILES)) { FAIL("wrong perms"); return; }

    u32 bad = sea_auth_validate(&auth, "invalid_token_string");
    if (bad != 0) { FAIL("should return 0 for invalid"); return; }
    PASS();
}

static void test_auth_has_perm(void) {
    TEST("auth_has_perm");
    SeaAuth auth;
    sea_auth_init(&auth, true);

    char token[SEA_TOKEN_LEN + 1];
    sea_auth_create_token(&auth, "perm-test", SEA_PERM_CHAT | SEA_PERM_SHELL,
                           0, token);

    if (!sea_auth_has_perm(&auth, token, SEA_PERM_CHAT)) { FAIL("should have CHAT"); return; }
    if (!sea_auth_has_perm(&auth, token, SEA_PERM_SHELL)) { FAIL("should have SHELL"); return; }
    if (sea_auth_has_perm(&auth, token, SEA_PERM_ADMIN)) { FAIL("should NOT have ADMIN"); return; }
    PASS();
}

static void test_auth_revoke(void) {
    TEST("auth_revoke");
    SeaAuth auth;
    sea_auth_init(&auth, true);

    char token[SEA_TOKEN_LEN + 1];
    sea_auth_create_token(&auth, "revoke-test", SEA_PERM_ALL, 0, token);

    if (sea_auth_validate(&auth, token) == 0) { FAIL("should be valid before revoke"); return; }

    SeaError err = sea_auth_revoke(&auth, token);
    if (err != SEA_OK) { FAIL("revoke failed"); return; }

    if (sea_auth_validate(&auth, token) != 0) { FAIL("should be 0 after revoke"); return; }
    PASS();
}

static void test_auth_revoke_missing(void) {
    TEST("auth_revoke_missing");
    SeaAuth auth;
    sea_auth_init(&auth, true);

    SeaError err = sea_auth_revoke(&auth, "nonexistent");
    if (err != SEA_ERR_NOT_FOUND) { FAIL("should return NOT_FOUND"); return; }
    PASS();
}

static void test_auth_active_count(void) {
    TEST("auth_active_count");
    SeaAuth auth;
    sea_auth_init(&auth, true);

    char t1[SEA_TOKEN_LEN + 1], t2[SEA_TOKEN_LEN + 1];
    sea_auth_create_token(&auth, "a1", SEA_PERM_CHAT, 0, t1);
    sea_auth_create_token(&auth, "a2", SEA_PERM_CHAT, 0, t2);

    if (sea_auth_active_count(&auth) != 2) { FAIL("expected 2"); return; }

    sea_auth_revoke(&auth, t1);
    if (sea_auth_active_count(&auth) != 1) { FAIL("expected 1 after revoke"); return; }
    PASS();
}

static void test_auth_list_masks_tokens(void) {
    TEST("auth_list_masks_token_strings");
    SeaAuth auth;
    sea_auth_init(&auth, true);

    char token[SEA_TOKEN_LEN + 1];
    sea_auth_create_token(&auth, "list-test", SEA_PERM_CHAT, 0, token);

    SeaAuthToken out[4];
    u32 n = sea_auth_list(&auth, out, 4);
    if (n != 1) { FAIL("expected 1"); return; }

    /* First 8 chars should match, rest should be masked */
    if (strncmp(out[0].token, token, 8) != 0) { FAIL("first 8 chars wrong"); return; }
    if (out[0].token[8] != '*') { FAIL("char 9 should be masked"); return; }
    PASS();
}

/* ── Skill Install Tests ──────────────────────────────────── */

static const char* VALID_SKILL =
    "---\n"
    "name: test_install_skill\n"
    "description: A test skill for installation\n"
    "trigger: /testinstall\n"
    "---\n"
    "You are a test skill. Echo back the user's input.\n";

static void test_skill_install_content(void) {
    TEST("skill_install_content");

    /* Use a temp directory for skills */
    char tmpdir[] = "/tmp/seaclaw_test_skills_XXXXXX";
    if (!mkdtemp(tmpdir)) { FAIL("mkdtemp failed"); return; }

    SeaSkillRegistry reg;
    sea_skill_init(&reg, tmpdir);

    u32 len = (u32)strlen(VALID_SKILL);
    SeaError err = sea_skill_install_content(&reg, VALID_SKILL, len);
    if (err != SEA_OK) { FAIL("install failed"); sea_skill_destroy(&reg); return; }

    /* Should be findable */
    const SeaSkill* s = sea_skill_find(&reg, "test_install_skill");
    if (!s) { FAIL("not found after install"); sea_skill_destroy(&reg); return; }
    if (strcmp(s->trigger, "/testinstall") != 0) { FAIL("wrong trigger"); sea_skill_destroy(&reg); return; }

    /* File should exist on disk */
    char path[1024];
    snprintf(path, sizeof(path), "%s/test_install_skill.md", tmpdir);
    struct stat st;
    if (stat(path, &st) != 0) { FAIL("file not on disk"); sea_skill_destroy(&reg); return; }

    /* Cleanup */
    unlink(path);
    rmdir(tmpdir);
    sea_skill_destroy(&reg);
    PASS();
}

static void test_skill_install_duplicate(void) {
    TEST("skill_install_duplicate_rejected");

    char tmpdir[] = "/tmp/seaclaw_test_skills_XXXXXX";
    if (!mkdtemp(tmpdir)) { FAIL("mkdtemp failed"); return; }

    SeaSkillRegistry reg;
    sea_skill_init(&reg, tmpdir);

    u32 len = (u32)strlen(VALID_SKILL);
    sea_skill_install_content(&reg, VALID_SKILL, len);

    SeaError err = sea_skill_install_content(&reg, VALID_SKILL, len);
    if (err != SEA_ERR_ALREADY_EXISTS) { FAIL("should reject duplicate"); }
    else { PASS(); }

    /* Cleanup */
    char path[1024];
    snprintf(path, sizeof(path), "%s/test_install_skill.md", tmpdir);
    unlink(path);
    rmdir(tmpdir);
    sea_skill_destroy(&reg);
}

static void test_skill_install_invalid(void) {
    TEST("skill_install_invalid_format_rejected");

    char tmpdir[] = "/tmp/seaclaw_test_skills_XXXXXX";
    if (!mkdtemp(tmpdir)) { FAIL("mkdtemp failed"); return; }

    SeaSkillRegistry reg;
    sea_skill_init(&reg, tmpdir);

    const char* bad = "This is not a valid skill file.\nNo YAML frontmatter.";
    SeaError err = sea_skill_install_content(&reg, bad, (u32)strlen(bad));
    if (err == SEA_OK) { FAIL("should reject invalid format"); }
    else { PASS(); }

    rmdir(tmpdir);
    sea_skill_destroy(&reg);
}

/* ── AGENT.md Discovery Tests ─────────────────────────────── */

static void test_agent_discover(void) {
    TEST("agent_discover_finds_agent_md");

    /* Create a temp dir with an AGENT.md */
    char tmpdir[] = "/tmp/seaclaw_test_agent_XXXXXX";
    if (!mkdtemp(tmpdir)) { FAIL("mkdtemp failed"); return; }

    char agent_path[512];
    snprintf(agent_path, sizeof(agent_path), "%s/AGENT.md", tmpdir);
    FILE* f = fopen(agent_path, "w");
    if (!f) { FAIL("cannot create AGENT.md"); rmdir(tmpdir); return; }
    fprintf(f, "---\nname: test_agent\ndescription: Test agent\n---\nYou are a test agent.\n");
    fclose(f);

    SeaAgentMd agents[4];
    u32 count = sea_skill_discover_agents(tmpdir, agents, 4);
    if (count < 1) { FAIL("should find at least 1"); }
    else { PASS(); }

    unlink(agent_path);
    rmdir(tmpdir);
}

static void test_agent_discover_empty(void) {
    TEST("agent_discover_empty_dir");

    char tmpdir[] = "/tmp/seaclaw_test_empty_XXXXXX";
    if (!mkdtemp(tmpdir)) { FAIL("mkdtemp failed"); return; }

    SeaAgentMd agents[4];
    u32 count = sea_skill_discover_agents(tmpdir, agents, 4);
    if (count != 0) { FAIL("should find 0"); }
    else { PASS(); }

    rmdir(tmpdir);
}

/* ── Main ─────────────────────────────────────────────────── */

int main(void) {
    sea_log_init(SEA_LOG_WARN);

    printf("\n\033[1m  test_auth — Auth, Skill Install & AGENT.md Discovery\033[0m\n\n");

    /* Auth Framework */
    test_auth_init();
    test_auth_init_disabled();
    test_auth_create_token();
    test_auth_validate();
    test_auth_has_perm();
    test_auth_revoke();
    test_auth_revoke_missing();
    test_auth_active_count();
    test_auth_list_masks_tokens();

    /* Skill Install */
    test_skill_install_content();
    test_skill_install_duplicate();
    test_skill_install_invalid();

    /* AGENT.md Discovery */
    test_agent_discover();
    test_agent_discover_empty();

    printf("\n  ────────────────────────────────────────\n");
    printf("  \033[1mResults:\033[0m %u passed, %u failed\n\n", s_pass, s_fail);

    return s_fail > 0 ? 1 : 0;
}
