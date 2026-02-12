/*
 * sea_shield.c — Byte-level charset validation
 *
 * The Grammar Filter: every byte is checked against a lookup table.
 * 256-byte bitmap per grammar — O(1) per byte, no branching.
 */

#include "seaclaw/sea_shield.h"
#include "seaclaw/sea_log.h"
#include <string.h>

/* ── Grammar lookup tables (256-byte bitmaps) ─────────────── */
/* 1 = allowed, 0 = rejected                                   */

static u8 s_grammar_tables[SEA_GRAMMAR_COUNT][256];
static bool s_initialized = false;

static void set_range(u8* table, u8 lo, u8 hi) {
    for (int c = lo; c <= hi; c++) table[c] = 1;
}

static void set_chars(u8* table, const char* chars) {
    while (*chars) { table[(u8)*chars] = 1; chars++; }
}

static void init_grammars(void) {
    if (s_initialized) return;
    memset(s_grammar_tables, 0, sizeof(s_grammar_tables));

    /* SAFE_TEXT: printable ASCII (0x20-0x7E) + tab + newline */
    {
        u8* t = s_grammar_tables[SEA_GRAMMAR_SAFE_TEXT];
        set_range(t, 0x20, 0x7E);
        t['\t'] = 1;
        t['\n'] = 1;
        t['\r'] = 1;
        /* Allow UTF-8 continuation bytes (0x80-0xFE) for unicode */
        set_range(t, 0x80, 0xFE);
    }

    /* NUMERIC: digits, dot, minus, plus, e/E */
    {
        u8* t = s_grammar_tables[SEA_GRAMMAR_NUMERIC];
        set_range(t, '0', '9');
        set_chars(t, ".-+eE");
    }

    /* ALPHA: letters only */
    {
        u8* t = s_grammar_tables[SEA_GRAMMAR_ALPHA];
        set_range(t, 'a', 'z');
        set_range(t, 'A', 'Z');
    }

    /* ALPHANUM: letters + digits */
    {
        u8* t = s_grammar_tables[SEA_GRAMMAR_ALPHANUM];
        set_range(t, 'a', 'z');
        set_range(t, 'A', 'Z');
        set_range(t, '0', '9');
    }

    /* FILENAME: alphanumeric + . - _ / */
    {
        u8* t = s_grammar_tables[SEA_GRAMMAR_FILENAME];
        set_range(t, 'a', 'z');
        set_range(t, 'A', 'Z');
        set_range(t, '0', '9');
        set_chars(t, ".-_/");
    }

    /* URL: RFC 3986 unreserved + reserved subset */
    {
        u8* t = s_grammar_tables[SEA_GRAMMAR_URL];
        set_range(t, 'a', 'z');
        set_range(t, 'A', 'Z');
        set_range(t, '0', '9');
        set_chars(t, "-._~:/?#[]@!$&'()*+,;=%");
    }

    /* JSON: all printable ASCII + whitespace */
    {
        u8* t = s_grammar_tables[SEA_GRAMMAR_JSON];
        set_range(t, 0x20, 0x7E);
        t['\t'] = 1;
        t['\n'] = 1;
        t['\r'] = 1;
        set_range(t, 0x80, 0xFE); /* UTF-8 in strings */
    }

    /* COMMAND: / prefix + alphanumeric + space + basic punctuation */
    {
        u8* t = s_grammar_tables[SEA_GRAMMAR_COMMAND];
        set_range(t, 'a', 'z');
        set_range(t, 'A', 'Z');
        set_range(t, '0', '9');
        set_chars(t, " /._-@#:,");
    }

    /* HEX */
    {
        u8* t = s_grammar_tables[SEA_GRAMMAR_HEX];
        set_range(t, '0', '9');
        set_range(t, 'a', 'f');
        set_range(t, 'A', 'F');
    }

    /* BASE64 */
    {
        u8* t = s_grammar_tables[SEA_GRAMMAR_BASE64];
        set_range(t, 'A', 'Z');
        set_range(t, 'a', 'z');
        set_range(t, '0', '9');
        set_chars(t, "+/=");
    }

    s_initialized = true;
}

/* ── Public API ───────────────────────────────────────────── */

SeaShieldResult sea_shield_validate(SeaSlice input, SeaGrammarType grammar) {
    init_grammars();

    SeaShieldResult result = {
        .valid    = true,
        .fail_pos = 0,
        .fail_byte = 0,
        .reason   = NULL,
    };

    if (grammar >= SEA_GRAMMAR_COUNT) {
        result.valid = false;
        result.reason = "Unknown grammar type";
        return result;
    }

    if (input.len == 0) return result; /* Empty input is valid */

    const u8* table = s_grammar_tables[grammar];

    for (u32 i = 0; i < input.len; i++) {
        if (!table[input.data[i]]) {
            result.valid     = false;
            result.fail_pos  = i;
            result.fail_byte = input.data[i];
            result.reason    = "Byte not in grammar charset";
            return result;
        }
    }

    return result;
}

