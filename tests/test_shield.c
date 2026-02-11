/*
 * test_shield.c — Grammar filter tests
 *
 * Fuzz test: throw garbage at the Shield, prove it blocks everything.
 */

#include "seaclaw/sea_types.h"
#include "seaclaw/sea_shield.h"
#include "seaclaw/sea_log.h"

#include <stdio.h>
#include <string.h>

static u32 s_pass = 0;
static u32 s_fail = 0;

#define TEST(name) \
    do { printf("  %-44s ", name); } while(0)

#define PASS() \
    do { printf("\033[32mPASS\033[0m\n"); s_pass++; } while(0)

#define FAIL(msg) \
    do { printf("\033[31mFAIL\033[0m (%s)\n", msg); s_fail++; } while(0)

/* ── Tests ────────────────────────────────────────────────── */

static void test_safe_text_allows_normal(void) {
    TEST("SAFE_TEXT allows normal text");
    SeaSlice input = SEA_SLICE_LIT("Hello, World! This is a test 123.");
    if (!sea_shield_check(input, SEA_GRAMMAR_SAFE_TEXT)) { FAIL("rejected normal text"); return; }
    PASS();
}

static void test_safe_text_blocks_null(void) {
    TEST("SAFE_TEXT blocks null bytes");
    u8 data[] = {'H', 'i', 0x00, '!'};
    SeaSlice input = { .data = data, .len = 4 };
    if (sea_shield_check(input, SEA_GRAMMAR_SAFE_TEXT)) { FAIL("allowed null byte"); return; }
    PASS();
}

static void test_safe_text_blocks_control(void) {
    TEST("SAFE_TEXT blocks control chars (0x01-0x08)");
    u8 data[] = {'H', 'i', 0x07, '!'};  /* BEL character */
    SeaSlice input = { .data = data, .len = 4 };
    if (sea_shield_check(input, SEA_GRAMMAR_SAFE_TEXT)) { FAIL("allowed control char"); return; }
    PASS();
}

static void test_numeric_allows_numbers(void) {
    TEST("NUMERIC allows valid numbers");
    SeaSlice n1 = SEA_SLICE_LIT("42");
    SeaSlice n2 = SEA_SLICE_LIT("-3.14");
    SeaSlice n3 = SEA_SLICE_LIT("1.5e10");
    if (!sea_shield_check(n1, SEA_GRAMMAR_NUMERIC)) { FAIL("rejected 42"); return; }
    if (!sea_shield_check(n2, SEA_GRAMMAR_NUMERIC)) { FAIL("rejected -3.14"); return; }
    if (!sea_shield_check(n3, SEA_GRAMMAR_NUMERIC)) { FAIL("rejected 1.5e10"); return; }
    PASS();
}

static void test_numeric_blocks_letters(void) {
    TEST("NUMERIC blocks letters");
    SeaSlice input = SEA_SLICE_LIT("42abc");
    if (sea_shield_check(input, SEA_GRAMMAR_NUMERIC)) { FAIL("allowed letters"); return; }
    PASS();
}

static void test_filename_allows_valid(void) {
    TEST("FILENAME allows valid paths");
    SeaSlice input = SEA_SLICE_LIT("docs/report-2026_v2.pdf");
    if (!sea_shield_check(input, SEA_GRAMMAR_FILENAME)) { FAIL("rejected valid path"); return; }
    PASS();
}

static void test_filename_blocks_spaces(void) {
    TEST("FILENAME blocks spaces");
    SeaSlice input = SEA_SLICE_LIT("my file.txt");
    if (sea_shield_check(input, SEA_GRAMMAR_FILENAME)) { FAIL("allowed space"); return; }
    PASS();
}

static void test_url_requires_https(void) {
    TEST("URL validation requires HTTPS");
    SeaSlice good = SEA_SLICE_LIT("https://example.com/path");
    SeaSlice bad  = SEA_SLICE_LIT("http://example.com/path");
    if (!sea_shield_validate_url(good)) { FAIL("rejected HTTPS"); return; }
    if (sea_shield_validate_url(bad)) { FAIL("allowed HTTP"); return; }
    PASS();
}

