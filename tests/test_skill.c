/*
 * test_skill.c — Skills & Plugin System Tests
 *
 * Tests skill parsing, registration, lookup, file loading,
 * directory scanning, enable/disable, and prompt building.
 */

#include "seaclaw/sea_skill.h"
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

#define TEST_SKILLS_DIR "/tmp/test_seaclaw_skills"

static void cleanup(void) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", TEST_SKILLS_DIR);
    (void)system(cmd);
}

/* ── Test: Parse skill from markdown ──────────────────────── */

static void test_parse_basic(void) {
    TEST("parse_basic");
    const char* md =
        "---\n"
        "name: summarize\n"
        "description: Summarize text concisely\n"
        "trigger: /summarize\n"
        "---\n"
        "You are a summarization expert.\n"
        "Given text, produce a concise summary.\n";

    SeaSkill skill;
    SeaError err = sea_skill_parse(md, (u32)strlen(md), &skill);
    if (err != SEA_OK) { FAIL("parse failed"); return; }
    if (strcmp(skill.name, "summarize") != 0) { FAIL("wrong name"); return; }
    if (strcmp(skill.description, "Summarize text concisely") != 0) { FAIL("wrong desc"); return; }
    if (strcmp(skill.trigger, "/summarize") != 0) { FAIL("wrong trigger"); return; }
    if (strstr(skill.body, "summarization expert") == NULL) { FAIL("wrong body"); return; }
    if (!skill.enabled) { FAIL("not enabled"); return; }
    PASS();
}

/* ── Test: Parse with missing fields ──────────────────────── */

static void test_parse_minimal(void) {
    TEST("parse_minimal");
    const char* md =
        "---\n"
        "name: hello\n"
        "---\n"
        "Say hello to the user.\n";

    SeaSkill skill;
    SeaError err = sea_skill_parse(md, (u32)strlen(md), &skill);
    if (err != SEA_OK) { FAIL("parse failed"); return; }
    if (strcmp(skill.name, "hello") != 0) { FAIL("wrong name"); return; }
    if (skill.trigger[0] != '\0') { FAIL("trigger should be empty"); return; }
    if (strstr(skill.body, "Say hello") == NULL) { FAIL("wrong body"); return; }
    PASS();
}

/* ── Test: Parse invalid (no frontmatter) ─────────────────── */

static void test_parse_invalid(void) {
    TEST("parse_invalid_no_frontmatter");
    const char* md = "Just some text without frontmatter.\n";
    SeaSkill skill;
    SeaError err = sea_skill_parse(md, (u32)strlen(md), &skill);
    if (err == SEA_OK) { FAIL("should fail"); return; }
    PASS();
}

/* ── Test: Parse invalid (no name) ────────────────────────── */

static void test_parse_no_name(void) {
    TEST("parse_invalid_no_name");
    const char* md =
        "---\n"
        "description: No name field\n"
        "---\n"
        "Body text.\n";

    SeaSkill skill;
    SeaError err = sea_skill_parse(md, (u32)strlen(md), &skill);
    if (err == SEA_OK) { FAIL("should fail without name"); return; }
    PASS();
}

/* ── Test: Init and Destroy ───────────────────────────────── */

static void test_init_destroy(void) {
    TEST("init_destroy");
    cleanup();
    SeaSkillRegistry reg;
    SeaError err = sea_skill_init(&reg, TEST_SKILLS_DIR);
    if (err != SEA_OK) { FAIL("init failed"); return; }
    if (sea_skill_count(&reg) != 0) { FAIL("count != 0"); sea_skill_destroy(&reg); return; }

    struct stat st;
    if (stat(TEST_SKILLS_DIR, &st) != 0) { FAIL("dir not created"); sea_skill_destroy(&reg); return; }

    sea_skill_destroy(&reg);
    PASS();
}

/* ── Test: Register and Find ──────────────────────────────── */

static void test_register_find(void) {
    TEST("register_find");
    cleanup();
    SeaSkillRegistry reg;
    sea_skill_init(&reg, TEST_SKILLS_DIR);

    SeaSkill skill = {0};
    strncpy(skill.name, "translate", SEA_SKILL_NAME_MAX - 1);
    strncpy(skill.description, "Translate text", SEA_SKILL_DESC_MAX - 1);
    strncpy(skill.trigger, "/translate", SEA_SKILL_TRIGGER_MAX - 1);
    strncpy(skill.body, "Translate the following text.", SEA_SKILL_BODY_MAX - 1);
    skill.enabled = true;

    SeaError err = sea_skill_register(&reg, &skill);
    if (err != SEA_OK) { FAIL("register failed"); sea_skill_destroy(&reg); return; }
    if (sea_skill_count(&reg) != 1) { FAIL("count != 1"); sea_skill_destroy(&reg); return; }

    const SeaSkill* found = sea_skill_find(&reg, "translate");
    if (!found) { FAIL("find returned NULL"); sea_skill_destroy(&reg); return; }
    if (strcmp(found->name, "translate") != 0) { FAIL("wrong name"); sea_skill_destroy(&reg); return; }

    sea_skill_destroy(&reg);
    PASS();
}

