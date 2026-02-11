/*
 * tool_process_list.c — List running processes
 *
 * Args: optional filter string
 * Returns: top processes by CPU/memory
 */

#include "seaclaw/sea_tools.h"
#include <stdio.h>
#include <string.h>

#define MAX_OUTPUT 4096

SeaError tool_process_list(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    char cmd[256];
    if (args.len > 0 && args.len < 64) {
        char filter[128];
        u32 flen = args.len < sizeof(filter) - 1 ? args.len : (u32)(sizeof(filter) - 1);
        memcpy(filter, args.data, flen);
        filter[flen] = '\0';
        /* Trim */
        char* f = filter;
        while (*f == ' ') f++;
        snprintf(cmd, sizeof(cmd), "ps aux --sort=-pcpu | head -1; ps aux --sort=-pcpu | grep -i '%s' | head -15", f);
    } else {
        snprintf(cmd, sizeof(cmd), "ps aux --sort=-pcpu | head -16");
    }

    FILE* fp = popen(cmd, "r");
    if (!fp) {
        *output = SEA_SLICE_LIT("Error: cannot list processes");
        return SEA_OK;
    }

    char* buf = (char*)sea_arena_alloc(arena, MAX_OUTPUT, 1);
    if (!buf) { pclose(fp); return SEA_ERR_ARENA_FULL; }

    u32 pos = 0;
    char line[512];
    while (fgets(line, sizeof(line), fp) && pos < MAX_OUTPUT - 512) {
        u32 llen = (u32)strlen(line);
        memcpy(buf + pos, line, llen);
        pos += llen;
    }
    pclose(fp);

    output->data = (const u8*)buf;
    output->len  = pos;
    return SEA_OK;
}