static void test_command_grammar(void) {
    TEST("COMMAND allows /status, /exec echo");
    SeaSlice c1 = SEA_SLICE_LIT("/status");
    SeaSlice c2 = SEA_SLICE_LIT("/exec echo hello");
    if (!sea_shield_check(c1, SEA_GRAMMAR_COMMAND)) { FAIL("rejected /status"); return; }
    if (!sea_shield_check(c2, SEA_GRAMMAR_COMMAND)) { FAIL("rejected /exec"); return; }
    PASS();
}

static void test_injection_shell(void) {
    TEST("detect shell injection: $(), ``, &&, ||, ;");
    SeaSlice i1 = SEA_SLICE_LIT("$(rm -rf /)");
    SeaSlice i2 = SEA_SLICE_LIT("`whoami`");
    SeaSlice i3 = SEA_SLICE_LIT("ls && rm -rf /");
    SeaSlice i4 = SEA_SLICE_LIT("true || cat /etc/passwd");
    SeaSlice i5 = SEA_SLICE_LIT("echo hi; rm -rf /");
    if (!sea_shield_detect_injection(i1)) { FAIL("missed $()"); return; }
    if (!sea_shield_detect_injection(i2)) { FAIL("missed backtick"); return; }
    if (!sea_shield_detect_injection(i3)) { FAIL("missed &&"); return; }
    if (!sea_shield_detect_injection(i4)) { FAIL("missed ||"); return; }
    if (!sea_shield_detect_injection(i5)) { FAIL("missed ;"); return; }
    PASS();
}

static void test_injection_sql(void) {
    TEST("detect SQL injection: DROP, UNION, OR 1=1");
    SeaSlice i1 = SEA_SLICE_LIT("'; DROP TABLE users; --");
    SeaSlice i2 = SEA_SLICE_LIT("1 UNION SELECT * FROM passwords");
    SeaSlice i3 = SEA_SLICE_LIT("admin' OR 1=1 --");
    if (!sea_shield_detect_injection(i1)) { FAIL("missed DROP TABLE"); return; }
    if (!sea_shield_detect_injection(i2)) { FAIL("missed UNION SELECT"); return; }
    if (!sea_shield_detect_injection(i3)) { FAIL("missed OR 1=1"); return; }
    PASS();
}

static void test_injection_xss(void) {
    TEST("detect XSS: <script>, javascript:");
    SeaSlice i1 = SEA_SLICE_LIT("<script>alert('xss')</script>");
    SeaSlice i2 = SEA_SLICE_LIT("javascript:alert(1)");
    if (!sea_shield_detect_injection(i1)) { FAIL("missed <script>"); return; }
    if (!sea_shield_detect_injection(i2)) { FAIL("missed javascript:"); return; }
    PASS();
}

static void test_injection_path_traversal(void) {
    TEST("detect path traversal: ../");
    SeaSlice input = SEA_SLICE_LIT("../../etc/passwd");
    if (!sea_shield_detect_injection(input)) { FAIL("missed ../"); return; }
    PASS();
}

static void test_clean_input_not_flagged(void) {
    TEST("clean input NOT flagged as injection");
    SeaSlice c1 = SEA_SLICE_LIT("Generate invoice for Acme Corp, $500");
    SeaSlice c2 = SEA_SLICE_LIT("Read the file report.pdf");
    SeaSlice c3 = SEA_SLICE_LIT("What is 2 + 2?");
    if (sea_shield_detect_injection(c1)) { FAIL("false positive on invoice"); return; }
    if (sea_shield_detect_injection(c2)) { FAIL("false positive on read"); return; }
    if (sea_shield_detect_injection(c3)) { FAIL("false positive on math"); return; }
    PASS();
}

