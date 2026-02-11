/*
 * tool_head_tail.c — Show first or last N lines of text/file
 *
 * Tool ID:    30
 * Category:   Text Processing
 * Args:       <head|tail> [N] <filepath_or_text>
 * Returns:    First or last N lines (default 10)
 *
 * Examples:
 *   /exec head_tail head 5 /root/seaclaw/src/main.c
 *   /exec head_tail tail 20 /var/log/syslog
 *   /exec head_tail head 3 "line1\nline2\nline3\nline4\nline5"
 *
 * Security: File paths validated by Shield.
 */

#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_shield.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_OUTPUT 8192

SeaError tool_head_tail(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Usage: <head|tail> [N] <filepath_or_text>");
        return SEA_OK;
    }

    char input[4096];
    u32 ilen = args.len < sizeof(input) - 1 ? args.len : (u32)(sizeof(input) - 1);
    memcpy(input, args.data, ilen);
    input[ilen] = '\0';

    char* p = input;
    while (*p == ' ') p++;

    /* Parse mode */
    char mode[8];
    u32 mi = 0;
    while (*p && *p != ' ' && mi < sizeof(mode) - 1) mode[mi++] = *p++;
    mode[mi] = '\0';
    while (*p == ' ') p++;

    bool is_head = (strcmp(mode, "head") == 0);
    bool is_tail = (strcmp(mode, "tail") == 0);
    if (!is_head && !is_tail) {
        *output = SEA_SLICE_LIT("Error: first arg must be 'head' or 'tail'");
        return SEA_OK;
    }

    /* Parse optional N */
    u32 n = 10;
    if (isdigit((unsigned char)*p)) {
        n = (u32)atoi(p);
        if (n == 0) n = 10;
        if (n > 500) n = 500;
        while (isdigit((unsigned char)*p)) p++;
        while (*p == ' ') p++;
    }

    if (*p == '\0') {
        *output = SEA_SLICE_LIT("Error: no file path or text provided");
        return SEA_OK;
    }

    /* Load data */
    char* data = NULL;
    u32 data_len = 0;

    if (*p == '/' || (*p == '.' && *(p+1) == '/')) {
        SeaSlice ps = { .data = (const u8*)p, .len = (u32)strlen(p) };
        if (!sea_shield_detect_injection(ps)) {
            FILE* fp = fopen(p, "r");
            if (fp) {
                fseek(fp, 0, SEEK_END);
                long sz = ftell(fp);
                fseek(fp, 0, SEEK_SET);
                if (sz > 0 && sz < 262144) {
                    data = (char*)sea_arena_alloc(arena, (u32)sz + 1, 1);
                    if (data) { data_len = (u32)fread(data, 1, (size_t)sz, fp); data[data_len] = '\0'; }
                }
                fclose(fp);
            }
        }
    }

    if (!data) {
        /* Inline text — unescape \n */
        data = (char*)sea_arena_alloc(arena, strlen(p) + 1, 1);
        if (!data) return SEA_ERR_ARENA_FULL;
        u32 di = 0;
        for (u32 j = 0; p[j]; j++) {
            if (p[j] == '\\' && p[j+1] == 'n') { data[di++] = '\n'; j++; }
            else data[di++] = p[j];
        }
        data[di] = '\0';
        data_len = di;
    }

    /* Split into lines */
    char** lines = (char**)sea_arena_alloc(arena, sizeof(char*) * 1024, 8);
    if (!lines) return SEA_ERR_ARENA_FULL;
    u32 line_count = 0;
    char* tok = data;
    while (tok && line_count < 1024) {
        lines[line_count++] = tok;
        char* nl = strchr(tok, '\n');
        if (nl) { *nl = '\0'; tok = nl + 1; } else break;
    }

    char* buf = (char*)sea_arena_alloc(arena, MAX_OUTPUT, 1);
    if (!buf) return SEA_ERR_ARENA_FULL;
    int pos = 0;

    u32 start, end_idx;
    if (is_head) {
        start = 0;
        end_idx = n < line_count ? n : line_count;
        pos = snprintf(buf, MAX_OUTPUT, "=== head %u (of %u lines) ===\n", n, line_count);
    } else {
        start = line_count > n ? line_count - n : 0;
        end_idx = line_count;
        pos = snprintf(buf, MAX_OUTPUT, "=== tail %u (of %u lines) ===\n", n, line_count);
    }

    for (u32 i = start; i < end_idx && pos < MAX_OUTPUT - 256; i++) {
        pos += snprintf(buf + pos, (size_t)(MAX_OUTPUT - pos), "%4u  %s\n", i + 1, lines[i]);
    }

    output->data = (const u8*)buf;
    output->len  = (u32)pos;
    return SEA_OK;
}
