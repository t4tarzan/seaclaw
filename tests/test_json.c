/*
 * test_json.c — Zero-copy JSON parser tests
 *
 * Correctness + benchmark: 1KB parse < 0.1ms target.
 */

#include "seaclaw/sea_types.h"
#include "seaclaw/sea_arena.h"
#include "seaclaw/sea_json.h"
#include "seaclaw/sea_log.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

#define TEST_ARENA_SIZE (4 * 1024 * 1024)

static u32 s_pass = 0;
static u32 s_fail = 0;

#define TEST(name) \
    do { printf("  %-44s ", name); } while(0)

#define PASS() \
    do { printf("\033[32mPASS\033[0m\n"); s_pass++; } while(0)

#define FAIL(msg) \
    do { printf("\033[31mFAIL\033[0m (%s)\n", msg); s_fail++; } while(0)

static SeaArena arena;

static void reset(void) { sea_arena_reset(&arena); }

/* ── Tests ────────────────────────────────────────────────── */

static void test_parse_null(void) {
    TEST("parse null");
    reset();
    SeaJsonValue val;
    SeaSlice input = SEA_SLICE_LIT("null");
    SeaError err = sea_json_parse(input, &arena, &val);
    if (err != SEA_OK) { FAIL("parse error"); return; }
    if (val.type != SEA_JSON_NULL) { FAIL("wrong type"); return; }
    PASS();
}

static void test_parse_true(void) {
    TEST("parse true");
    reset();
    SeaJsonValue val;
    SeaSlice input = SEA_SLICE_LIT("true");
    SeaError err = sea_json_parse(input, &arena, &val);
    if (err != SEA_OK) { FAIL("parse error"); return; }
    if (val.type != SEA_JSON_BOOL || !val.boolean) { FAIL("wrong value"); return; }
    PASS();
}

static void test_parse_false(void) {
    TEST("parse false");
    reset();
    SeaJsonValue val;
    SeaSlice input = SEA_SLICE_LIT("false");
    SeaError err = sea_json_parse(input, &arena, &val);
    if (err != SEA_OK) { FAIL("parse error"); return; }
    if (val.type != SEA_JSON_BOOL || val.boolean) { FAIL("wrong value"); return; }
    PASS();
}

static void test_parse_integer(void) {
    TEST("parse integer");
    reset();
    SeaJsonValue val;
    SeaSlice input = SEA_SLICE_LIT("42");
    SeaError err = sea_json_parse(input, &arena, &val);
    if (err != SEA_OK) { FAIL("parse error"); return; }
    if (val.type != SEA_JSON_NUMBER) { FAIL("wrong type"); return; }
    if (fabs(val.number - 42.0) > 0.001) { FAIL("wrong value"); return; }
    PASS();
}

static void test_parse_negative(void) {
    TEST("parse negative number");
    reset();
    SeaJsonValue val;
    SeaSlice input = SEA_SLICE_LIT("-3.14");
    SeaError err = sea_json_parse(input, &arena, &val);
    if (err != SEA_OK) { FAIL("parse error"); return; }
    if (fabs(val.number - (-3.14)) > 0.001) { FAIL("wrong value"); return; }
    PASS();
}

static void test_parse_string(void) {
    TEST("parse string");
    reset();
    SeaJsonValue val;
    SeaSlice input = SEA_SLICE_LIT("\"Hello, Vault!\"");
    SeaError err = sea_json_parse(input, &arena, &val);
    if (err != SEA_OK) { FAIL("parse error"); return; }
    if (val.type != SEA_JSON_STRING) { FAIL("wrong type"); return; }
    if (!sea_slice_eq_cstr(val.string, "Hello, Vault!")) { FAIL("wrong content"); return; }
    PASS();
}

static void test_parse_empty_string(void) {
    TEST("parse empty string");
    reset();
    SeaJsonValue val;
    SeaSlice input = SEA_SLICE_LIT("\"\"");
    SeaError err = sea_json_parse(input, &arena, &val);
    if (err != SEA_OK) { FAIL("parse error"); return; }
    if (val.string.len != 0) { FAIL("not empty"); return; }
    PASS();
}

static void test_parse_escaped_string(void) {
    TEST("parse escaped string");
    reset();
    SeaJsonValue val;
    SeaSlice input = SEA_SLICE_LIT("\"line1\\nline2\"");
    SeaError err = sea_json_parse(input, &arena, &val);
    if (err != SEA_OK) { FAIL("parse error"); return; }
    if (val.type != SEA_JSON_STRING) { FAIL("wrong type"); return; }
    PASS();
}