static void test_magic_pdf(void) {
    TEST("file magic: detect PDF");
    u8 pdf[] = {'%', 'P', 'D', 'F', '-', '1', '.', '4'};
    SeaSlice data = { .data = pdf, .len = 8 };
    if (!sea_shield_check_magic(data, "pdf")) { FAIL("missed PDF"); return; }

    u8 fake[] = {'N', 'O', 'T', 'P', 'D', 'F'};
    SeaSlice bad = { .data = fake, .len = 6 };
    if (sea_shield_check_magic(bad, "pdf")) { FAIL("false positive"); return; }
    PASS();
}

static void test_magic_json(void) {
    TEST("file magic: detect JSON");
    SeaSlice j1 = SEA_SLICE_LIT("{\"key\": \"value\"}");
    SeaSlice j2 = SEA_SLICE_LIT("  [1, 2, 3]");
    if (!sea_shield_check_magic(j1, "json")) { FAIL("missed object"); return; }
    if (!sea_shield_check_magic(j2, "json")) { FAIL("missed array"); return; }
    PASS();
}

static void test_validate_result_detail(void) {
    TEST("validate returns position of bad byte");
    SeaSlice input = SEA_SLICE_LIT("abc!def");
    SeaShieldResult r = sea_shield_validate(input, SEA_GRAMMAR_ALPHA);
    if (r.valid) { FAIL("should reject"); return; }
    if (r.fail_pos != 3) { FAIL("wrong position"); return; }
    if (r.fail_byte != '!') { FAIL("wrong byte"); return; }
    PASS();
}

static void test_empty_input_valid(void) {
    TEST("empty input is valid for all grammars");
    SeaSlice empty = SEA_SLICE_EMPTY;
    for (int g = 0; g < SEA_GRAMMAR_COUNT; g++) {
        if (!sea_shield_check(empty, (SeaGrammarType)g)) {
            FAIL("rejected empty input");
            return;
        }
    }
    PASS();
}

static void test_benchmark_validation(void) {
    TEST("benchmark: 100K validations of 200B input");

    const char* text = "Generate an invoice for Acme Corp with amount $500.00 USD. "
                       "Include items: Widget A, Widget B, and Service C. "
                       "Send to billing@acme.com and log the transaction.";
    SeaSlice input = { .data = (const u8*)text, .len = (u32)strlen(text) };

    u64 t0 = sea_log_elapsed_ms();

    for (u32 i = 0; i < 100000; i++) {
        sea_shield_check(input, SEA_GRAMMAR_SAFE_TEXT);
    }

    u64 t1 = sea_log_elapsed_ms();
    u64 elapsed = t1 - t0;
    f64 per_check_us = (f64)elapsed * 1000.0 / 100000.0;

    if (per_check_us > 10.0) {
        char msg[64];
        snprintf(msg, sizeof(msg), "too slow: %.2f us/check (target <10us)", per_check_us);
        FAIL(msg);
        return;
    }

    printf("\033[32mPASS\033[0m (%lums total, %.2f us/check)\n",
           (unsigned long)elapsed, per_check_us);
    s_pass++;
}

/* ── Main ─────────────────────────────────────────────────── */

int main(void) {
    sea_log_init(SEA_LOG_WARN);

    printf("\n  \033[1mSea-Claw Shield Tests\033[0m\n");
    printf("  ════════════════════════════════════════════════\n\n");

    test_safe_text_allows_normal();
    test_safe_text_blocks_null();
    test_safe_text_blocks_control();
    test_numeric_allows_numbers();
    test_numeric_blocks_letters();
    test_filename_allows_valid();
    test_filename_blocks_spaces();
    test_url_requires_https();
    test_command_grammar();
    test_injection_shell();
    test_injection_sql();
    test_injection_xss();
    test_injection_path_traversal();
    test_clean_input_not_flagged();
    test_magic_pdf();
    test_magic_json();
    test_validate_result_detail();
    test_empty_input_valid();
    test_benchmark_validation();

    printf("\n  ────────────────────────────────────────────────\n");
    printf("  Results: \033[32m%u passed\033[0m", s_pass);
    if (s_fail > 0) printf(", \033[31m%u failed\033[0m", s_fail);
    printf("\n\n");

    return s_fail > 0 ? 1 : 0;
}
