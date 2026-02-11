/*
 * sea_json.c — Zero-copy JSON parser implementation
 *
 * The Shape Sorter: raw bytes in, SeaSlice pointers out.
 * All nodes allocated from arena. No malloc.
 */

#include "seaclaw/sea_json.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ── Internal parser state ────────────────────────────────── */

typedef struct {
    const u8* src;
    u32       len;
    u32       pos;
    SeaArena* arena;
    u32       depth;
} JsonParser;

/* ── Forward declarations ─────────────────────────────────── */

static SeaError parse_value(JsonParser* p, SeaJsonValue* out);

/* ── Helpers ──────────────────────────────────────────────── */

static inline bool at_end(const JsonParser* p) {
    return p->pos >= p->len;
}

static inline u8 peek(const JsonParser* p) {
    if (at_end(p)) return 0;
    return p->src[p->pos];
}

static inline u8 advance(JsonParser* p) {
    if (at_end(p)) return 0;
    return p->src[p->pos++];
}

static void skip_whitespace(JsonParser* p) {
    while (!at_end(p)) {
        u8 c = p->src[p->pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            p->pos++;
        } else {
            break;
        }
    }
}

static bool expect(JsonParser* p, u8 ch) {
    skip_whitespace(p);
    if (peek(p) == ch) {
        p->pos++;
        return true;
    }
    return false;
}

/* ── Parse string ─────────────────────────────────────────── */

static SeaError parse_string(JsonParser* p, SeaSlice* out) {
    if (advance(p) != '"') return SEA_ERR_INVALID_JSON;

    u32 start = p->pos;

    while (!at_end(p)) {
        u8 c = p->src[p->pos];
        if (c == '"') {
            out->data = p->src + start;
            out->len  = p->pos - start;
            p->pos++; /* skip closing quote */
            return SEA_OK;
        }
        if (c == '\\') {
            p->pos++; /* skip escape char */
            if (at_end(p)) return SEA_ERR_INVALID_JSON;
        }
        p->pos++;
    }

    return SEA_ERR_INVALID_JSON; /* unterminated string */
}

/* ── Parse number ─────────────────────────────────────────── */

static SeaError parse_number(JsonParser* p, SeaJsonValue* out) {
    u32 start = p->pos;

    /* Optional minus */
    if (peek(p) == '-') p->pos++;

    /* Digits */
    if (at_end(p) || p->src[p->pos] < '0' || p->src[p->pos] > '9') {
        return SEA_ERR_INVALID_JSON;
    }
    while (!at_end(p) && p->src[p->pos] >= '0' && p->src[p->pos] <= '9') {
        p->pos++;
    }

    /* Decimal */
    if (!at_end(p) && p->src[p->pos] == '.') {
        p->pos++;
        while (!at_end(p) && p->src[p->pos] >= '0' && p->src[p->pos] <= '9') {
            p->pos++;
        }
    }

    /* Exponent */
    if (!at_end(p) && (p->src[p->pos] == 'e' || p->src[p->pos] == 'E')) {
        p->pos++;
        if (!at_end(p) && (p->src[p->pos] == '+' || p->src[p->pos] == '-')) {
            p->pos++;
        }
        while (!at_end(p) && p->src[p->pos] >= '0' && p->src[p->pos] <= '9') {
            p->pos++;
        }
    }

    out->type = SEA_JSON_NUMBER;
    out->raw.data = p->src + start;
    out->raw.len  = p->pos - start;

    /* Convert to f64 — use a temp null-terminated copy on stack */
    char buf[64];
    u32 nlen = out->raw.len;
    if (nlen >= sizeof(buf)) nlen = sizeof(buf) - 1;
    memcpy(buf, out->raw.data, nlen);
    buf[nlen] = '\0';
    out->number = strtod(buf, NULL);

    return SEA_OK;
}

/* ── Parse literal (true/false/null) ──────────────────────── */

static SeaError parse_literal(JsonParser* p, const char* lit, u32 litlen, SeaJsonValue* out) {
    if (p->pos + litlen > p->len) return SEA_ERR_INVALID_JSON;
    if (memcmp(p->src + p->pos, lit, litlen) != 0) return SEA_ERR_INVALID_JSON;

    out->raw.data = p->src + p->pos;
    out->raw.len  = litlen;
    p->pos += litlen;
    return SEA_OK;
}

/* ── Parse array ──────────────────────────────────────────── */