static void test_parse_empty_array(void) {
    TEST("parse empty array");
    reset();
    SeaJsonValue val;
    SeaSlice input = SEA_SLICE_LIT("[]");
    SeaError err = sea_json_parse(input, &arena, &val);
    if (err != SEA_OK) { FAIL("parse error"); return; }
    if (val.type != SEA_JSON_ARRAY) { FAIL("wrong type"); return; }
    if (val.array.count != 0) { FAIL("not empty"); return; }
    PASS();
}

static void test_parse_array(void) {
    TEST("parse array [1, \"two\", true]");
    reset();
    SeaJsonValue val;
    SeaSlice input = SEA_SLICE_LIT("[1, \"two\", true]");
    SeaError err = sea_json_parse(input, &arena, &val);
    if (err != SEA_OK) { FAIL("parse error"); return; }
    if (val.type != SEA_JSON_ARRAY) { FAIL("wrong type"); return; }
    if (val.array.count != 3) { FAIL("wrong count"); return; }
    if (val.array.items[0].type != SEA_JSON_NUMBER) { FAIL("item 0 not number"); return; }
    if (val.array.items[1].type != SEA_JSON_STRING) { FAIL("item 1 not string"); return; }
    if (val.array.items[2].type != SEA_JSON_BOOL) { FAIL("item 2 not bool"); return; }
    PASS();
}

static void test_parse_empty_object(void) {
    TEST("parse empty object");
    reset();
    SeaJsonValue val;
    SeaSlice input = SEA_SLICE_LIT("{}");
    SeaError err = sea_json_parse(input, &arena, &val);
    if (err != SEA_OK) { FAIL("parse error"); return; }
    if (val.type != SEA_JSON_OBJECT) { FAIL("wrong type"); return; }
    if (val.object.count != 0) { FAIL("not empty"); return; }
    PASS();
}

static void test_parse_object(void) {
    TEST("parse object with mixed values");
    reset();
    SeaJsonValue val;
    SeaSlice input = SEA_SLICE_LIT(
        "{\"name\": \"Acme Corp\", \"amount\": 500.00, \"paid\": false}"
    );
    SeaError err = sea_json_parse(input, &arena, &val);
    if (err != SEA_OK) { FAIL("parse error"); return; }
    if (val.type != SEA_JSON_OBJECT) { FAIL("wrong type"); return; }
    if (val.object.count != 3) { FAIL("wrong count"); return; }

    SeaSlice name = sea_json_get_string(&val, "name");
    if (!sea_slice_eq_cstr(name, "Acme Corp")) { FAIL("wrong name"); return; }

    f64 amount = sea_json_get_number(&val, "amount", 0);
    if (fabs(amount - 500.0) > 0.001) { FAIL("wrong amount"); return; }

    bool paid = sea_json_get_bool(&val, "paid", true);
    if (paid != false) { FAIL("wrong paid"); return; }

    PASS();
}

static void test_nested_object(void) {
    TEST("parse nested object");
    reset();
    SeaJsonValue val;
    SeaSlice input = SEA_SLICE_LIT(
        "{\"user\": {\"id\": 42, \"name\": \"Dev\"}, \"active\": true}"
    );
    SeaError err = sea_json_parse(input, &arena, &val);
    if (err != SEA_OK) { FAIL("parse error"); return; }

    const SeaJsonValue* user = sea_json_get(&val, "user");
    if (!user || user->type != SEA_JSON_OBJECT) { FAIL("no user object"); return; }

    f64 id = sea_json_get_number(user, "id", 0);
    if (fabs(id - 42.0) > 0.001) { FAIL("wrong id"); return; }

    SeaSlice name = sea_json_get_string(user, "name");
    if (!sea_slice_eq_cstr(name, "Dev")) { FAIL("wrong name"); return; }

    PASS();
}

static void test_get_missing_key(void) {
    TEST("get missing key returns NULL/fallback");
    reset();
    SeaJsonValue val;
    SeaSlice input = SEA_SLICE_LIT("{\"a\": 1}");
    sea_json_parse(input, &arena, &val);

    const SeaJsonValue* v = sea_json_get(&val, "nonexistent");
    if (v != NULL) { FAIL("should be NULL"); return; }

    f64 n = sea_json_get_number(&val, "nonexistent", -999.0);
    if (fabs(n - (-999.0)) > 0.001) { FAIL("wrong fallback"); return; }

    PASS();
}

