/*
 * sea_pii.h — PII Firewall
 *
 * Detects and redacts Personally Identifiable Information (PII)
 * from text before it leaves the local system. Patterns:
 *   - Email addresses
 *   - Phone numbers (US/international)
 *   - Social Security Numbers (SSN)
 *   - Credit card numbers (Luhn-validated)
 *   - IP addresses (v4)
 *
 * "Your data stays sovereign. PII never leaks."
 */

#ifndef SEA_PII_H
#define SEA_PII_H

#include "sea_types.h"
#include "sea_arena.h"

/* ── PII Categories ──────────────────────────────────────── */

typedef enum {
    SEA_PII_EMAIL       = (1 << 0),
    SEA_PII_PHONE       = (1 << 1),
    SEA_PII_SSN         = (1 << 2),
    SEA_PII_CREDIT_CARD = (1 << 3),
    SEA_PII_IP_ADDR     = (1 << 4),
    SEA_PII_ALL         = 0x1F,
} SeaPiiCategory;

/* ── PII Match ───────────────────────────────────────────── */

typedef struct {
    SeaPiiCategory category;
    u32            offset;      /* Byte offset in input */
    u32            length;      /* Length of match */
} SeaPiiMatch;

/* ── PII Scan Result ─────────────────────────────────────── */

#define SEA_PII_MAX_MATCHES 32

typedef struct {
    SeaPiiMatch matches[SEA_PII_MAX_MATCHES];
    u32         count;
    bool        has_pii;
} SeaPiiResult;

/* ── API ──────────────────────────────────────────────────── */

/* Scan text for PII. Returns result with matches. */
SeaPiiResult sea_pii_scan(SeaSlice text, u32 categories);

/* Redact PII in text, replacing matches with [REDACTED].
 * Returns new string allocated in arena. */
const char* sea_pii_redact(SeaSlice text, u32 categories, SeaArena* arena);

/* Check if text contains any PII (quick boolean check). */
bool sea_pii_contains(SeaSlice text, u32 categories);

/* Get human-readable name for a PII category. */
const char* sea_pii_category_name(SeaPiiCategory cat);

#endif /* SEA_PII_H */
