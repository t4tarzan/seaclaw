/*
 * test_usage.c — Tests for Usage Tracking
 *
 * Tests: sea_usage.h (init, record, provider lookup, daily stats, summary, total)
 */

#include "seaclaw/sea_types.h"
#include "seaclaw/sea_usage.h"
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

/* ── Tests ────────────────────────────────────────────────── */

static void test_usage_init(void) {
    TEST("usage_init_no_db");
    SeaUsageTracker t;
    SeaError err = sea_usage_init(&t, NULL);
    if (err != SEA_OK) { FAIL("init failed"); return; }
    if (t.provider_count != 0) { FAIL("providers not 0"); return; }
    if (t.total_requests != 0) { FAIL("requests not 0"); return; }
    PASS();
}

static void test_usage_init_null(void) {
    TEST("usage_init_null_returns_error");
    SeaError err = sea_usage_init(NULL, NULL);
    if (err == SEA_OK) { FAIL("should fail"); return; }
    PASS();
}

static void test_usage_record_single(void) {
    TEST("usage_record_single_request");
    SeaUsageTracker t;
    sea_usage_init(&t, NULL);
    sea_usage_record(&t, "anthropic", 500, 200, false);
    if (t.total_tokens_in != 500) { FAIL("wrong tokens_in"); return; }
    if (t.total_tokens_out != 200) { FAIL("wrong tokens_out"); return; }
    if (t.total_requests != 1) { FAIL("wrong requests"); return; }
    if (t.total_errors != 0) { FAIL("wrong errors"); return; }
    PASS();
}

static void test_usage_record_error(void) {
    TEST("usage_record_error_increments_errors");
    SeaUsageTracker t;
    sea_usage_init(&t, NULL);
    sea_usage_record(&t, "openai", 100, 0, true);
    if (t.total_errors != 1) { FAIL("errors not 1"); return; }
    PASS();
}

static void test_usage_record_multiple_providers(void) {
    TEST("usage_record_multiple_providers");
    SeaUsageTracker t;
    sea_usage_init(&t, NULL);
    sea_usage_record(&t, "anthropic", 500, 200, false);
    sea_usage_record(&t, "openai", 300, 100, false);
    sea_usage_record(&t, "anthropic", 400, 150, false);
    if (t.provider_count != 2) { FAIL("expected 2 providers"); return; }
    if (t.total_requests != 3) { FAIL("expected 3 requests"); return; }
    PASS();
}

static void test_usage_provider_lookup(void) {
    TEST("usage_provider_lookup");
    SeaUsageTracker t;
    sea_usage_init(&t, NULL);
    sea_usage_record(&t, "anthropic", 500, 200, false);
    sea_usage_record(&t, "anthropic", 300, 100, true);
    const SeaUsageProvider* p = sea_usage_provider(&t, "anthropic");
    if (!p) { FAIL("not found"); return; }
    if (p->tokens_in != 800) { FAIL("wrong tokens_in"); return; }
    if (p->tokens_out != 300) { FAIL("wrong tokens_out"); return; }
    if (p->requests != 2) { FAIL("wrong requests"); return; }
    if (p->errors != 1) { FAIL("wrong errors"); return; }
    PASS();
}

static void test_usage_provider_missing(void) {
    TEST("usage_provider_missing_returns_null");
    SeaUsageTracker t;
    sea_usage_init(&t, NULL);
    const SeaUsageProvider* p = sea_usage_provider(&t, "nonexistent");
    if (p != NULL) { FAIL("should be NULL"); return; }
    PASS();
}

static void test_usage_today(void) {
    TEST("usage_today_returns_stats");
    SeaUsageTracker t;
    sea_usage_init(&t, NULL);
    sea_usage_record(&t, "openai", 100, 50, false);
    const SeaUsageDay* d = sea_usage_today(&t);
    if (!d) { FAIL("today is NULL"); return; }
    if (d->tokens_in != 100) { FAIL("wrong tokens_in"); return; }
    if (d->requests != 1) { FAIL("wrong requests"); return; }
    PASS();
}

static void test_usage_total_tokens(void) {
    TEST("usage_total_tokens");
    SeaUsageTracker t;
    sea_usage_init(&t, NULL);
    sea_usage_record(&t, "anthropic", 500, 200, false);
    sea_usage_record(&t, "openai", 300, 100, false);
    u64 total = sea_usage_total_tokens(&t);
    if (total != 1100) { FAIL("expected 1100"); return; }
    PASS();
}

static void test_usage_total_tokens_null(void) {
    TEST("usage_total_tokens_null_returns_0");
    u64 total = sea_usage_total_tokens(NULL);
    if (total != 0) { FAIL("expected 0"); return; }
    PASS();
}

static void test_usage_summary(void) {
    TEST("usage_summary_formats_string");
    SeaUsageTracker t;
    sea_usage_init(&t, NULL);
    sea_usage_record(&t, "anthropic", 500, 200, false);
    sea_usage_record(&t, "openai", 300, 100, true);
    char buf[2048];
    u32 len = sea_usage_summary(&t, buf, sizeof(buf));
    if (len == 0) { FAIL("empty summary"); return; }
    if (strstr(buf, "Usage Summary") == NULL) { FAIL("missing header"); return; }
    if (strstr(buf, "anthropic") == NULL) { FAIL("missing anthropic"); return; }
    if (strstr(buf, "openai") == NULL) { FAIL("missing openai"); return; }
    PASS();
}

static void test_usage_summary_null(void) {
    TEST("usage_summary_null_returns_0");
    u32 len = sea_usage_summary(NULL, NULL, 0);
    if (len != 0) { FAIL("expected 0"); return; }
    PASS();
}

static void test_usage_load_no_db(void) {
    TEST("usage_load_no_db_returns_config_error");
    SeaUsageTracker t;
    sea_usage_init(&t, NULL);
    SeaError err = sea_usage_load(&t);
    if (err != SEA_ERR_CONFIG) { FAIL("expected CONFIG error"); return; }
    PASS();
}

static void test_usage_provider_max(void) {
    TEST("usage_max_providers_is_8");
    if (SEA_USAGE_PROVIDER_MAX != 8) { FAIL("wrong max"); return; }
    PASS();
}

static void test_usage_days_max(void) {
    TEST("usage_max_days_is_30");
    if (SEA_USAGE_DAYS_MAX != 30) { FAIL("wrong max"); return; }
    PASS();
}

/* ── Main ─────────────────────────────────────────────────── */

int main(void) {
    sea_log_init(SEA_LOG_WARN);

    printf("\n  \033[1mtest_usage\033[0m\n");

    test_usage_init();
    test_usage_init_null();
    test_usage_record_single();
    test_usage_record_error();
    test_usage_record_multiple_providers();
    test_usage_provider_lookup();
    test_usage_provider_missing();
    test_usage_today();
    test_usage_total_tokens();
    test_usage_total_tokens_null();
    test_usage_summary();
    test_usage_summary_null();
    test_usage_load_no_db();
    test_usage_provider_max();
    test_usage_days_max();

    printf("  Results: %u passed, %u failed\n\n", s_pass, s_fail);
    return s_fail > 0 ? 1 : 0;
}
