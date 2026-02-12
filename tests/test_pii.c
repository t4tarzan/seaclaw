/*
 * test_pii.c — PII Firewall Tests
 */

#include "seaclaw/sea_pii.h"
#include "seaclaw/sea_arena.h"
#include "seaclaw/sea_log.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

static SeaArena arena;

#define PASS(name) printf("  \033[32m✓\033[0m %s\n", name)
#define FAIL(name, msg) do { printf("  \033[31m✗\033[0m %s: %s\n", name, msg); assert(0); } while(0)

/* ── Email Detection ─────────────────────────────────────── */

static void test_email_detection(void) {
    {
        SeaSlice text = SEA_SLICE_LIT("Contact me at john@example.com for details");
        SeaPiiResult r = sea_pii_scan(text, SEA_PII_EMAIL);
        if (!r.has_pii || r.count != 1) FAIL("email_basic", "should detect 1 email");
        if (r.matches[0].category != SEA_PII_EMAIL) FAIL("email_basic", "wrong category");
        PASS("email_basic");
    }
    {
        SeaSlice text = SEA_SLICE_LIT("Send to alice+tag@sub.domain.co.uk now");
        SeaPiiResult r = sea_pii_scan(text, SEA_PII_EMAIL);
        if (!r.has_pii) FAIL("email_complex", "should detect complex email");
        PASS("email_complex");
    }
    {
        SeaSlice text = SEA_SLICE_LIT("No emails here, just plain text.");
        SeaPiiResult r = sea_pii_scan(text, SEA_PII_EMAIL);
        if (r.has_pii) FAIL("email_none", "false positive");
        PASS("email_none");
    }
}

/* ── Phone Detection ─────────────────────────────────────── */

static void test_phone_detection(void) {
    {
        SeaSlice text = SEA_SLICE_LIT("Call me at +1-234-567-8901");
        SeaPiiResult r = sea_pii_scan(text, SEA_PII_PHONE);
        if (!r.has_pii) FAIL("phone_intl", "should detect international phone");
        PASS("phone_intl");
    }
    {
        SeaSlice text = SEA_SLICE_LIT("My number is (555) 123-4567");
        SeaPiiResult r = sea_pii_scan(text, SEA_PII_PHONE);
        if (!r.has_pii) FAIL("phone_us", "should detect US phone");
        PASS("phone_us");
    }
    {
        SeaSlice text = SEA_SLICE_LIT("The year is 2025.");
        SeaPiiResult r = sea_pii_scan(text, SEA_PII_PHONE);
        if (r.has_pii) FAIL("phone_none", "false positive on year");
        PASS("phone_none");
    }
}

/* ── SSN Detection ───────────────────────────────────────── */

static void test_ssn_detection(void) {
    {
        SeaSlice text = SEA_SLICE_LIT("SSN: 123-45-6789");
        SeaPiiResult r = sea_pii_scan(text, SEA_PII_SSN);
        if (!r.has_pii) FAIL("ssn_basic", "should detect SSN");
        PASS("ssn_basic");
    }
    {
        /* Area code 000 is invalid */
        SeaSlice text = SEA_SLICE_LIT("SSN: 000-12-3456");
        SeaPiiResult r = sea_pii_scan(text, SEA_PII_SSN);
        if (r.has_pii) FAIL("ssn_invalid", "should reject 000 area");
        PASS("ssn_invalid");
    }
}

/* ── Credit Card Detection ───────────────────────────────── */

static void test_credit_card_detection(void) {
    {
        /* Valid Visa test number */
        SeaSlice text = SEA_SLICE_LIT("Card: 4111 1111 1111 1111");
        SeaPiiResult r = sea_pii_scan(text, SEA_PII_CREDIT_CARD);
        if (!r.has_pii) FAIL("cc_visa", "should detect Visa test number");
        PASS("cc_visa");
    }
    {
        SeaSlice text = SEA_SLICE_LIT("Not a card: 1234567890");
        SeaPiiResult r = sea_pii_scan(text, SEA_PII_CREDIT_CARD);
        if (r.has_pii) FAIL("cc_none", "false positive");
        PASS("cc_none");
    }
}

/* ── IP Address Detection ────────────────────────────────── */

static void test_ip_detection(void) {
    {
        SeaSlice text = SEA_SLICE_LIT("Server at 192.168.1.100");
        SeaPiiResult r = sea_pii_scan(text, SEA_PII_IP_ADDR);
        if (!r.has_pii) FAIL("ip_basic", "should detect IP");
        PASS("ip_basic");
    }
    {
        SeaSlice text = SEA_SLICE_LIT("Version 1.2.3");
        SeaPiiResult r = sea_pii_scan(text, SEA_PII_IP_ADDR);
        if (r.has_pii) FAIL("ip_version", "false positive on version");
        PASS("ip_version");
    }
}

/* ── Redaction ───────────────────────────────────────────── */

static void test_redaction(void) {
    {
        SeaSlice text = SEA_SLICE_LIT("Email john@example.com and call +1-234-567-8901");
        const char* redacted = sea_pii_redact(text, SEA_PII_EMAIL | SEA_PII_PHONE, &arena);
        if (!redacted) FAIL("redact_multi", "redaction returned NULL");
        if (strstr(redacted, "john@example.com")) FAIL("redact_multi", "email not redacted");
        if (strstr(redacted, "[REDACTED]") == NULL) FAIL("redact_multi", "no redaction marker");
        PASS("redact_multi");
    }
    {
        SeaSlice text = SEA_SLICE_LIT("No PII here at all.");
        const char* redacted = sea_pii_redact(text, SEA_PII_ALL, &arena);
        if (!redacted) FAIL("redact_none", "redaction returned NULL");
        if (strcmp(redacted, "No PII here at all.") != 0) FAIL("redact_none", "text modified");
        PASS("redact_none");
    }
}

/* ── Category Names ──────────────────────────────────────── */

static void test_category_names(void) {
    if (strcmp(sea_pii_category_name(SEA_PII_EMAIL), "email") != 0)
        FAIL("cat_email", "wrong name");
    if (strcmp(sea_pii_category_name(SEA_PII_PHONE), "phone") != 0)
        FAIL("cat_phone", "wrong name");
    if (strcmp(sea_pii_category_name(SEA_PII_SSN), "ssn") != 0)
        FAIL("cat_ssn", "wrong name");
    if (strcmp(sea_pii_category_name(SEA_PII_CREDIT_CARD), "credit_card") != 0)
        FAIL("cat_cc", "wrong name");
    if (strcmp(sea_pii_category_name(SEA_PII_IP_ADDR), "ip_address") != 0)
        FAIL("cat_ip", "wrong name");
    PASS("category_names");
}

/* ── Main ─────────────────────────────────────────────────── */

int main(void) {
    sea_log_init(SEA_LOG_WARN);
    sea_arena_create(&arena, 64 * 1024);

    printf("\n  PII Firewall Tests\n  ──────────────────\n");

    test_email_detection();
    test_phone_detection();
    test_ssn_detection();
    test_credit_card_detection();
    test_ip_detection();
    test_redaction();
    test_category_names();

    sea_arena_destroy(&arena);
    printf("\n  \033[32mAll PII tests passed.\033[0m\n\n");
    return 0;
}
