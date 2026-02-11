/*
 * tool_string_replace.c — Find and replace text
 *
 * Tool ID:    38
 * Category:   Text Processing
 * Args:       <find>|||<replace>|||<text>  (separated by |||)
 * Returns:    Text with all occurrences replaced, plus count
 *
 * Examples:
 *   /exec string_replace foo|||bar|||"foo is great, foo is fun"
 *   /exec string_replace http|||https|||"visit http://example.com"
 *
 * Security: Input validated by standard tool pipeline.
 */

#include "seaclaw/sea_tools.h"
#include <stdio.h>
#include <string.h>

#define MAX_OUTPUT 8192

SeaError tool_string_replace(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Usage: <find>|||<replace>|||<text>");
        return SEA_OK;
    }

    char* input = (char*)sea_arena_alloc(arena, args.len + 1, 1);
    if (!input) return SEA_ERR_ARENA_FULL;
    memcpy(input, args.data, args.len);
    input[args.len] = '\0';

    /* Split on ||| */
    char* sep1 = strstr(input, "|||");
    if (!sep1) { *output = SEA_SLICE_LIT("Error: use ||| to separate find, replace, and text"); return SEA_OK; }
    *sep1 = '\0';
    char* replace = sep1 + 3;

    char* sep2 = strstr(replace, "|||");
    if (!sep2) { *output = SEA_SLICE_LIT("Error: need three parts: find|||replace|||text"); return SEA_OK; }
    *sep2 = '\0';
    char* text = sep2 + 3;

    char* find = input;
    u32 flen = (u32)strlen(find);
    u32 rlen = (u32)strlen(replace);
    u32 tlen = (u32)strlen(text);

    if (flen == 0) { *output = SEA_SLICE_LIT("Error: find string cannot be empty"); return SEA_OK; }

    char* buf = (char*)sea_arena_alloc(arena, MAX_OUTPUT, 1);
    if (!buf) return SEA_ERR_ARENA_FULL;

    u32 pos = 0, count = 0;
    const char* cursor = text;

    while (*cursor && pos < MAX_OUTPUT - rlen - 64) {
        const char* match = strstr(cursor, find);
        if (!match) {
            u32 remaining = tlen - (u32)(cursor - text);
            if (pos + remaining >= MAX_OUTPUT) remaining = MAX_OUTPUT - pos - 1;
            memcpy(buf + pos, cursor, remaining);
            pos += remaining;
            break;
        }
        /* Copy text before match */
        u32 before = (u32)(match - cursor);
        if (pos + before >= MAX_OUTPUT) break;
        memcpy(buf + pos, cursor, before);
        pos += before;
        /* Copy replacement */
        if (pos + rlen >= MAX_OUTPUT) break;
        memcpy(buf + pos, replace, rlen);
        pos += rlen;
        cursor = match + flen;
        count++;
    }
    buf[pos] = '\0';

    /* Append count */
    int extra = snprintf(buf + pos, MAX_OUTPUT - pos, "\n\n(%u replacements)", count);
    pos += (u32)extra;

    output->data = (const u8*)buf;
    output->len  = pos;
    return SEA_OK;
}