static void test_reject_invalid(void) {
    TEST("reject invalid JSON");
    reset();
    SeaJsonValue val;

    SeaSlice bad1 = SEA_SLICE_LIT("{broken");
    if (sea_json_parse(bad1, &arena, &val) == SEA_OK) { FAIL("should reject"); return; }

    reset();
    SeaSlice bad2 = SEA_SLICE_LIT("\"unterminated");
    if (sea_json_parse(bad2, &arena, &val) == SEA_OK) { FAIL("should reject"); return; }

    reset();
    SeaSlice bad3 = SEA_SLICE_LIT("[1, 2,]");
    if (sea_json_parse(bad3, &arena, &val) == SEA_OK) { FAIL("should reject trailing comma"); return; }

    PASS();
}

static void test_telegram_message(void) {
    TEST("parse Telegram-style message JSON");
    reset();
    SeaJsonValue val;
    SeaSlice input = SEA_SLICE_LIT(
        "{\"update_id\": 123456789,"
        " \"message\": {"
        "   \"message_id\": 42,"
        "   \"from\": {\"id\": 987654, \"first_name\": \"Dev\"},"
        "   \"chat\": {\"id\": -100123, \"type\": \"private\"},"
        "   \"text\": \"/status\""
        " }}"
    );
    SeaError err = sea_json_parse(input, &arena, &val);
    if (err != SEA_OK) { FAIL("parse error"); return; }

    const SeaJsonValue* msg = sea_json_get(&val, "message");
    if (!msg) { FAIL("no message"); return; }

    SeaSlice text = sea_json_get_string(msg, "text");
    if (!sea_slice_eq_cstr(text, "/status")) { FAIL("wrong text"); return; }

    const SeaJsonValue* from = sea_json_get(msg, "from");
    if (!from) { FAIL("no from"); return; }

    SeaSlice fname = sea_json_get_string(from, "first_name");
    if (!sea_slice_eq_cstr(fname, "Dev")) { FAIL("wrong first_name"); return; }

    PASS();
}

static void test_benchmark_1kb(void) {
    TEST("benchmark: 10K parses of ~200B object");

    /* Build a realistic JSON payload */
    const char* json =
        "{\"cmd\": \"invoice_gen\","
        " \"client\": \"Acme Corp\","
        " \"amount\": 500.00,"
        " \"currency\": \"USD\","
        " \"items\": [\"Widget A\", \"Widget B\", \"Service C\"],"
        " \"paid\": false,"
        " \"notes\": null}";

    SeaSlice input = { .data = (const u8*)json, .len = (u32)strlen(json) };

    u64 t0 = sea_log_elapsed_ms();

    for (u32 i = 0; i < 10000; i++) {
        reset();
        SeaJsonValue val;
        SeaError err = sea_json_parse(input, &arena, &val);
        if (err != SEA_OK) { FAIL("parse failed during benchmark"); return; }
    }

    u64 t1 = sea_log_elapsed_ms();
    u64 elapsed = t1 - t0;
    f64 per_parse_us = (f64)elapsed * 1000.0 / 10000.0;

    if (per_parse_us > 100.0) {
        char msg[64];
        snprintf(msg, sizeof(msg), "too slow: %.1f us/parse (target <100us)", per_parse_us);
        FAIL(msg);
        return;
    }

    printf("\033[32mPASS\033[0m (%lums total, %.1f us/parse)\n",
           (unsigned long)elapsed, per_parse_us);
    s_pass++;
}

/* ── Main ─────────────────────────────────────────────────── */

int main(void) {
    sea_log_init(SEA_LOG_WARN);
    sea_arena_create(&arena, TEST_ARENA_SIZE);

    printf("\n  \033[1mSea-Claw JSON Parser Tests\033[0m\n");
    printf("  ════════════════════════════════════════════════\n\n");

    test_parse_null();
    test_parse_true();
    test_parse_false();
    test_parse_integer();
    test_parse_negative();
    test_parse_string();
    test_parse_empty_string();
    test_parse_escaped_string();
    test_parse_empty_array();
    test_parse_array();
    test_parse_empty_object();
    test_parse_object();
    test_nested_object();
    test_get_missing_key();
    test_reject_invalid();
    test_telegram_message();
    test_benchmark_1kb();

    printf("\n  ────────────────────────────────────────────────\n");
    printf("  Results: \033[32m%u passed\033[0m", s_pass);
    if (s_fail > 0) printf(", \033[31m%u failed\033[0m", s_fail);
    printf("\n\n");

    sea_arena_destroy(&arena);
    return s_fail > 0 ? 1 : 0;
}
