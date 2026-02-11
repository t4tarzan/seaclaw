/*
 * tool_whois_lookup.c — Domain WHOIS lookup
 *
 * Tool ID:    44
 * Category:   Network
 * Args:       <domain>
 * Returns:    WHOIS registration data (registrar, dates, nameservers)
 *
 * Examples:
 *   /exec whois_lookup example.com
 *   /exec whois_lookup google.com
 *
 * Security: Domain validated by Shield. Read-only network query.
 */

#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_shield.h"
#include <stdio.h>
#include <string.h>

#define MAX_OUTPUT 4096

SeaError tool_whois_lookup(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Usage: <domain>");
        return SEA_OK;
    }

    char domain[256];
    u32 dlen = args.len < sizeof(domain) - 1 ? args.len : (u32)(sizeof(domain) - 1);
    memcpy(domain, args.data, dlen);
    domain[dlen] = '\0';
    char* d = domain;
    while (*d == ' ') d++;
    char* end = d + strlen(d) - 1;
    while (end > d && (*end == ' ' || *end == '\n')) *end-- = '\0';

    SeaSlice ds = { .data = (const u8*)d, .len = (u32)strlen(d) };
    if (sea_shield_detect_injection(ds)) {
        *output = SEA_SLICE_LIT("Error: domain rejected by Shield");
        return SEA_OK;
    }

    char cmd[512];
    snprintf(cmd, sizeof(cmd),
        "whois '%s' 2>/dev/null | grep -iE "
        "'(registrar|creation|expir|updated|name server|status|registrant)' | head -20",
        d);

    FILE* fp = popen(cmd, "r");
    if (!fp) {
        *output = SEA_SLICE_LIT("Error: whois command failed");
        return SEA_OK;
    }

    char* buf = (char*)sea_arena_alloc(arena, MAX_OUTPUT, 1);
    if (!buf) { pclose(fp); return SEA_ERR_ARENA_FULL; }

    int pos = snprintf(buf, MAX_OUTPUT, "WHOIS: %s\n", d);
    char line[256];
    while (fgets(line, sizeof(line), fp) && pos < MAX_OUTPUT - 256) {
        u32 llen = (u32)strlen(line);
        memcpy(buf + pos, line, llen);
        pos += (int)llen;
    }
    pclose(fp);

    if (pos < 20) {
        pos = snprintf(buf, MAX_OUTPUT, "No WHOIS data found for '%s' (whois may not be installed)", d);
    }

    output->data = (const u8*)buf;
    output->len  = (u32)pos;
    return SEA_OK;
}
