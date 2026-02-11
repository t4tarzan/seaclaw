/*
 * sea_types.h — Shared type definitions for Sea-Claw
 *
 * Fixed-width types, error codes, and core structures
 * used across the entire system for bit-level consistency.
 *
 * Zero dependencies. Pure C11.
 */

#ifndef SEA_TYPES_H
#define SEA_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ── Fixed-width type aliases ─────────────────────────────── */

typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;
typedef int8_t    i8;
typedef int16_t   i16;
typedef int32_t   i32;
typedef int64_t   i64;
typedef float     f32;
typedef double    f64;

/* ── Compile-time assertions ──────────────────────────────── */

_Static_assert(sizeof(f32) == 4, "f32 must be 4 bytes");
_Static_assert(sizeof(f64) == 8, "f64 must be 8 bytes");
_Static_assert(sizeof(u64) == 8, "u64 must be 8 bytes");

/* ── SeaSlice: Zero-copy string view ─────────────────────── */
/* Points into existing buffer. Never owns memory.            */

typedef struct {
    const u8* data;
    u32       len;
} SeaSlice;

#define SEA_SLICE_EMPTY ((SeaSlice){NULL, 0})
#define SEA_SLICE_LIT(s) ((SeaSlice){(const u8*)(s), sizeof(s) - 1})

static inline bool sea_slice_eq(SeaSlice a, SeaSlice b) {
    if (a.len != b.len) return false;
    if (a.len == 0) return true;
    for (u32 i = 0; i < a.len; i++) {
        if (a.data[i] != b.data[i]) return false;
    }
    return true;
}

static inline bool sea_slice_eq_cstr(SeaSlice s, const char* cstr) {
    u32 clen = 0;
    while (cstr[clen]) clen++;
    if (s.len != clen) return false;
    for (u32 i = 0; i < clen; i++) {
        if (s.data[i] != (u8)cstr[i]) return false;
    }
    return true;
}

/* ── Error codes ──────────────────────────────────────────── */

typedef enum {
    SEA_OK = 0,

    /* Memory */
    SEA_ERR_OOM,
    SEA_ERR_ARENA_FULL,

    /* I/O */
    SEA_ERR_IO,
    SEA_ERR_EOF,
    SEA_ERR_TIMEOUT,
    SEA_ERR_CONNECT,

    /* Parsing */
    SEA_ERR_PARSE,
    SEA_ERR_INVALID_JSON,
    SEA_ERR_UNEXPECTED_TOKEN,

    /* Security */
    SEA_ERR_INVALID_INPUT,
    SEA_ERR_GRAMMAR_REJECT,
    SEA_ERR_SANDBOX_FAIL,
    SEA_ERR_PERMISSION,

    /* Tools */
    SEA_ERR_TOOL_NOT_FOUND,
    SEA_ERR_TOOL_FAILED,

    /* Model */
    SEA_ERR_MODEL_LOAD,
    SEA_ERR_INFERENCE,

    /* Config */
    SEA_ERR_CONFIG,
    SEA_ERR_MISSING_KEY,

    /* General */
    SEA_ERR_NOT_FOUND,
    SEA_ERR_ALREADY_EXISTS,
    SEA_ERR_NOT_IMPLEMENTED,

    SEA_ERR_COUNT
} SeaError;

static inline const char* sea_error_str(SeaError err) {
    switch (err) {
        case SEA_OK:                  return "OK";
        case SEA_ERR_OOM:             return "Out of memory";
        case SEA_ERR_ARENA_FULL:      return "Arena full";
        case SEA_ERR_IO:              return "I/O error";
        case SEA_ERR_EOF:             return "End of file";
        case SEA_ERR_TIMEOUT:         return "Timeout";
        case SEA_ERR_CONNECT:         return "Connection failed";
        case SEA_ERR_PARSE:           return "Parse error";
        case SEA_ERR_INVALID_JSON:    return "Invalid JSON";
        case SEA_ERR_UNEXPECTED_TOKEN:return "Unexpected token";
        case SEA_ERR_INVALID_INPUT:   return "Invalid input";
        case SEA_ERR_GRAMMAR_REJECT:  return "Grammar rejected";
        case SEA_ERR_SANDBOX_FAIL:    return "Sandbox failure";
        case SEA_ERR_PERMISSION:      return "Permission denied";
        case SEA_ERR_TOOL_NOT_FOUND:  return "Tool not found";
        case SEA_ERR_TOOL_FAILED:     return "Tool execution failed";
        case SEA_ERR_MODEL_LOAD:      return "Model load failed";
        case SEA_ERR_INFERENCE:       return "Inference error";
        case SEA_ERR_CONFIG:          return "Config error";
        case SEA_ERR_MISSING_KEY:     return "Missing key";
        case SEA_ERR_NOT_FOUND:       return "Not found";
        case SEA_ERR_ALREADY_EXISTS:  return "Already exists";
        case SEA_ERR_NOT_IMPLEMENTED: return "Not implemented";
        default:                      return "Unknown error";
    }
}

/* ── Agent states ─────────────────────────────────────────── */

typedef enum {
    SEA_AGENT_IDLE = 0,
    SEA_AGENT_PLANNING,
    SEA_AGENT_EXECUTING,
    SEA_AGENT_STREAMING,
    SEA_AGENT_REFLECTING,
    SEA_AGENT_HALTED,
} SeaAgentState;

/* ── Version ──────────────────────────────────────────────── */

#define SEA_VERSION_MAJOR 1
#define SEA_VERSION_MINOR 0
#define SEA_VERSION_PATCH 0
#define SEA_VERSION_STRING "1.0.0"

/* ── Limits ───────────────────────────────────────────────── */

#define SEA_MAX_TOOLS       256
#define SEA_MAX_TOOL_NAME   64
#define SEA_MAX_PATH        4096
#define SEA_MAX_LINE        8192
#define SEA_MAX_JSON_DEPTH  32

#endif /* SEA_TYPES_H */
