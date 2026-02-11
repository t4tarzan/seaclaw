/*
 * tool_file_search.c — Search for files by name pattern
 *
 * Tool ID:    41
 * Category:   File I/O
 * Args:       <pattern> [directory]
 * Returns:    List of matching files with sizes
 *
 * Uses recursive directory traversal. Pattern is substring match.
 * Default directory is current working directory.
 *
 * Examples:
 *   /exec file_search .c /root/seaclaw/src
 *   /exec file_search config /root/seaclaw
 *   /exec file_search .log /var/log
 *
 * Security: Directory path validated by Shield. Read-only.
 */

#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_shield.h"
#include <stdio.h>
#include <string.h>

#define MAX_OUTPUT 8192

SeaError tool_file_search(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Usage: <pattern> [directory]");
        return SEA_OK;
    }

    char input[512];
    u32 ilen = args.len < sizeof(input) - 1 ? args.len : (u32)(sizeof(input) - 1);
    memcpy(input, args.data, ilen);
    input[ilen] = '\0';

    char pattern[128] = "", dir[256] = ".";
    sscanf(input, "%127s %255s", pattern, dir);

    if (pattern[0] == '\0') {
        *output = SEA_SLICE_LIT("Error: no pattern provided");
        return SEA_OK;
    }

    SeaSlice ds = { .data = (const u8*)dir, .len = (u32)strlen(dir) };
    if (sea_shield_detect_injection(ds)) {
        *output = SEA_SLICE_LIT("Error: directory rejected by Shield");
        return SEA_OK;
    }

    char cmd[512];
    int clen = snprintf(cmd, sizeof(cmd),
        "find '%s' -maxdepth 5 -name '*%s*' -type f -printf '%%10s  %%p\\n' 2>/dev/null | head -30",
        dir, pattern);
    (void)clen;

    FILE* fp = popen(cmd, "r");
    if (!fp) {
        *output = SEA_SLICE_LIT("Error: search failed");
        return SEA_OK;
    }

    char* buf = (char*)sea_arena_alloc(arena, MAX_OUTPUT, 1);
    if (!buf) { pclose(fp); return SEA_ERR_ARENA_FULL; }

    int pos = snprintf(buf, MAX_OUTPUT, "Search: '*%s*' in %s\n", pattern, dir);
    u32 count = 0;
    char line[512];

    while (fgets(line, sizeof(line), fp) && pos < MAX_OUTPUT - 512) {
        u32 llen = (u32)strlen(line);
        memcpy(buf + pos, line, llen);
        pos += (int)llen;
        count++;
    }
    pclose(fp);

    pos += snprintf(buf + pos, (size_t)(MAX_OUTPUT - pos), "(%u files found)", count);

    output->data = (const u8*)buf;
    output->len  = (u32)pos;
    return SEA_OK;
}
