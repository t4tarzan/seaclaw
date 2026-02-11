/*
 * tool_net_info.c — Network interface and connectivity information
 *
 * Tool ID:    32
 * Category:   System / Network
 * Args:       [interfaces|ip|ping <host>|ports]
 * Returns:    Network information
 *
 * Examples:
 *   /exec net_info interfaces
 *   /exec net_info ip
 *   /exec net_info ping google.com
 *   /exec net_info ports
 *
 * Security: Ping target validated by Shield. Read-only operations.
 */

#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_shield.h"
#include <stdio.h>
#include <string.h>

#define MAX_OUTPUT 4096

SeaError tool_net_info(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    char op[32] = "ip";
    char arg[256] = "";

    if (args.len > 0) {
        char input[512];
        u32 ilen = args.len < sizeof(input) - 1 ? args.len : (u32)(sizeof(input) - 1);
        memcpy(input, args.data, ilen);
        input[ilen] = '\0';
        sscanf(input, "%31s %255s", op, arg);
    }

    char cmd[512];
    if (strcmp(op, "interfaces") == 0) {
        snprintf(cmd, sizeof(cmd), "ip -br addr 2>/dev/null || ifconfig -a 2>/dev/null | head -40");
    } else if (strcmp(op, "ip") == 0) {
        snprintf(cmd, sizeof(cmd),
            "echo 'Local:' && ip -4 addr show scope global 2>/dev/null | grep inet | awk '{print \"  \" $2}' && "
            "echo 'Public:' && curl -s --max-time 5 ifconfig.me 2>/dev/null || echo '  (unavailable)'");
    } else if (strcmp(op, "ping") == 0) {
        if (arg[0] == '\0') {
            *output = SEA_SLICE_LIT("Error: ping requires a hostname");
            return SEA_OK;
        }
        SeaSlice hs = { .data = (const u8*)arg, .len = (u32)strlen(arg) };
        if (sea_shield_detect_injection(hs)) {
            *output = SEA_SLICE_LIT("Error: hostname rejected by Shield");
            return SEA_OK;
        }
        snprintf(cmd, sizeof(cmd), "ping -c 3 -W 2 '%s' 2>&1 | tail -5", arg);
    } else if (strcmp(op, "ports") == 0) {
        snprintf(cmd, sizeof(cmd), "ss -tlnp 2>/dev/null | head -20 || netstat -tlnp 2>/dev/null | head -20");
    } else {
        *output = SEA_SLICE_LIT("Usage: <interfaces|ip|ping <host>|ports>");
        return SEA_OK;
    }

    FILE* fp = popen(cmd, "r");
    if (!fp) {
        *output = SEA_SLICE_LIT("Error: command execution failed");
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
