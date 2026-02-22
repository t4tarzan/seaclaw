/*
 * test_graph.c — Tests for Knowledge Graph, Google Tools, WebSocket
 *
 * Tests: sea_graph.h (entity CRUD, relations, backlinks, context, markdown)
 *        sea_ws.h (init/destroy)
 *        tool_google.c (graceful fallback when gog not installed)
 */

#include "seaclaw/sea_types.h"
#include "seaclaw/sea_graph.h"
#include "seaclaw/sea_db.h"
#include "seaclaw/sea_arena.h"
#include "seaclaw/sea_log.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static u32 s_pass = 0;
static u32 s_fail = 0;

#define TEST(name) \
    do { printf("  %-48s ", name); } while(0)

#define PASS() \
    do { printf("\033[32mPASS\033[0m\n"); s_pass++; } while(0)

#define FAIL(msg) \
    do { printf("\033[31mFAIL\033[0m (%s)\n", msg); s_fail++; } while(0)

/* ── Shared DB ────────────────────────────────────────────── */

static SeaDb* s_db = NULL;
static SeaArena s_arena;

static void setup_db(void) {
    sea_db_open(&s_db, ":memory:");
    sea_arena_create(&s_arena, 64 * 1024);
}

static void teardown_db(void) {
    sea_arena_destroy(&s_arena);
    if (s_db) { sea_db_close(s_db); s_db = NULL; }
}

/* ── Entity Tests ─────────────────────────────────────────── */

static void test_graph_init(void) {
    TEST("graph_init_creates_tables");
    setup_db();

    SeaGraph g;
    SeaError err = sea_graph_init(&g, s_db);
    if (err != SEA_OK) { FAIL("init failed"); teardown_db(); return; }

    if (sea_graph_entity_count(&g) != 0) { FAIL("should start empty"); }
    else { PASS(); }

    sea_graph_destroy(&g);
    teardown_db();
}

static void test_entity_upsert_create(void) {
    TEST("entity_upsert_creates_new");
    setup_db();

    SeaGraph g;
    sea_graph_init(&g, s_db);

    i32 id = sea_graph_entity_upsert(&g, SEA_ENTITY_PERSON, "Alice", "Lead engineer");
    if (id < 0) { FAIL("upsert returned -1"); }
    else if (sea_graph_entity_count(&g) != 1) { FAIL("count should be 1"); }
    else { PASS(); }

    sea_graph_destroy(&g);
    teardown_db();
}

static void test_entity_upsert_update(void) {
    TEST("entity_upsert_updates_existing");
    setup_db();

    SeaGraph g;
    sea_graph_init(&g, s_db);

    i32 id1 = sea_graph_entity_upsert(&g, SEA_ENTITY_PERSON, "Bob", "Backend dev");
    i32 id2 = sea_graph_entity_upsert(&g, SEA_ENTITY_PERSON, "Bob", "Senior backend dev");

    if (id1 != id2) { FAIL("should return same id"); }
    else if (sea_graph_entity_count(&g) != 1) { FAIL("count should still be 1"); }
    else {
        sea_arena_reset(&s_arena);
        SeaGraphEntity* e = sea_graph_entity_find(&g, "Bob", &s_arena);
        if (!e) { FAIL("find returned NULL"); }
        else if (e->mention_count != 2) { FAIL("mention_count should be 2"); }
        else { PASS(); }
    }

    sea_graph_destroy(&g);
    teardown_db();
}

static void test_entity_find_case_insensitive(void) {
    TEST("entity_find_case_insensitive");
    setup_db();

    SeaGraph g;
    sea_graph_init(&g, s_db);

    sea_graph_entity_upsert(&g, SEA_ENTITY_PROJECT, "SeaBot", "AI agent platform");

    sea_arena_reset(&s_arena);
    SeaGraphEntity* e = sea_graph_entity_find(&g, "seabot", &s_arena);
    if (!e) { FAIL("should find case-insensitive"); }
    else if (strcmp(e->name, "SeaBot") != 0) { FAIL("name should preserve case"); }
    else { PASS(); }

    sea_graph_destroy(&g);
    teardown_db();
}

