/*
 * tool_ssl_check.c — Check SSL certificate for a domain
 *
 * Tool ID:    45
 * Category:   Network / Security
 * Args:       <domain>
 * Returns:    SSL certificate details (issuer, expiry, subject)
 *
 * Examples:
 *   /exec ssl_check google.com
 *   /exec ssl_check github.com
 *
 * Security: Domain validated by Shield. Read-only network query.
 */

#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_shield.h"
#include <stdio.h>
#include <string.h>

#define MAX_OUTPUT 4096

SeaError tool_ssl_check(SeaSlice args, SeaArena* arena, SeaSlice* output) {
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
    int cl = snprintf(cmd, sizeof(cmd),
        "echo | openssl s_client -servername '%s' -connect '%s:443' 2>/dev/null | "
        "openssl x509 -noout -subject -issuer -dates -serial 2>/dev/null",
        d, d);
    (void)cl;

    FILE* fp = popen(cmd, "r");
    if (!fp) {
        *output = SEA_SLICE_LIT("Error: SSL check failed");
        return SEA_OK;
    }

    char* buf = (char*)sea_arena_alloc(arena, MAX_OUTPUT, 1);
    if (!buf) { pclose(fp); return SEA_ERR_ARENA_FULL; }

    int pos = snprintf(buf, MAX_OUTPUT, "SSL Certificate: %s\n", d);
    char line[512];
    while (fgets(line, sizeof(line), fp) && pos < MAX_OUTPUT - 512) {
        u32 llen = (u32)strlen(line);
        pos += snprintf(buf + pos, (size_t)(MAX_OUTPUT - pos), "  %.*s", (int)llen, line);
    }
    pclose(fp);

    if (pos < 30) {
        pos = snprintf(buf, MAX_OUTPUT, "Error: could not retrieve SSL cert for '%s'", d);
    }

    output->data = (const u8*)buf;
    output->len  = (u32)pos;
    return SEA_OK;
}