bool sea_shield_check(SeaSlice input, SeaGrammarType grammar) {
    return sea_shield_validate(input, grammar).valid;
}

SeaError sea_shield_enforce(SeaSlice input, SeaGrammarType grammar, const char* context) {
    SeaShieldResult r = sea_shield_validate(input, grammar);
    if (r.valid) return SEA_OK;

    SEA_LOG_WARN("SHIELD", "REJECTED [%s] grammar=%s pos=%u byte=0x%02X: %s",
                 context,
                 sea_grammar_name(grammar),
                 r.fail_pos,
                 r.fail_byte,
                 r.reason);

    return SEA_ERR_GRAMMAR_REJECT;
}

/* ── Injection detection ──────────────────────────────────── */

/* Strict patterns for USER INPUT and TOOL ARGS — shell metacharacters matter */
static const char* s_input_injection_patterns[] = {
    "$(", "`",  "&&", "||", ";",
    "../", "\\",
    "<script", "javascript:", "eval(",
    "DROP TABLE", "DELETE FROM", "INSERT INTO",
    "UNION SELECT", "OR 1=1", "' OR '",
    NULL
};

/* Relaxed patterns for LLM OUTPUT — skip shell metacharacters that appear
 * naturally in markdown tables (|), comparisons (||), semicolons in prose,
 * and backslashes in paths. Only catch actual prompt injection and XSS. */
static const char* s_output_injection_patterns[] = {
    "<script", "javascript:", "eval(",
    "ignore previous instructions",
    "ignore all previous",
    "disregard your instructions",
    "you are now",
    "new instructions:",
    "system prompt:",
    "ADMIN OVERRIDE",
    NULL
};

static bool detect_patterns(SeaSlice input, const char** patterns) {
    if (input.len == 0) return false;

    for (int p = 0; patterns[p]; p++) {
        const char* pat = patterns[p];
        u32 plen = (u32)strlen(pat);
        if (plen > input.len) continue;

        for (u32 i = 0; i <= input.len - plen; i++) {
            bool match = true;
            for (u32 j = 0; j < plen; j++) {
                /* Case-insensitive */
                u8 a = input.data[i + j];
                u8 b = (u8)pat[j];
                if (a >= 'A' && a <= 'Z') a += 32;
                if (b >= 'A' && b <= 'Z') b += 32;
                if (a != b) { match = false; break; }
            }
            if (match) return true;
        }
    }

    /* Check for null bytes */
    for (u32 i = 0; i < input.len; i++) {
        if (input.data[i] == 0x00) return true;
    }

    return false;
}

bool sea_shield_detect_injection(SeaSlice input) {
    return detect_patterns(input, s_input_injection_patterns);
}

bool sea_shield_detect_output_injection(SeaSlice output) {
    return detect_patterns(output, s_output_injection_patterns);
}

/* ── URL validation ───────────────────────────────────────── */

bool sea_shield_validate_url(SeaSlice url) {
    if (url.len < 9) return false; /* minimum: https://x */

    /* Must start with https:// */
    if (memcmp(url.data, "https://", 8) != 0) return false;

    /* Must pass URL grammar */
    if (!sea_shield_check(url, SEA_GRAMMAR_URL)) return false;

    return true;
}

/* ── File magic ───────────────────────────────────────────── */

bool sea_shield_check_magic(SeaSlice data, const char* expected_type) {
    if (data.len < 4) return false;

    if (strcmp(expected_type, "pdf") == 0) {
        return memcmp(data.data, "%PDF", 4) == 0;
    }
    if (strcmp(expected_type, "png") == 0) {
        return data.len >= 8 &&
               data.data[0] == 0x89 &&
               data.data[1] == 'P' &&
               data.data[2] == 'N' &&
               data.data[3] == 'G';
    }
    if (strcmp(expected_type, "json") == 0) {
        /* JSON starts with { or [ (after optional whitespace) */
        u32 i = 0;
        while (i < data.len && (data.data[i] == ' ' || data.data[i] == '\n' ||
               data.data[i] == '\r' || data.data[i] == '\t')) i++;
        if (i >= data.len) return false;
        return data.data[i] == '{' || data.data[i] == '[';
    }

    return false;
}

/* ── Grammar names ────────────────────────────────────────── */

const char* sea_grammar_name(SeaGrammarType grammar) {
    switch (grammar) {
        case SEA_GRAMMAR_SAFE_TEXT: return "SAFE_TEXT";
        case SEA_GRAMMAR_NUMERIC:  return "NUMERIC";
        case SEA_GRAMMAR_ALPHA:    return "ALPHA";
        case SEA_GRAMMAR_ALPHANUM: return "ALPHANUM";
        case SEA_GRAMMAR_FILENAME: return "FILENAME";
        case SEA_GRAMMAR_URL:      return "URL";
        case SEA_GRAMMAR_JSON:     return "JSON";
        case SEA_GRAMMAR_COMMAND:  return "COMMAND";
        case SEA_GRAMMAR_HEX:     return "HEX";
        case SEA_GRAMMAR_BASE64:  return "BASE64";
        default:                   return "UNKNOWN";
    }
}