static SeaError parse_array(JsonParser* p, SeaJsonValue* out) {
    p->pos++; /* skip '[' */
    p->depth++;
    if (p->depth > SEA_MAX_JSON_DEPTH) return SEA_ERR_INVALID_JSON;

    out->type = SEA_JSON_ARRAY;
    out->array.items = NULL;
    out->array.count = 0;

    /* Count pass: we'll collect into a temp list in the arena */
    /* For simplicity, pre-allocate a reasonable max and shrink */
    u32 cap = 16;
    SeaJsonValue* items = (SeaJsonValue*)sea_arena_alloc(p->arena, cap * sizeof(SeaJsonValue), 8);
    if (!items) return SEA_ERR_ARENA_FULL;

    skip_whitespace(p);
    if (peek(p) == ']') {
        p->pos++;
        p->depth--;
        out->array.items = items;
        out->array.count = 0;
        return SEA_OK;
    }

    u32 count = 0;
    while (1) {
        if (count >= cap) {
            /* Grow — allocate new block (old one is wasted but arena resets anyway) */
            u32 new_cap = cap * 2;
            SeaJsonValue* new_items = (SeaJsonValue*)sea_arena_alloc(p->arena, new_cap * sizeof(SeaJsonValue), 8);
            if (!new_items) return SEA_ERR_ARENA_FULL;
            memcpy(new_items, items, count * sizeof(SeaJsonValue));
            items = new_items;
            cap = new_cap;
        }

        skip_whitespace(p);
        SeaError err = parse_value(p, &items[count]);
        if (err != SEA_OK) return err;
        count++;

        skip_whitespace(p);
        if (peek(p) == ',') {
            p->pos++;
        } else {
            break;
        }
    }

    if (!expect(p, ']')) return SEA_ERR_INVALID_JSON;

    out->array.items = items;
    out->array.count = count;
    p->depth--;
    return SEA_OK;
}

/* ── Parse object ─────────────────────────────────────────── */

static SeaError parse_object(JsonParser* p, SeaJsonValue* out) {
    p->pos++; /* skip '{' */
    p->depth++;
    if (p->depth > SEA_MAX_JSON_DEPTH) return SEA_ERR_INVALID_JSON;

    out->type = SEA_JSON_OBJECT;
    out->object.keys   = NULL;
    out->object.values = NULL;
    out->object.count  = 0;

    u32 cap = 16;
    SeaSlice* keys = (SeaSlice*)sea_arena_alloc(p->arena, cap * sizeof(SeaSlice), 8);
    SeaJsonValue* vals = (SeaJsonValue*)sea_arena_alloc(p->arena, cap * sizeof(SeaJsonValue), 8);
    if (!keys || !vals) return SEA_ERR_ARENA_FULL;

    skip_whitespace(p);
    if (peek(p) == '}') {
        p->pos++;
        p->depth--;
        out->object.keys   = keys;
        out->object.values = vals;
        out->object.count  = 0;
        return SEA_OK;
    }

    u32 count = 0;
    while (1) {
        if (count >= cap) {
            u32 new_cap = cap * 2;
            SeaSlice* new_keys = (SeaSlice*)sea_arena_alloc(p->arena, new_cap * sizeof(SeaSlice), 8);
            SeaJsonValue* new_vals = (SeaJsonValue*)sea_arena_alloc(p->arena, new_cap * sizeof(SeaJsonValue), 8);
            if (!new_keys || !new_vals) return SEA_ERR_ARENA_FULL;
            memcpy(new_keys, keys, count * sizeof(SeaSlice));
            memcpy(new_vals, vals, count * sizeof(SeaJsonValue));
            keys = new_keys;
            vals = new_vals;
            cap = new_cap;
        }

        skip_whitespace(p);
        SeaError err = parse_string(p, &keys[count]);
        if (err != SEA_OK) return err;

        skip_whitespace(p);
        if (!expect(p, ':')) return SEA_ERR_INVALID_JSON;

        skip_whitespace(p);
        err = parse_value(p, &vals[count]);
        if (err != SEA_OK) return err;
        count++;

        skip_whitespace(p);
        if (peek(p) == ',') {
            p->pos++;
        } else {
            break;
        }
    }

    if (!expect(p, '}')) return SEA_ERR_INVALID_JSON;

    out->object.keys   = keys;
    out->object.values = vals;
    out->object.count  = count;
    p->depth--;
    return SEA_OK;
}

/* ── Parse any value ──────────────────────────────────────── */

