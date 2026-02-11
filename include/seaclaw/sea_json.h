/*
 * sea_json.h — The Shape Sorter
 *
 * Zero-copy JSON parser. Extracts values as SeaSlice pointers
 * directly into the existing buffer. No malloc, no copy, no GC.
 *
 * "Treat input as a River of Bytes. Look via pointers, do not copy.
 *  If data does not fit the Grammar, it is rejected instantly."
 */

#ifndef SEA_JSON_H
#define SEA_JSON_H

#include "sea_types.h"
#include "sea_arena.h"

/* ── JSON value types ─────────────────────────────────────── */

typedef enum {
    SEA_JSON_NULL = 0,
    SEA_JSON_BOOL,
    SEA_JSON_NUMBER,
    SEA_JSON_STRING,
    SEA_JSON_ARRAY,
    SEA_JSON_OBJECT,
} SeaJsonType;

/* ── JSON value (zero-copy — points into source buffer) ───── */

typedef struct SeaJsonValue {
    SeaJsonType type;
    SeaSlice    raw;        /* Raw bytes in source buffer          */

    union {
        bool    boolean;    /* For SEA_JSON_BOOL                   */
        f64     number;     /* For SEA_JSON_NUMBER                 */
        SeaSlice string;    /* For SEA_JSON_STRING (without quotes) */

        struct {            /* For SEA_JSON_ARRAY                  */
            struct SeaJsonValue* items;
            u32 count;
        } array;

        struct {            /* For SEA_JSON_OBJECT                 */
            SeaSlice*            keys;
            struct SeaJsonValue* values;
            u32 count;
        } object;
    };
} SeaJsonValue;

/* ── Parser ───────────────────────────────────────────────── */

/* Parse JSON from a byte slice. Allocates nodes in arena.
 * Returns SEA_OK on success, SEA_ERR_INVALID_JSON on failure.
 * `out` receives the root value. */
SeaError sea_json_parse(SeaSlice input, SeaArena* arena, SeaJsonValue* out);

/* ── Accessors (convenience) ──────────────────────────────── */

/* Find a key in an object. Returns NULL if not found or not an object. */
const SeaJsonValue* sea_json_get(const SeaJsonValue* obj, const char* key);

/* Get string value. Returns empty slice if not a string. */
SeaSlice sea_json_get_string(const SeaJsonValue* obj, const char* key);

/* Get number value. Returns fallback if not a number. */
f64 sea_json_get_number(const SeaJsonValue* obj, const char* key, f64 fallback);

/* Get bool value. Returns fallback if not a bool. */
bool sea_json_get_bool(const SeaJsonValue* obj, const char* key, bool fallback);

/* Get array item by index. Returns NULL if out of bounds. */
const SeaJsonValue* sea_json_array_get(const SeaJsonValue* arr, u32 index);

/* ── Utility ──────────────────────────────────────────────── */

/* Print a JSON value for debugging. */
void sea_json_debug_print(const SeaJsonValue* val, int indent);

#endif /* SEA_JSON_H */
