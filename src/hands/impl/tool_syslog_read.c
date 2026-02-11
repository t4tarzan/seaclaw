/*
 * tool_syslog_read.c — Read recent system log entries
 *
 * Tool ID:    35
 * Category:   System
 * Args:       [lines] [filter]  (default: 20 lines, no filter)
 * Returns:    Recent syslog/journal entries
 *
 * Examples:
 *   /exec syslog_read
 *   /exec syslog_read 50
 *   /exec syslog_read 30 error
 *
 * Security: Read-only. Filter validated by Shield.
 */

#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_shield.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_OUTPUT 8192

SeaError tool_syslog_read(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    u32 lines = 20;
    char filter[128] = "";

    if (args.len > 0) {
        char input[256];
        u32 ilen = args.len < sizeof(input) - 1 ? args.len : (u32)(sizeof(input) - 1);
        memcpy(input, args.data, ilen);
        input[ilen] = '\0';

        char* p = input;
        while (*p == ' ') p++;
        if (isdigit((unsigned char)*p)) {
            lines = (u32)atoi(p);
            if (lines == 0) lines = 20;
            if (lines > 200) lines = 200;
            while (isdigit((unsigned char)*p)) p++;
            while (*p == ' ') p++;
        }
        if (*p) {
            u32 flen = (u32)strlen(p);
            if (flen < sizeof(filter)) { memcpy(filter, p, flen); filter[flen] = '\0'; }
        }
    }

    if (filter[0]) {
        SeaSlice fs = { .data = (const u8*)filter, .len = (u32)strlen(filter) };
        if (sea_shield_detect_injection(fs)) {
            *output = SEA_SLICE_LIT("Error: filter rejected by Shield");
            return SEA_OK;
        }
    }

    char cmd[512];
    if (filter[0]) {
        snprintf(cmd, sizeof(cmd),
            "journalctl --no-pager -n %u 2>/dev/null | grep -i '%s' | tail -%u || "
            "tail -%u /var/log/syslog 2>/dev/null | grep -i '%s'",
            lines * 3, filter, lines, lines * 3, filter);
    } else {
        snprintf(cmd, sizeof(cmd),
            "journalctl --no-pager -n %u 2>/dev/null || tail -%u /var/log/syslog 2>/dev/null",
            lines, lines);
    }

    FILE* fp = popen(cmd, "r");
    if (!fp) {
        *output = SEA_SLICE_LIT("Error: cannot read system logs");
        return SEA_OK;
    }

    char* buf = (char*)sea_arena_alloc(arena, MAX_OUTPUT, 1);
    if (!buf) { pclose(fp); return SEA_ERR_ARENA_FULL; }

    u32 pos = 0;
    char line[512];
    while (fgets(line, sizeof(line), fp) && pos < MAX_OUTPUT - 512) {
        u32 llen = (u32)strlen(line);
        if (llen > 200) { llen = 200; line[200] = '\n'; line[201] = '\0'; llen = 201; }
        memcpy(buf + pos, line, llen);
        pos += llen;
    }
    pclose(fp);

    if (pos == 0) {
        *output = SEA_SLICE_LIT("No log entries found.");
        return SEA_OK;
    }

    output->data = (const u8*)buf;
    output->len  = pos;
    return SEA_OK;
}
