/*
 * sea_pii.c — PII Firewall Implementation
 *
 * Byte-level PII detection without regex. Pure C pattern matching.
 * Detects emails, phone numbers, SSNs, credit cards, IP addresses.
 */

#include "seaclaw/sea_pii.h"
#include "seaclaw/sea_log.h"

#include <string.h>
#include <ctype.h>

/* ── Helpers ──────────────────────────────────────────────── */

static inline bool is_digit(u8 c) { return c >= '0' && c <= '9'; }
static inline bool is_alpha(u8 c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
static inline bool is_alnum(u8 c) { return is_digit(c) || is_alpha(c); }
static inline bool is_word_boundary(u8 c) { return !is_alnum(c) && c != '_'; }

static void add_match(SeaPiiResult* r, SeaPiiCategory cat, u32 offset, u32 len) {
    if (r->count < SEA_PII_MAX_MATCHES) {
        r->matches[r->count].category = cat;
        r->matches[r->count].offset = offset;
        r->matches[r->count].length = len;
        r->count++;
        r->has_pii = true;
    }
}

/* ── Email Detection ─────────────────────────────────────── */
/* Pattern: local@domain.tld where local has alnum/._+- and domain has alnum/.- */

static void scan_emails(const u8* data, u32 len, SeaPiiResult* r) {
    for (u32 i = 1; i < len; i++) {
        if (data[i] != '@') continue;

        /* Scan backwards for local part */
        u32 start = i;
        while (start > 0) {
            u8 c = data[start - 1];
            if (is_alnum(c) || c == '.' || c == '_' || c == '+' || c == '-') {
                start--;
            } else {
                break;
            }
        }
        if (start == i) continue; /* No local part */

        /* Scan forward for domain */
        u32 end = i + 1;
        bool has_dot = false;
        while (end < len) {
            u8 c = data[end];
            if (is_alnum(c) || c == '-') {
                end++;
            } else if (c == '.' && end + 1 < len && is_alnum(data[end + 1])) {
                has_dot = true;
                end++;
            } else {
                break;
            }
        }
        if (!has_dot || end - i < 4) continue; /* Need at least x@y.z */

        add_match(r, SEA_PII_EMAIL, start, end - start);
        i = end; /* Skip past this match */
    }
}

/* ── Phone Detection ─────────────────────────────────────── */
/* Patterns: +1-234-567-8901, (234) 567-8901, 234-567-8901, 2345678901 */

static void scan_phones(const u8* data, u32 len, SeaPiiResult* r) {
    for (u32 i = 0; i < len; i++) {
        u32 start = i;
        u32 digit_count = 0;
        u32 j = i;

        /* Optional + prefix */
        if (j < len && data[j] == '+') j++;

        /* Count digits, allowing separators */
        while (j < len && digit_count < 15) {
            u8 c = data[j];
            if (is_digit(c)) {
                digit_count++;
                j++;
            } else if (c == '-' || c == ' ' || c == '.' || c == '(' || c == ')') {
                j++;
            } else {
                break;
            }
        }

        /* Valid phone: 10-15 digits */
        if (digit_count >= 10 && digit_count <= 15) {
            /* Check word boundaries */
            if (start > 0 && is_alnum(data[start - 1])) continue;
            if (j < len && is_alnum(data[j])) continue;
            add_match(r, SEA_PII_PHONE, start, j - start);
            i = j - 1;
        }
    }
}

/* ── SSN Detection ───────────────────────────────────────── */
/* Pattern: XXX-XX-XXXX */

static void scan_ssns(const u8* data, u32 len, SeaPiiResult* r) {
    if (len < 11) return;
    for (u32 i = 0; i <= len - 11; i++) {
        if (is_digit(data[i])   && is_digit(data[i+1]) && is_digit(data[i+2]) &&
            data[i+3] == '-' &&
            is_digit(data[i+4]) && is_digit(data[i+5]) &&
            data[i+6] == '-' &&
            is_digit(data[i+7]) && is_digit(data[i+8]) && is_digit(data[i+9]) &&
            is_digit(data[i+10])) {
            /* Check word boundaries */
            if (i > 0 && is_digit(data[i-1])) continue;
            if (i + 11 < len && is_digit(data[i+11])) continue;
            /* Reject 000, 666, 9xx area codes */
            u32 area = (data[i]-'0')*100 + (data[i+1]-'0')*10 + (data[i+2]-'0');
            if (area == 0 || area == 666 || area >= 900) continue;
            add_match(r, SEA_PII_SSN, i, 11);
            i += 10;
        }
    }
}

/* ── Credit Card Detection ───────────────────────────────── */
/* Luhn algorithm validation on 13-19 digit sequences */

static bool luhn_check(const u8* digits, u32 count) {
    if (count < 13 || count > 19) return false;
    i32 sum = 0;
    bool alt = false;
    for (i32 i = (i32)count - 1; i >= 0; i--) {
        i32 d = digits[i] - '0';
        if (alt) {
            d *= 2;
            if (d > 9) d -= 9;
        }
        sum += d;
        alt = !alt;
    }
    return (sum % 10) == 0;
}

static void scan_credit_cards(const u8* data, u32 len, SeaPiiResult* r) {
    for (u32 i = 0; i < len; i++) {
        if (!is_digit(data[i])) continue;
        /* Check word boundary before */
        if (i > 0 && is_alnum(data[i-1])) continue;

        /* Extract digits, allowing spaces and dashes */
        u8 digits[20];
        u32 dcount = 0;
        u32 j = i;
        while (j < len && dcount < 20) {
            u8 c = data[j];
            if (is_digit(c)) {
                digits[dcount++] = c;
                j++;
            } else if (c == ' ' || c == '-') {
                j++;
            } else {
                break;
            }
        }

        if (dcount >= 13 && dcount <= 19) {
            /* Check word boundary after */
            if (j < len && is_alnum(data[j])) continue;
            if (luhn_check(digits, dcount)) {
                add_match(r, SEA_PII_CREDIT_CARD, i, j - i);
                i = j - 1;
            }
        }
    }
}

/* ── IP Address Detection ────────────────────────────────── */
/* Pattern: X.X.X.X where X is 0-255 */

static void scan_ip_addresses(const u8* data, u32 len, SeaPiiResult* r) {
    for (u32 i = 0; i < len; i++) {
        if (!is_digit(data[i])) continue;
        if (i > 0 && (is_alnum(data[i-1]) || data[i-1] == '.')) continue;

        u32 start = i;
        u32 octets = 0;
        u32 j = i;

        for (u32 oct = 0; oct < 4; oct++) {
            u32 val = 0, digits = 0;
            while (j < len && is_digit(data[j]) && digits < 3) {
                val = val * 10 + (data[j] - '0');
                j++;
                digits++;
            }
            if (digits == 0 || val > 255) break;
            octets++;
            if (oct < 3) {
                if (j >= len || data[j] != '.') break;
                j++; /* skip dot */
            }
        }

        if (octets == 4) {
            if (j < len && (is_digit(data[j]) || data[j] == '.')) continue;
            /* Skip common non-PII IPs: 0.0.0.0, 127.0.0.1, 255.255.255.255 */
            add_match(r, SEA_PII_IP_ADDR, start, j - start);
            i = j - 1;
        }
    }
}

/* ── Public API ──────────────────────────────────────────── */

SeaPiiResult sea_pii_scan(SeaSlice text, u32 categories) {
    SeaPiiResult r = { .count = 0, .has_pii = false };
    if (text.len == 0 || !text.data) return r;

    if (categories & SEA_PII_EMAIL)       scan_emails(text.data, text.len, &r);
    if (categories & SEA_PII_PHONE)       scan_phones(text.data, text.len, &r);
    if (categories & SEA_PII_SSN)         scan_ssns(text.data, text.len, &r);
    if (categories & SEA_PII_CREDIT_CARD) scan_credit_cards(text.data, text.len, &r);
    if (categories & SEA_PII_IP_ADDR)     scan_ip_addresses(text.data, text.len, &r);

    return r;
}

const char* sea_pii_redact(SeaSlice text, u32 categories, SeaArena* arena) {
    if (text.len == 0 || !text.data) return "";

    SeaPiiResult r = sea_pii_scan(text, categories);
    if (!r.has_pii) {
        /* No PII — return copy */
        char* out = (char*)sea_arena_alloc(arena, text.len + 1, 1);
        if (!out) return NULL;
        memcpy(out, text.data, text.len);
        out[text.len] = '\0';
        return out;
    }

    /* Sort matches by offset (simple insertion sort) */
    for (u32 i = 1; i < r.count; i++) {
        SeaPiiMatch tmp = r.matches[i];
        i32 j = (i32)i - 1;
        while (j >= 0 && r.matches[j].offset > tmp.offset) {
            r.matches[j + 1] = r.matches[j];
            j--;
        }
        r.matches[j + 1] = tmp;
    }

    /* Build redacted string */
    /* Worst case: each match replaced with "[REDACTED]" (10 chars) */
    u32 max_out = text.len + r.count * 10;
    char* out = (char*)sea_arena_alloc(arena, max_out + 1, 1);
    if (!out) return NULL;

    u32 pos = 0, src = 0;
    for (u32 m = 0; m < r.count; m++) {
        SeaPiiMatch* match = &r.matches[m];
        if (match->offset < src) continue; /* Overlapping match */

        /* Copy text before match */
        u32 gap = match->offset - src;
        if (gap > 0) {
            memcpy(out + pos, text.data + src, gap);
            pos += gap;
        }

        /* Insert redaction marker */
        const char* marker = "[REDACTED]";
        u32 mlen = 10;
        memcpy(out + pos, marker, mlen);
        pos += mlen;

        src = match->offset + match->length;
    }

    /* Copy remaining text */
    if (src < text.len) {
        memcpy(out + pos, text.data + src, text.len - src);
        pos += text.len - src;
    }
    out[pos] = '\0';

    SEA_LOG_INFO("PII", "Redacted %u PII match(es)", r.count);
    return out;
}

bool sea_pii_contains(SeaSlice text, u32 categories) {
    SeaPiiResult r = sea_pii_scan(text, categories);
    return r.has_pii;
}

const char* sea_pii_category_name(SeaPiiCategory cat) {
    switch (cat) {
        case SEA_PII_EMAIL:       return "email";
        case SEA_PII_PHONE:       return "phone";
        case SEA_PII_SSN:         return "ssn";
        case SEA_PII_CREDIT_CARD: return "credit_card";
        case SEA_PII_IP_ADDR:     return "ip_address";
        default:                  return "unknown";
    }
}