static SeaError parse_value(JsonParser* p, SeaJsonValue* out) {
    skip_whitespace(p);
    if (at_end(p)) return SEA_ERR_INVALID_JSON;

    u32 start = p->pos;
    u8 c = peek(p);

    if (c == '"') {
        out->type = SEA_JSON_STRING;
        SeaError err = parse_string(p, &out->string);
        if (err != SEA_OK) return err;
        out->raw.data = p->src + start;
        out->raw.len  = p->pos - start;
        return SEA_OK;
    }

    if (c == '{') return parse_object(p, out);
    if (c == '[') return parse_array(p, out);

    if (c == 't') {
        SeaError err = parse_literal(p, "true", 4, out);
        if (err != SEA_OK) return err;
        out->type = SEA_JSON_BOOL;
        out->boolean = true;
        return SEA_OK;
    }

    if (c == 'f') {
        SeaError err = parse_literal(p, "false", 5, out);
        if (err != SEA_OK) return err;
        out->type = SEA_JSON_BOOL;
        out->boolean = false;
        return SEA_OK;
    }

    if (c == 'n') {
        SeaError err = parse_literal(p, "null", 4, out);
        if (err != SEA_OK) return err;
        out->type = SEA_JSON_NULL;
        return SEA_OK;
    }

    if (c == '-' || (c >= '0' && c <= '9')) {
        return parse_number(p, out);
    }

    return SEA_ERR_INVALID_JSON;
}

/* ── Public API ───────────────────────────────────────────── */

SeaError sea_json_parse(SeaSlice input, SeaArena* arena, SeaJsonValue* out) {
    if (!input.data || input.len == 0 || !arena || !out) {
        return SEA_ERR_INVALID_JSON;
    }

    JsonParser p = {
        .src   = input.data,
        .len   = input.len,
        .pos   = 0,
        .arena = arena,
        .depth = 0,
    };

    memset(out, 0, sizeof(*out));
    SeaError err = parse_value(&p, out);
    if (err != SEA_OK) return err;

    /* Ensure no trailing garbage (except whitespace) */
    skip_whitespace(&p);
    if (!at_end(&p)) return SEA_ERR_INVALID_JSON;

    return SEA_OK;
}

const SeaJsonValue* sea_json_get(const SeaJsonValue* obj, const char* key) {
    if (!obj || obj->type != SEA_JSON_OBJECT || !key) return NULL;

    u32 klen = (u32)strlen(key);
    for (u32 i = 0; i < obj->object.count; i++) {
        if (obj->object.keys[i].len == klen &&
            memcmp(obj->object.keys[i].data, key, klen) == 0) {
            return &obj->object.values[i];
        }
    }
    return NULL;
}

SeaSlice sea_json_get_string(const SeaJsonValue* obj, const char* key) {
    const SeaJsonValue* v = sea_json_get(obj, key);
    if (!v || v->type != SEA_JSON_STRING) return SEA_SLICE_EMPTY;
    return v->string;
}

f64 sea_json_get_number(const SeaJsonValue* obj, const char* key, f64 fallback) {
    const SeaJsonValue* v = sea_json_get(obj, key);
    if (!v || v->type != SEA_JSON_NUMBER) return fallback;
    return v->number;
}

bool sea_json_get_bool(const SeaJsonValue* obj, const char* key, bool fallback) {
    const SeaJsonValue* v = sea_json_get(obj, key);
    if (!v || v->type != SEA_JSON_BOOL) return fallback;
    return v->boolean;
}

const SeaJsonValue* sea_json_array_get(const SeaJsonValue* arr, u32 index) {
    if (!arr || arr->type != SEA_JSON_ARRAY) return NULL;
    if (index >= arr->array.count) return NULL;
    return &arr->array.items[index];
}

void sea_json_debug_print(const SeaJsonValue* val, int indent) {
    if (!val) { printf("(null)\n"); return; }

    switch (val->type) {
        case SEA_JSON_NULL:
            printf("null\n");
            break;
        case SEA_JSON_BOOL:
            printf("%s\n", val->boolean ? "true" : "false");
            break;
        case SEA_JSON_NUMBER:
            printf("%g\n", val->number);
            break;
        case SEA_JSON_STRING:
            printf("\"%.*s\"\n", (int)val->string.len, (const char*)val->string.data);
            break;
        case SEA_JSON_ARRAY:
            printf("[\n");
            for (u32 i = 0; i < val->array.count; i++) {
                printf("%*s[%u] ", indent + 2, "", i);
                sea_json_debug_print(&val->array.items[i], indent + 2);
            }
            printf("%*s]\n", indent, "");
            break;
        case SEA_JSON_OBJECT:
            printf("{\n");
            for (u32 i = 0; i < val->object.count; i++) {
                printf("%*s\"%.*s\": ", indent + 2, "",
                       (int)val->object.keys[i].len,
                       (const char*)val->object.keys[i].data);
                sea_json_debug_print(&val->object.values[i], indent + 2);
            }
            printf("%*s}\n", indent, "");
            break;
    }
}