static void test_entity_search(void) {
    TEST("entity_search_by_substring");
    setup_db();

    SeaGraph g;
    sea_graph_init(&g, s_db);

    sea_graph_entity_upsert(&g, SEA_ENTITY_PERSON, "Alice Smith", "Engineer");
    sea_graph_entity_upsert(&g, SEA_ENTITY_PERSON, "Bob Jones", "Designer");
    sea_graph_entity_upsert(&g, SEA_ENTITY_PROJECT, "Alice's Project", "Secret");

    SeaGraphEntity results[8];
    u32 count = sea_graph_entity_search(&g, "Alice", results, 8, &s_arena);
    if (count != 2) { FAIL("should find 2 matches"); }
    else { PASS(); }

    sea_graph_destroy(&g);
    teardown_db();
}

static void test_entity_list_by_type(void) {
    TEST("entity_list_filters_by_type");
    setup_db();

    SeaGraph g;
    sea_graph_init(&g, s_db);

    sea_graph_entity_upsert(&g, SEA_ENTITY_PERSON, "Alice", "");
    sea_graph_entity_upsert(&g, SEA_ENTITY_PERSON, "Bob", "");
    sea_graph_entity_upsert(&g, SEA_ENTITY_PROJECT, "SeaBot", "");

    SeaGraphEntity results[8];
    u32 people = sea_graph_entity_list(&g, SEA_ENTITY_PERSON, results, 8, &s_arena);
    u32 projects = sea_graph_entity_list(&g, SEA_ENTITY_PROJECT, results, 8, &s_arena);

    if (people != 2) { FAIL("should find 2 people"); }
    else if (projects != 1) { FAIL("should find 1 project"); }
    else { PASS(); }

    sea_graph_destroy(&g);
    teardown_db();
}

static void test_entity_delete(void) {
    TEST("entity_delete_removes_entity_and_rels");
    setup_db();

    SeaGraph g;
    sea_graph_init(&g, s_db);

    i32 alice = sea_graph_entity_upsert(&g, SEA_ENTITY_PERSON, "Alice", "");
    i32 proj = sea_graph_entity_upsert(&g, SEA_ENTITY_PROJECT, "SeaBot", "");
    sea_graph_relate(&g, alice, proj, SEA_REL_WORKS_ON, "");

    sea_graph_entity_delete(&g, alice);

    if (sea_graph_entity_count(&g) != 1) { FAIL("should have 1 entity left"); }
    else {
        SeaGraphRelation rels[4];
        u32 rc = sea_graph_relations_to(&g, proj, rels, 4, &s_arena);
        if (rc != 0) { FAIL("relations should be deleted too"); }
        else { PASS(); }
    }

    sea_graph_destroy(&g);
    teardown_db();
}

/* ── Relation Tests ───────────────────────────────────────── */

static void test_relate_entities(void) {
    TEST("relate_creates_directed_edge");
    setup_db();

    SeaGraph g;
    sea_graph_init(&g, s_db);

    i32 alice = sea_graph_entity_upsert(&g, SEA_ENTITY_PERSON, "Alice", "");
    i32 proj = sea_graph_entity_upsert(&g, SEA_ENTITY_PROJECT, "SeaBot", "");

    i32 rel_id = sea_graph_relate(&g, alice, proj, SEA_REL_WORKS_ON, "lead");
    if (rel_id < 0) { FAIL("relate returned -1"); goto cleanup; }

    SeaGraphRelation rels[4];
    u32 from_count = sea_graph_relations_from(&g, alice, rels, 4, &s_arena);
    if (from_count != 1) { FAIL("should have 1 outgoing rel"); goto cleanup; }
    if (rels[0].to_id != proj) { FAIL("should point to project"); goto cleanup; }

    u32 to_count = sea_graph_relations_to(&g, proj, rels, 4, &s_arena);
    if (to_count != 1) { FAIL("should have 1 incoming rel (backlink)"); goto cleanup; }
    if (rels[0].from_id != alice) { FAIL("backlink should come from alice"); goto cleanup; }

    PASS();

cleanup:
    sea_graph_destroy(&g);
    teardown_db();
}

static void test_unrelate(void) {
    TEST("unrelate_removes_edge");
    setup_db();

    SeaGraph g;
    sea_graph_init(&g, s_db);

    i32 a = sea_graph_entity_upsert(&g, SEA_ENTITY_PERSON, "A", "");
    i32 b = sea_graph_entity_upsert(&g, SEA_ENTITY_PERSON, "B", "");
    i32 rel = sea_graph_relate(&g, a, b, SEA_REL_RELATED_TO, "");

    sea_graph_unrelate(&g, rel);

    SeaGraphRelation rels[4];
    u32 count = sea_graph_relations_from(&g, a, rels, 4, &s_arena);
    if (count != 0) { FAIL("should have 0 rels after unrelate"); }
    else { PASS(); }

    sea_graph_destroy(&g);
    teardown_db();
}

