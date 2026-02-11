/*
 * tool_math_eval.c — Basic arithmetic expression evaluator
 *
 * Args: arithmetic expression (e.g. "2 + 3 * 4")
 * Returns: result
 *
 * Supports: +, -, *, /, %, parentheses, integers and decimals
 */

#include "seaclaw/sea_tools.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

typedef struct { const char* s; u32 pos; u32 len; } MathCtx;

static void skip_ws(MathCtx* c) { while (c->pos < c->len && c->s[c->pos] == ' ') c->pos++; }

static f64 parse_number(MathCtx* c) {
    skip_ws(c);
    f64 sign = 1.0;
    if (c->pos < c->len && c->s[c->pos] == '-') { sign = -1.0; c->pos++; }
    f64 val = 0;
    while (c->pos < c->len && isdigit((unsigned char)c->s[c->pos]))
        val = val * 10 + (c->s[c->pos++] - '0');
    if (c->pos < c->len && c->s[c->pos] == '.') {
        c->pos++;
        f64 frac = 0.1;
        while (c->pos < c->len && isdigit((unsigned char)c->s[c->pos])) {
            val += (c->s[c->pos++] - '0') * frac;
            frac *= 0.1;
        }
    }
    return sign * val;
}

static f64 parse_expr(MathCtx* c);

static f64 parse_atom(MathCtx* c) {
    skip_ws(c);
    if (c->pos < c->len && c->s[c->pos] == '(') {
        c->pos++;
        f64 val = parse_expr(c);
        skip_ws(c);
        if (c->pos < c->len && c->s[c->pos] == ')') c->pos++;
        return val;
    }
    /* Check for math functions */
    if (c->pos + 4 <= c->len && memcmp(c->s + c->pos, "sqrt", 4) == 0) {
        c->pos += 4; return sqrt(parse_atom(c));
    }
    if (c->pos + 3 <= c->len && memcmp(c->s + c->pos, "abs", 3) == 0) {
        c->pos += 3; return fabs(parse_atom(c));
    }
    return parse_number(c);
}

static f64 parse_factor(MathCtx* c) {
    f64 val = parse_atom(c);
    skip_ws(c);
    while (c->pos < c->len && c->s[c->pos] == '^') {
        c->pos++;
        val = pow(val, parse_atom(c));
        skip_ws(c);
    }
    return val;
}

static f64 parse_term(MathCtx* c) {
    f64 val = parse_factor(c);
    skip_ws(c);
    while (c->pos < c->len && (c->s[c->pos] == '*' || c->s[c->pos] == '/' || c->s[c->pos] == '%')) {
        char op = c->s[c->pos++];
        f64 right = parse_factor(c);
        if (op == '*') val *= right;
        else if (op == '/' && right != 0) val /= right;
        else if (op == '%' && right != 0) val = fmod(val, right);
        else return 0.0 / 0.0; /* NaN for div by zero */
        skip_ws(c);
    }
    return val;
}

static f64 parse_expr(MathCtx* c) {
    f64 val = parse_term(c);
    skip_ws(c);
    while (c->pos < c->len && (c->s[c->pos] == '+' || c->s[c->pos] == '-')) {
        char op = c->s[c->pos++];
        f64 right = parse_term(c);
        if (op == '+') val += right; else val -= right;
        skip_ws(c);
    }
    return val;
}

SeaError tool_math_eval(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Usage: <expression>\nExamples: 2+3*4, (10-3)/2, sqrt(144), 2^10");
        return SEA_OK;
    }

    char expr[512];
    u32 elen = args.len < sizeof(expr) - 1 ? args.len : (u32)(sizeof(expr) - 1);
    memcpy(expr, args.data, elen);
    expr[elen] = '\0';

    MathCtx ctx = { .s = expr, .pos = 0, .len = elen };
    f64 result = parse_expr(&ctx);

    char buf[128];
    int len;
    if (result == (i64)result && fabs(result) < 1e15)
        len = snprintf(buf, sizeof(buf), "%lld", (long long)(i64)result);
    else
        len = snprintf(buf, sizeof(buf), "%.10g", result);

    u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)len);
    if (!dst) return SEA_ERR_ARENA_FULL;
    output->data = dst; output->len = (u32)len;
    return SEA_OK;
}
