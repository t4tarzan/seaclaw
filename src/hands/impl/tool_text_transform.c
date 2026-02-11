/*
 * tool_text_transform.c — Text transformations
 *
 * Args: <operation> <text>
 * Operations: upper, lower, reverse, length, trim, base64enc, base64dec
 */

#include "seaclaw/sea_tools.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static u32 base64_encode(const u8* src, u32 slen, char* dst, u32 dlen) {
    u32 i = 0, o = 0;
    while (i < slen && o + 4 < dlen) {
        u32 a = src[i++];
        u32 b = (i < slen) ? src[i++] : 0;
        u32 c = (i < slen) ? src[i++] : 0;
        u32 triple = (a << 16) | (b << 8) | c;
        dst[o++] = b64_table[(triple >> 18) & 0x3F];
        dst[o++] = b64_table[(triple >> 12) & 0x3F];
        dst[o++] = (i > slen + 1) ? '=' : b64_table[(triple >> 6) & 0x3F];
        dst[o++] = (i > slen) ? '=' : b64_table[triple & 0x3F];
    }
    dst[o] = '\0';
    return o;
}

static i32 b64_val(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static u32 base64_decode(const char* src, u32 slen, u8* dst, u32 dlen) {
    u32 i = 0, o = 0;
    while (i < slen && o < dlen) {
        i32 a = b64_val(src[i++]);
        i32 b = (i < slen) ? b64_val(src[i++]) : 0;
        i32 c = (i < slen) ? b64_val(src[i++]) : 0;
        i32 d = (i < slen) ? b64_val(src[i++]) : 0;
        if (a < 0) break;
        u32 triple = ((u32)a << 18) | ((u32)(b > 0 ? b : 0) << 12) |
                     ((u32)(c > 0 ? c : 0) << 6) | (u32)(d > 0 ? d : 0);
        if (o < dlen) dst[o++] = (u8)((triple >> 16) & 0xFF);
        if (b >= 0 && o < dlen) dst[o++] = (u8)((triple >> 8) & 0xFF);
        if (c >= 0 && o < dlen) dst[o++] = (u8)(triple & 0xFF);
    }
    return o;
}

SeaError tool_text_transform(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Usage: <upper|lower|reverse|length|trim|base64enc|base64dec> <text>");
        return SEA_OK;
    }

    /* Parse operation */
    char op[16];
    u32 i = 0;
    while (i < args.len && i < sizeof(op) - 1 && args.data[i] != ' ') {
        op[i] = (char)args.data[i]; i++;
    }
    op[i] = '\0';
    while (i < args.len && args.data[i] == ' ') i++;

    const u8* text = args.data + i;
    u32 tlen = args.len - i;

    if (tlen == 0 && strcmp(op, "length") != 0) {
        *output = SEA_SLICE_LIT("Error: no text provided after operation");
        return SEA_OK;
    }

    char* buf = (char*)sea_arena_alloc(arena, tlen * 2 + 64, 1);
    if (!buf) return SEA_ERR_ARENA_FULL;

    if (strcmp(op, "upper") == 0) {
        for (u32 j = 0; j < tlen; j++) buf[j] = (char)toupper((unsigned char)text[j]);
        output->data = (const u8*)buf; output->len = tlen;
    } else if (strcmp(op, "lower") == 0) {
        for (u32 j = 0; j < tlen; j++) buf[j] = (char)tolower((unsigned char)text[j]);
        output->data = (const u8*)buf; output->len = tlen;
    } else if (strcmp(op, "reverse") == 0) {
        for (u32 j = 0; j < tlen; j++) buf[j] = (char)text[tlen - 1 - j];
        output->data = (const u8*)buf; output->len = tlen;
    } else if (strcmp(op, "length") == 0) {
        int n = snprintf(buf, tlen * 2 + 64, "%u", tlen);
        output->data = (const u8*)buf; output->len = (u32)n;
    } else if (strcmp(op, "trim") == 0) {
        const u8* s = text; const u8* e = text + tlen - 1;
        while (s <= e && isspace(*s)) s++;
        while (e >= s && isspace(*e)) e--;
        u32 rlen = (u32)(e - s + 1);
        memcpy(buf, s, rlen);
        output->data = (const u8*)buf; output->len = rlen;
    } else if (strcmp(op, "base64enc") == 0) {
        u32 elen = base64_encode(text, tlen, buf, tlen * 2 + 64);
        output->data = (const u8*)buf; output->len = elen;
    } else if (strcmp(op, "base64dec") == 0) {
        u32 dlen = base64_decode((const char*)text, tlen, (u8*)buf, tlen * 2 + 64);
        output->data = (const u8*)buf; output->len = dlen;
    } else {
        int n = snprintf(buf, tlen * 2 + 64,
            "Unknown operation: %s\nAvailable: upper, lower, reverse, length, trim, base64enc, base64dec", op);
        output->data = (const u8*)buf; output->len = (u32)n;
    }
    return SEA_OK;
}
