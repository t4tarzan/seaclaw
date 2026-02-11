/*
 * tool_regex_match.c — Match regex pattern against text
 *
 * Tool ID:    25
 * Category:   Text Processing
 * Args:       <pattern> <text>
 * Returns:    All matches found, with positions
 *
 * Uses POSIX extended regex (ERE). No external dependencies.
 *
 * Examples:
 *   /exec regex_match [0-9]+ "There are 42 cats and 7 dogs"
 *   /exec regex_match https?://[^ ]+ "Visit https://example.com today"
 *
 * Security: Pattern is validated by Shield before compilation.
 */

#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_shield.h"
#include <stdio.h>
#include <string.h>
#include <regex.h>

#define MAX_MATCHES 20
#define MAX_OUTPUT  4096

SeaError tool_regex_match(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Usage: <pattern> <text>\nExample: [0-9]+ \"There are 42 cats\"");
        return SEA_OK;
    }

    /* Parse: first token is pattern, rest is text */
    char input[2048];
    u32 ilen = args.len < sizeof(input) - 1 ? args.len : (u32)(sizeof(input) - 1);
    memcpy(input, args.data, ilen);
    input[ilen] = '\0';

    char* p = input;
    while (*p == ' ') p++;

    /* Find end of pattern (space-delimited unless quoted) */
    char pattern[256];
    char* text;
    if (*p == '"' || *p == '\'') {
        char q = *p++;
        char* end = strchr(p, q);
        if (!end) { *output = SEA_SLICE_LIT("Error: unclosed quote in pattern"); return SEA_OK; }
        u32 plen = (u32)(end - p);
        if (plen >= sizeof(pattern)) plen = sizeof(pattern) - 1;
        memcpy(pattern, p, plen); pattern[plen] = '\0';
        text = end + 1;
    } else {
        u32 pi = 0;
        while (*p && *p != ' ' && pi < sizeof(pattern) - 1) pattern[pi++] = *p++;
        pattern[pi] = '\0';
        text = p;
    }
    while (*text == ' ') text++;

    if (strlen(pattern) == 0 || strlen(text) == 0) {
        *output = SEA_SLICE_LIT("Error: need both pattern and text");
        return SEA_OK;
    }

    /* Compile regex */
    regex_t reg;
    int rc = regcomp(&reg, pattern, REG_EXTENDED);
    if (rc != 0) {
        char errbuf[128];
        regerror(rc, &reg, errbuf, sizeof(errbuf));
        char buf[256];
        int len = snprintf(buf, sizeof(buf), "Regex error: %s", errbuf);
        u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)len);
        if (dst) { output->data = dst; output->len = (u32)len; }
        return SEA_OK;
    }

    /* Find all matches */
    char* buf = (char*)sea_arena_alloc(arena, MAX_OUTPUT, 1);
    if (!buf) { regfree(&reg); return SEA_ERR_ARENA_FULL; }

    int pos = snprintf(buf, MAX_OUTPUT, "Pattern: /%s/\n", pattern);
    regmatch_t match;
    const char* cursor = text;
    u32 count = 0;
    u32 offset = 0;

    while (count < MAX_MATCHES && regexec(&reg, cursor, 1, &match, 0) == 0) {
        u32 start = offset + (u32)match.rm_so;
        u32 end_pos = offset + (u32)match.rm_eo;
        u32 mlen = (u32)(match.rm_eo - match.rm_so);

        pos += snprintf(buf + pos, (size_t)(MAX_OUTPUT - pos),
            "  [%u] pos %u-%u: \"%.*s\"\n",
            count + 1, start, end_pos, (int)mlen, cursor + match.rm_so);

        offset += (u32)match.rm_eo;
        cursor += match.rm_eo;
        count++;
        if (match.rm_so == match.rm_eo) { cursor++; offset++; } /* avoid infinite loop on empty match */
    }

    regfree(&reg);

    if (count == 0) {
        pos += snprintf(buf + pos, (size_t)(MAX_OUTPUT - pos), "  No matches found.");
    } else {
        pos += snprintf(buf + pos, (size_t)(MAX_OUTPUT - pos), "(%u matches)", count);
    }

    output->data = (const u8*)buf;
    output->len  = (u32)pos;
    return SEA_OK;
}
