/*
 * tool_diff_text.c — Compare two texts and show differences
 *
 * Tool ID:    27
 * Category:   Text Processing
 * Args:       <text1>|||<text2>  (separated by |||)
 * Returns:    Line-by-line comparison showing additions/removals
 *
 * Examples:
 *   /exec diff_text "hello world"|||"hello earth"
 *   /exec diff_text "line1\nline2\nline3"|||"line1\nline2modified\nline3"
 *
 * Security: Input validated by standard tool pipeline.
 */

#include "seaclaw/sea_tools.h"
#include <stdio.h>
#include <string.h>

#define MAX_OUTPUT 8192
#define MAX_LINES  100

static u32 split_lines(char* text, char** lines, u32 max) {
    u32 count = 0;
    char* tok = text;
    while (tok && count < max) {
        lines[count] = tok;
        char* nl = strchr(tok, '\n');
        if (nl) { *nl = '\0'; tok = nl + 1; } else tok = NULL;
        count++;
    }
    return count;
}

SeaError tool_diff_text(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Usage: <text1>|||<text2>\nSeparate the two texts with |||");
        return SEA_OK;
    }

    char* input = (char*)sea_arena_alloc(arena, args.len + 1, 1);
    if (!input) return SEA_ERR_ARENA_FULL;
    memcpy(input, args.data, args.len);
    input[args.len] = '\0';

    /* Unescape \n */
    for (u32 i = 0; input[i]; i++) {
        if (input[i] == '\\' && input[i+1] == 'n') { input[i] = '\n'; memmove(input+i+1, input+i+2, strlen(input+i+2)+1); }
    }

    /* Split on ||| */
    char* sep = strstr(input, "|||");
    if (!sep) {
        *output = SEA_SLICE_LIT("Error: use ||| to separate the two texts");
        return SEA_OK;
    }
    *sep = '\0';
    char* text1 = input;
    char* text2 = sep + 3;

    char* lines1[MAX_LINES], *lines2[MAX_LINES];
    u32 n1 = split_lines(text1, lines1, MAX_LINES);
    u32 n2 = split_lines(text2, lines2, MAX_LINES);

    char* buf = (char*)sea_arena_alloc(arena, MAX_OUTPUT, 1);
    if (!buf) return SEA_ERR_ARENA_FULL;

    int pos = snprintf(buf, MAX_OUTPUT, "Diff (%u vs %u lines):\n", n1, n2);
    u32 changes = 0;
    u32 max = n1 > n2 ? n1 : n2;

    for (u32 i = 0; i < max && pos < MAX_OUTPUT - 256; i++) {
        const char* l1 = (i < n1) ? lines1[i] : NULL;
        const char* l2 = (i < n2) ? lines2[i] : NULL;

        if (l1 && l2 && strcmp(l1, l2) == 0) {
            pos += snprintf(buf + pos, (size_t)(MAX_OUTPUT - pos), "  %3u   %s\n", i + 1, l1);
        } else {
            if (l1) pos += snprintf(buf + pos, (size_t)(MAX_OUTPUT - pos), "- %3u   %s\n", i + 1, l1);
            if (l2) pos += snprintf(buf + pos, (size_t)(MAX_OUTPUT - pos), "+ %3u   %s\n", i + 1, l2);
            changes++;
        }
    }

    pos += snprintf(buf + pos, (size_t)(MAX_OUTPUT - pos), "(%u lines changed)", changes);

    output->data = (const u8*)buf;
    output->len  = (u32)pos;
    return SEA_OK;
}
