/*
 * tool_grep_text.c — Search for pattern in text, return matching lines
 *
 * Tool ID:    28
 * Category:   Text Processing
 * Args:       <pattern> <text_or_filepath>
 * Returns:    Matching lines with line numbers
 *
 * If the second argument is a valid file path, reads from file.
 * Otherwise treats it as inline text (with \n as line separators).
 *
 * Examples:
 *   /exec grep_text error "line1\nerror: bad\nline3\nerror: fail"
 *   /exec grep_text TODO /root/seaclaw/src/main.c
 *
 * Security: File paths validated by Shield. Pattern is plain substring match.
 */

#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_shield.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define MAX_OUTPUT 8192

static bool icase_contains(const char* haystack, const char* needle) {
    u32 nlen = (u32)strlen(needle);
    u32 hlen = (u32)strlen(haystack);
    if (nlen > hlen) return false;
    for (u32 i = 0; i <= hlen - nlen; i++) {
        bool match = true;
        for (u32 j = 0; j < nlen; j++) {
            if (tolower((unsigned char)haystack[i+j]) != tolower((unsigned char)needle[j])) {
                match = false; break;
            }
        }
        if (match) return true;
    }
    return false;
}

SeaError tool_grep_text(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Usage: <pattern> <text_or_filepath>");
        return SEA_OK;
    }

    char input[8192];
    u32 ilen = args.len < sizeof(input) - 1 ? args.len : (u32)(sizeof(input) - 1);
    memcpy(input, args.data, ilen);
    input[ilen] = '\0';

    /* Parse pattern */
    char pattern[256];
    u32 pi = 0;
    char* p = input;
    while (*p == ' ') p++;
    while (*p && *p != ' ' && pi < sizeof(pattern) - 1) pattern[pi++] = *p++;
    pattern[pi] = '\0';
    while (*p == ' ') p++;

    if (pi == 0 || *p == '\0') {
        *output = SEA_SLICE_LIT("Error: need both pattern and text/filepath");
        return SEA_OK;
    }

    /* Check if it's a file path */
    char* text_data = NULL;
    u32 text_len = 0;
    FILE* fp = NULL;

    if (*p == '/' || (*p == '.' && *(p+1) == '/')) {
        SeaSlice ps = { .data = (const u8*)p, .len = (u32)strlen(p) };
        if (!sea_shield_detect_injection(ps)) {
            fp = fopen(p, "r");
        }
    }

    if (fp) {
        text_data = (char*)sea_arena_alloc(arena, 32768, 1);
        if (!text_data) { fclose(fp); return SEA_ERR_ARENA_FULL; }
        text_len = (u32)fread(text_data, 1, 32767, fp);
        text_data[text_len] = '\0';
        fclose(fp);
    } else {
        /* Unescape \n in inline text */
        text_data = (char*)sea_arena_alloc(arena, strlen(p) + 1, 1);
        if (!text_data) return SEA_ERR_ARENA_FULL;
        u32 di = 0;
        for (u32 j = 0; p[j]; j++) {
            if (p[j] == '\\' && p[j+1] == 'n') { text_data[di++] = '\n'; j++; }
            else text_data[di++] = p[j];
        }
        text_data[di] = '\0';
        text_len = di;
    }

    char* buf = (char*)sea_arena_alloc(arena, MAX_OUTPUT, 1);
    if (!buf) return SEA_ERR_ARENA_FULL;

    int pos = snprintf(buf, MAX_OUTPUT, "grep \"%s\":\n", pattern);
    u32 line_num = 1, matches = 0;
    char* line = text_data;

    while (line && pos < MAX_OUTPUT - 256) {
        char* nl = strchr(line, '\n');
        if (nl) *nl = '\0';

        if (icase_contains(line, pattern)) {
            u32 llen = (u32)strlen(line);
            if (llen > 120) llen = 120;
            pos += snprintf(buf + pos, (size_t)(MAX_OUTPUT - pos),
                "  %4u: %.*s%s\n", line_num, (int)llen, line, strlen(line) > 120 ? "..." : "");
            matches++;
        }

        line_num++;
        line = nl ? nl + 1 : NULL;
    }

    pos += snprintf(buf + pos, (size_t)(MAX_OUTPUT - pos), "(%u matches in %u lines)", matches, line_num - 1);

    output->data = (const u8*)buf;
    output->len  = (u32)pos;
    return SEA_OK;
}
