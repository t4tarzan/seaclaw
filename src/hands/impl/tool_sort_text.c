/*
 * tool_sort_text.c — Sort lines of text
 *
 * Tool ID:    31
 * Category:   Text Processing
 * Args:       [options] <text>
 * Options:    -r (reverse), -n (numeric), -u (unique)
 * Returns:    Sorted lines
 *
 * Examples:
 *   /exec sort_text "banana\napple\ncherry"
 *   /exec sort_text -r "3\n1\n2"
 *   /exec sort_text -n "10\n2\n30\n1"
 *   /exec sort_text -u "a\nb\na\nc\nb"
 *
 * Security: Input validated by standard tool pipeline.
 */

#include "seaclaw/sea_tools.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINES 500
#define MAX_OUTPUT 8192

static int cmp_str_asc(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}
static int cmp_str_desc(const void* a, const void* b) {
    return strcmp(*(const char**)b, *(const char**)a);
}
static int cmp_num_asc(const void* a, const void* b) {
    double da = atof(*(const char**)a), db = atof(*(const char**)b);
    return (da > db) - (da < db);
}
static int cmp_num_desc(const void* a, const void* b) {
    return cmp_num_asc(b, a);
}

SeaError tool_sort_text(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Usage: [-r] [-n] [-u] <text with \\n separators>");
        return SEA_OK;
    }

    char* input = (char*)sea_arena_alloc(arena, args.len + 1, 1);
    if (!input) return SEA_ERR_ARENA_FULL;
    memcpy(input, args.data, args.len);
    input[args.len] = '\0';

    /* Parse options */
    bool reverse = false, numeric = false, unique = false;
    char* p = input;
    while (*p == ' ') p++;
    while (*p == '-') {
        p++;
        while (*p && *p != ' ') {
            if (*p == 'r') reverse = true;
            else if (*p == 'n') numeric = true;
            else if (*p == 'u') unique = true;
            p++;
        }
        while (*p == ' ') p++;
    }

    /* Unescape \n */
    char* text = (char*)sea_arena_alloc(arena, strlen(p) + 1, 1);
    if (!text) return SEA_ERR_ARENA_FULL;
    u32 ti = 0;
    for (u32 i = 0; p[i]; i++) {
        if (p[i] == '\\' && p[i+1] == 'n') { text[ti++] = '\n'; i++; }
        else text[ti++] = p[i];
    }
    text[ti] = '\0';

    /* Split into lines */
    char* lines[MAX_LINES];
    u32 count = 0;
    char* tok = text;
    while (tok && count < MAX_LINES) {
        lines[count] = tok;
        char* nl = strchr(tok, '\n');
        if (nl) { *nl = '\0'; tok = nl + 1; } else tok = NULL;
        if (strlen(lines[count]) > 0) count++;
    }

    /* Sort */
    if (numeric)
        qsort(lines, count, sizeof(char*), reverse ? cmp_num_desc : cmp_num_asc);
    else
        qsort(lines, count, sizeof(char*), reverse ? cmp_str_desc : cmp_str_asc);

    /* Output */
    char* buf = (char*)sea_arena_alloc(arena, MAX_OUTPUT, 1);
    if (!buf) return SEA_ERR_ARENA_FULL;
    int pos = 0;
    const char* prev = NULL;

    for (u32 i = 0; i < count && pos < MAX_OUTPUT - 256; i++) {
        if (unique && prev && strcmp(prev, lines[i]) == 0) continue;
        pos += snprintf(buf + pos, (size_t)(MAX_OUTPUT - pos), "%s\n", lines[i]);
        prev = lines[i];
    }

    output->data = (const u8*)buf;
    output->len  = (u32)pos;
    return SEA_OK;
}
