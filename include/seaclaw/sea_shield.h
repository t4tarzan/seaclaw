/*
 * sea_shield.h — The Grammar Filter
 *
 * "A genius in a straightjacket. The AI has no voice, only a filter.
 *  It has no freedom, only assigned tools."
 *
 * Byte-level charset validation. Every input and output is checked
 * against a grammar before it touches the engine. If data doesn't
 * fit the shape, it is rejected instantly.
 */

#ifndef SEA_SHIELD_H
#define SEA_SHIELD_H

#include "sea_types.h"

/* ── Grammar types ────────────────────────────────────────── */

typedef enum {
    SEA_GRAMMAR_SAFE_TEXT = 0,  /* Printable ASCII + common unicode, no control chars */
    SEA_GRAMMAR_NUMERIC,        /* Digits, dot, minus, plus, e/E                      */
    SEA_GRAMMAR_ALPHA,          /* Letters only (a-z, A-Z)                             */
    SEA_GRAMMAR_ALPHANUM,       /* Letters + digits                                    */
    SEA_GRAMMAR_FILENAME,       /* Alphanumeric + . - _ /                              */
    SEA_GRAMMAR_URL,            /* URL-safe characters                                 */
    SEA_GRAMMAR_JSON,           /* Valid JSON characters                                */
    SEA_GRAMMAR_COMMAND,        /* / prefix + alphanumeric + space + basic punctuation  */
    SEA_GRAMMAR_HEX,            /* 0-9, a-f, A-F                                       */
    SEA_GRAMMAR_BASE64,         /* A-Z, a-z, 0-9, +, /, =                              */

    SEA_GRAMMAR_COUNT
} SeaGrammarType;

/* ── Validation result ────────────────────────────────────── */

typedef struct {
    bool     valid;
    u32      fail_pos;      /* Position of first invalid byte (if !valid) */
    u8       fail_byte;     /* The offending byte                         */
    const char* reason;     /* Human-readable rejection reason            */
} SeaShieldResult;

/* ── API ──────────────────────────────────────────────────── */

/* Validate a byte slice against a grammar.
 * Returns result with valid=true if all bytes pass. */
SeaShieldResult sea_shield_validate(SeaSlice input, SeaGrammarType grammar);

/* Quick check — returns true/false only */
bool sea_shield_check(SeaSlice input, SeaGrammarType grammar);

/* Validate and reject with logging */
SeaError sea_shield_enforce(SeaSlice input, SeaGrammarType grammar, const char* context);

/* ── Specific validators ──────────────────────────────────── */

/* Check if input looks like a shell injection attempt (strict: shell metacharacters) */
bool sea_shield_detect_injection(SeaSlice input);

/* Check if LLM output contains injection (relaxed: no shell metachar, only prompt injection + XSS) */
bool sea_shield_detect_output_injection(SeaSlice output);

/* Check if a URL is HTTPS and on an allowed domain */
bool sea_shield_validate_url(SeaSlice url);

/* Check file magic bytes (PDF, PNG, etc.) */
bool sea_shield_check_magic(SeaSlice data, const char* expected_type);

/* Canonicalize path and verify it's within workspace (prevents symlink escape) */
bool sea_shield_canonicalize_path(const char* path, const char* workspace_dir, char* resolved, size_t resolved_size);

/* Get grammar name for logging */
const char* sea_grammar_name(SeaGrammarType grammar);

#endif /* SEA_SHIELD_H */