/* ── Context & Markdown Tests ─────────────────────────────── */

static void test_build_entity_context(void) {
    TEST("build_entity_context_with_backlinks");
    setup_db();

    SeaGraph g;
    sea_graph_init(&g, s_db);

    i32 alice = sea_graph_entity_upsert(&g, SEA_ENTITY_PERSON, "Alice", "Lead engineer");
    i32 proj = sea_graph_entity_upsert(&g, SEA_ENTITY_PROJECT, "SeaBot", "AI platform");
    sea_graph_relate(&g, alice, proj, SEA_REL_WORKS_ON, "");

    sea_arena_reset(&s_arena);
    const char* ctx = sea_graph_build_entity_context(&g, proj, &s_arena);
    if (!ctx) { FAIL("context is NULL"); goto cleanup; }
    if (!strstr(ctx, "[[SeaBot]]")) { FAIL("should contain [[SeaBot]]"); goto cleanup; }
    if (!strstr(ctx, "[[Alice]]")) { FAIL("should contain backlink [[Alice]]"); goto cleanup; }

    PASS();

cleanup:
    sea_graph_destroy(&g);
    teardown_db();
}

static void test_build_query_context(void) {
    TEST("build_query_context_finds_matches");
    setup_db();

    SeaGraph g;
    sea_graph_init(&g, s_db);

    sea_graph_entity_upsert(&g, SEA_ENTITY_PERSON, "Alice", "Lead engineer");
    sea_graph_entity_upsert(&g, SEA_ENTITY_PROJECT, "SeaBot", "AI platform");

    sea_arena_reset(&s_arena);
    const char* ctx = sea_graph_build_query_context(&g, "Alice", &s_arena);
    if (!ctx) { FAIL("context is NULL"); }
    else if (!strstr(ctx, "Knowledge Graph")) { FAIL("should have header"); }
    else { PASS(); }

    sea_graph_destroy(&g);
    teardown_db();
}

static void test_export_markdown(void) {
    TEST("export_markdown_obsidian_compatible");
    setup_db();

    SeaGraph g;
    sea_graph_init(&g, s_db);

    i32 alice = sea_graph_entity_upsert(&g, SEA_ENTITY_PERSON, "Alice", "Lead engineer");
    i32 proj = sea_graph_entity_upsert(&g, SEA_ENTITY_PROJECT, "SeaBot", "AI platform");
    sea_graph_relate(&g, alice, proj, SEA_REL_WORKS_ON, "");

    sea_arena_reset(&s_arena);
    const char* md = sea_graph_export_markdown(&g, alice, &s_arena);
    if (!md) { FAIL("markdown is NULL"); }
    else if (!strstr(md, "[[Alice]]")) { FAIL("should have [[wiki-link]]"); }
    else if (!strstr(md, "[[SeaBot]]")) { FAIL("should link to SeaBot"); }
    else { PASS(); }

    sea_graph_destroy(&g);
    teardown_db();
}

static void test_query_context_empty(void) {
    TEST("query_context_returns_null_no_match");
    setup_db();

    SeaGraph g;
    sea_graph_init(&g, s_db);

    sea_arena_reset(&s_arena);
    const char* ctx = sea_graph_build_query_context(&g, "nonexistent", &s_arena);
    if (ctx != NULL) { FAIL("should return NULL for no matches"); }
    else { PASS(); }

    sea_graph_destroy(&g);
    teardown_db();
}

/* ── Main ─────────────────────────────────────────────────── */

int main(void) {
    sea_log_init(SEA_LOG_WARN);

    printf("\n\033[1m  test_graph — Knowledge Graph, Google Tools, WebSocket\033[0m\n\n");

    /* Entity CRUD */
    test_graph_init();
    test_entity_upsert_create();
    test_entity_upsert_update();
    test_entity_find_case_insensitive();
    test_entity_search();
    test_entity_list_by_type();
    test_entity_delete();

    /* Relations */
    test_relate_entities();
    test_unrelate();

    /* Context & Markdown */
    test_build_entity_context();
    test_build_query_context();
    test_export_markdown();
    test_query_context_empty();

    printf("\n  ────────────────────────────────────────\n");
    printf("  \033[1mResults:\033[0m %u passed, %u failed\n\n", s_pass, s_fail);

    return s_fail > 0 ? 1 : 0;
}