/* ── Test: Find by Trigger ────────────────────────────────── */

static void test_find_by_trigger(void) {
    TEST("find_by_trigger");
    cleanup();
    SeaSkillRegistry reg;
    sea_skill_init(&reg, TEST_SKILLS_DIR);

    SeaSkill s1 = {0};
    strncpy(s1.name, "code", SEA_SKILL_NAME_MAX - 1);
    strncpy(s1.trigger, "/code", SEA_SKILL_TRIGGER_MAX - 1);
    strncpy(s1.body, "Write code.", SEA_SKILL_BODY_MAX - 1);
    s1.enabled = true;
    sea_skill_register(&reg, &s1);

    SeaSkill s2 = {0};
    strncpy(s2.name, "review", SEA_SKILL_NAME_MAX - 1);
    strncpy(s2.trigger, "/review", SEA_SKILL_TRIGGER_MAX - 1);
    strncpy(s2.body, "Review code.", SEA_SKILL_BODY_MAX - 1);
    s2.enabled = true;
    sea_skill_register(&reg, &s2);

    const SeaSkill* found = sea_skill_find_by_trigger(&reg, "/review");
    if (!found) { FAIL("not found"); sea_skill_destroy(&reg); return; }
    if (strcmp(found->name, "review") != 0) { FAIL("wrong skill"); sea_skill_destroy(&reg); return; }

    /* Non-existent trigger */
    if (sea_skill_find_by_trigger(&reg, "/nonexistent") != NULL) {
        FAIL("should be NULL"); sea_skill_destroy(&reg); return;
    }

    sea_skill_destroy(&reg);
    PASS();
}

/* ── Test: Load from File ─────────────────────────────────── */

static void test_load_file(void) {
    TEST("load_file");
    cleanup();
    mkdir(TEST_SKILLS_DIR, 0755);

    /* Write a skill file */
    char path[512];
    snprintf(path, sizeof(path), "%s/test_skill.md", TEST_SKILLS_DIR);
    FILE* f = fopen(path, "w");
    fprintf(f,
        "---\n"
        "name: greet\n"
        "description: Greet the user warmly\n"
        "trigger: /greet\n"
        "---\n"
        "You are a friendly greeter. Say hello warmly.\n");
    fclose(f);

    SeaSkillRegistry reg;
    sea_skill_init(&reg, TEST_SKILLS_DIR);

    SeaError err = sea_skill_load_file(&reg, path);
    if (err != SEA_OK) { FAIL("load failed"); sea_skill_destroy(&reg); return; }
    if (sea_skill_count(&reg) != 1) { FAIL("count != 1"); sea_skill_destroy(&reg); return; }

    const SeaSkill* s = sea_skill_find(&reg, "greet");
    if (!s) { FAIL("not found"); sea_skill_destroy(&reg); return; }
    if (strcmp(s->trigger, "/greet") != 0) { FAIL("wrong trigger"); sea_skill_destroy(&reg); return; }

    sea_skill_destroy(&reg);
    PASS();
}

/* ── Test: Load All from Directory ────────────────────────── */

static void test_load_all(void) {
    TEST("load_all_from_dir");
    cleanup();
    mkdir(TEST_SKILLS_DIR, 0755);

    /* Write multiple skill files */
    const char* skills[] = {
        "---\nname: skill_a\ntrigger: /a\n---\nDo A.\n",
        "---\nname: skill_b\ntrigger: /b\n---\nDo B.\n",
        "---\nname: skill_c\ntrigger: /c\n---\nDo C.\n",
    };
    for (int i = 0; i < 3; i++) {
        char path[512];
        snprintf(path, sizeof(path), "%s/skill_%c.md", TEST_SKILLS_DIR, 'a' + i);
        FILE* f = fopen(path, "w");
        fputs(skills[i], f);
        fclose(f);
    }

    /* Also write a non-.md file (should be ignored) */
    char txt_path[512];
    snprintf(txt_path, sizeof(txt_path), "%s/readme.txt", TEST_SKILLS_DIR);
    FILE* f = fopen(txt_path, "w");
    fputs("Not a skill.\n", f);
    fclose(f);

    SeaSkillRegistry reg;
    sea_skill_init(&reg, TEST_SKILLS_DIR);
    sea_skill_load_all(&reg);

    if (sea_skill_count(&reg) != 3) {
        char buf[64];
        snprintf(buf, sizeof(buf), "count=%u expected=3", sea_skill_count(&reg));
        FAIL(buf);
        sea_skill_destroy(&reg);
        return;
    }

    sea_skill_destroy(&reg);
    PASS();
}

