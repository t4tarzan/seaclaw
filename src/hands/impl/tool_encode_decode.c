/*
 * tool_encode_decode.c — URL encode/decode, HTML entity encode/decode
 *
 * Args: <urlencode|urldecode|htmlencode|htmldecode> <text>
 * Returns: encoded/decoded text
 */

#include "seaclaw/sea_tools.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static u32 url_encode(const u8* src, u32 slen, char* dst, u32 dlen) {
    u32 o = 0;
    for (u32 i = 0; i < slen && o + 3 < dlen; i++) {
        char c = (char)src[i];
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
            dst[o++] = c;
        } else {
            o += (u32)snprintf(dst + o, dlen - o, "%%%02X", (unsigned char)c);
        }
    }
    dst[o] = '\0';
    return o;
}

static u32 url_decode(const char* src, u32 slen, char* dst, u32 dlen) {
    u32 o = 0;
    for (u32 i = 0; i < slen && o < dlen - 1; i++) {
        if (src[i] == '%' && i + 2 < slen) {
            char hex[3] = { src[i+1], src[i+2], '\0' };
            dst[o++] = (char)strtol(hex, NULL, 16);
            i += 2;
        } else if (src[i] == '+') {
            dst[o++] = ' ';
        } else {
            dst[o++] = src[i];
        }
    }
    dst[o] = '\0';
    return o;
}

static u32 html_encode(const u8* src, u32 slen, char* dst, u32 dlen) {
    u32 o = 0;
    for (u32 i = 0; i < slen && o + 6 < dlen; i++) {
        switch (src[i]) {
            case '&':  o += (u32)snprintf(dst + o, dlen - o, "&amp;"); break;
            case '<':  o += (u32)snprintf(dst + o, dlen - o, "&lt;"); break;
            case '>':  o += (u32)snprintf(dst + o, dlen - o, "&gt;"); break;
            case '"':  o += (u32)snprintf(dst + o, dlen - o, "&quot;"); break;
            case '\'': o += (u32)snprintf(dst + o, dlen - o, "&#39;"); break;
            default:   dst[o++] = (char)src[i]; break;
        }
    }
    dst[o] = '\0';
    return o;
}

static u32 html_decode(const char* src, u32 slen, char* dst, u32 dlen) {
    u32 o = 0;
    for (u32 i = 0; i < slen && o < dlen - 1; i++) {
        if (src[i] == '&') {
            if (i + 3 < slen && memcmp(src + i, "&lt;", 4) == 0) { dst[o++] = '<'; i += 3; }
            else if (i + 3 < slen && memcmp(src + i, "&gt;", 4) == 0) { dst[o++] = '>'; i += 3; }
            else if (i + 4 < slen && memcmp(src + i, "&amp;", 5) == 0) { dst[o++] = '&'; i += 4; }
            else if (i + 5 < slen && memcmp(src + i, "&quot;", 6) == 0) { dst[o++] = '"'; i += 5; }
            else if (i + 4 < slen && memcmp(src + i, "&#39;", 5) == 0) { dst[o++] = '\''; i += 4; }
            else dst[o++] = src[i];
        } else {
            dst[o++] = src[i];
        }
    }
    dst[o] = '\0';
    return o;
}

SeaError tool_encode_decode(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Usage: <urlencode|urldecode|htmlencode|htmldecode> <text>");
        return SEA_OK;
    }

    char op[16];
    u32 i = 0;
    while (i < args.len && i < sizeof(op) - 1 && args.data[i] != ' ') {
        op[i] = (char)args.data[i]; i++;
    }
    op[i] = '\0';
    while (i < args.len && args.data[i] == ' ') i++;

    const u8* text = args.data + i;
    u32 tlen = args.len - i;

    if (tlen == 0) {
        *output = SEA_SLICE_LIT("Error: no text provided");
        return SEA_OK;
    }

    u32 cap = tlen * 6 + 64;
    char* buf = (char*)sea_arena_alloc(arena, cap, 1);
    if (!buf) return SEA_ERR_ARENA_FULL;

    u32 rlen;
    if (strcmp(op, "urlencode") == 0) {
        rlen = url_encode(text, tlen, buf, cap);
    } else if (strcmp(op, "urldecode") == 0) {
        rlen = url_decode((const char*)text, tlen, buf, cap);
    } else if (strcmp(op, "htmlencode") == 0) {
        rlen = html_encode(text, tlen, buf, cap);
    } else if (strcmp(op, "htmldecode") == 0) {
        rlen = html_decode((const char*)text, tlen, buf, cap);
    } else {
        rlen = (u32)snprintf(buf, cap,
            "Unknown operation: %s\nAvailable: urlencode, urldecode, htmlencode, htmldecode", op);
    }

    output->data = (const u8*)buf;
    output->len  = rlen;
    return SEA_OK;
}
