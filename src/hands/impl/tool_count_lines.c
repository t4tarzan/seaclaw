/*
 * tool_count_lines.c — Count lines of code in a directory
 *
 * Tool ID:    50
 * Category:   Development / Utility
 * Args:       [directory] [extension]
 * Returns:    Line counts per file and total, like cloc/sloccount
 *
 * Default: current directory, all .c and .h files
 *
 * Examples:
 *   /exec count_lines /root/seaclaw/src
 *   /exec count_lines /root/seaclaw/src .c
 *   /exec count_lines /root/seaclaw/include .h
 *
 * Security: Directory path validated by Shield. Read-only.
 */

#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_shield.h"
#include <stdio.h>
#include <string.h>

#define MAX_OUTPUT 8192

SeaError tool_count_lines(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    char dir[256] = ".";
    char ext[16] = "";

    if (args.len > 0) {
        char input[512];
        u32 ilen = args.len < sizeof(input) - 1 ? args.len : (u32)(sizeof(input) - 1);
        memcpy(input, args.data, ilen);
        input[ilen] = '\0';
        sscanf(input, "%255s %15s", dir, ext);
    }

    SeaSlice ds = { .data = (const u8*)dir, .len = (u32)strlen(dir) };
    if (sea_shield_detect_injection(ds)) {
        *output = SEA_SLICE_LIT("Error: directory rejected by Shield");
        return SEA_OK;
    }

    char cmd[512];
    if (ext[0]) {
        snprintf(cmd, sizeof(cmd),
            "find '%s' -name '*%s' -type f | xargs wc -l 2>/dev/null | sort -n | tail -30",
            dir, ext);
    } else {
        snprintf(cmd, sizeof(cmd),
            "find '%s' \\( -name '*.c' -o -name '*.h' \\) -type f | xargs wc -l 2>/dev/null | sort -n | tail -30",
            dir);
    }

    FILE* fp = popen(cmd, "r");
    if (!fp) {
        *output = SEA_SLICE_LIT("Error: line count failed");
        return SEA_OK;
    }

    char* buf = (char*)sea_arena_alloc(arena, MAX_OUTPUT, 1);
    if (!buf) { pclose(fp); return SEA_ERR_ARENA_FULL; }

    int pos = snprintf(buf, MAX_OUTPUT, "Lines of code in %s%s%s:\n",
                       dir, ext[0] ? " (*" : "", ext[0] ? ext : "");
    if (ext[0]) pos += snprintf(buf + pos, (size_t)(MAX_OUTPUT - pos), "):\n") - 2;

    char line[512];
    while (fgets(line, sizeof(line), fp) && pos < MAX_OUTPUT - 512) {
        u32 llen = (u32)strlen(line);
        memcpy(buf + pos, line, llen);
        pos += (int)llen;
    }
    pclose(fp);

    output->data = (const u8*)buf;
    output->len  = (u32)pos;
    return SEA_OK;
}