/* ── Test: Enable/Disable ─────────────────────────────────── */

static void test_enable_disable(void) {
    TEST("enable_disable");
    cleanup();
    SeaSkillRegistry reg;
    sea_skill_init(&reg, TEST_SKILLS_DIR);

    SeaSkill skill = {0};
    strncpy(skill.name, "toggle", SEA_SKILL_NAME_MAX - 1);
    strncpy(skill.trigger, "/toggle", SEA_SKILL_TRIGGER_MAX - 1);
    strncpy(skill.body, "Toggle test.", SEA_SKILL_BODY_MAX - 1);
    skill.enabled = true;
    sea_skill_register(&reg, &skill);

    /* Disable */
    sea_skill_enable(&reg, "toggle", false);
    /* Disabled skill should NOT be found by trigger */
    if (sea_skill_find_by_trigger(&reg, "/toggle") != NULL) {
        FAIL("disabled skill found by trigger");
        sea_skill_destroy(&reg);
        return;
    }
    /* But should still be found by name */
    if (sea_skill_find(&reg, "toggle") == NULL) {
        FAIL("disabled skill not found by name");
        sea_skill_destroy(&reg);
        return;
    }

    /* Re-enable */
    sea_skill_enable(&reg, "toggle", true);
    if (sea_skill_find_by_trigger(&reg, "/toggle") == NULL) {
        FAIL("re-enabled skill not found");
        sea_skill_destroy(&reg);
        return;
    }

    sea_skill_destroy(&reg);
    PASS();
}

/* ── Test: List Skills ────────────────────────────────────── */

static void test_list(void) {
    TEST("list_skills");
    cleanup();
    SeaSkillRegistry reg;
    sea_skill_init(&reg, TEST_SKILLS_DIR);

    SeaSkill s1 = {0}; strncpy(s1.name, "alpha", SEA_SKILL_NAME_MAX - 1); s1.enabled = true;
    SeaSkill s2 = {0}; strncpy(s2.name, "beta", SEA_SKILL_NAME_MAX - 1); s2.enabled = true;
    sea_skill_register(&reg, &s1);
    sea_skill_register(&reg, &s2);

    const char* names[10];
    u32 count = sea_skill_list(&reg, names, 10);
    if (count != 2) { FAIL("count != 2"); sea_skill_destroy(&reg); return; }
    if (strcmp(names[0], "alpha") != 0) { FAIL("wrong names[0]"); sea_skill_destroy(&reg); return; }

    sea_skill_destroy(&reg);
    PASS();
}

/* ── Test: Build Prompt ───────────────────────────────────── */

static void test_build_prompt(void) {
    TEST("build_prompt");
    SeaSkill skill = {0};
    strncpy(skill.body, "Summarize the following:", SEA_SKILL_BODY_MAX - 1);

    SeaArena arena;
    sea_arena_create(&arena, 8192);

    const char* prompt = sea_skill_build_prompt(&skill, "Hello world", &arena);
    if (!prompt) { FAIL("null prompt"); sea_arena_destroy(&arena); return; }
    if (strstr(prompt, "Summarize the following:") == NULL) { FAIL("missing body"); sea_arena_destroy(&arena); return; }
    if (strstr(prompt, "Hello world") == NULL) { FAIL("missing input"); sea_arena_destroy(&arena); return; }

    /* Without user input */
    const char* prompt2 = sea_skill_build_prompt(&skill, NULL, &arena);
    if (!prompt2) { FAIL("null prompt2"); sea_arena_destroy(&arena); return; }
    if (strstr(prompt2, "User input") != NULL) { FAIL("should not have user input section"); sea_arena_destroy(&arena); return; }

    sea_arena_destroy(&arena);
    PASS();
}

/* ── Main ─────────────────────────────────────────────────── */

int main(void) {
    sea_log_init(SEA_LOG_WARN);

    printf("\n\033[1m=== Sea-Claw Skill Tests ===\033[0m\n\n");

    test_parse_basic();
    test_parse_minimal();
    test_parse_invalid();
    test_parse_no_name();
    test_init_destroy();
    test_register_find();
    test_find_by_trigger();
    test_load_file();
    test_load_all();
    test_enable_disable();
    test_list();
    test_build_prompt();

    cleanup();

    printf("\n\033[1mResults: %d passed, %d failed\033[0m\n\n", s_pass, s_fail);
    return s_fail > 0 ? 1 : 0;
}
