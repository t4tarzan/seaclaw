/*
 * tool_weather.c — Get current weather for a location
 *
 * Tool ID:    47
 * Category:   Network / Utility
 * Args:       <city_name>
 * Returns:    Current weather conditions from wttr.in (no API key needed)
 *
 * Examples:
 *   /exec weather London
 *   /exec weather "New York"
 *   /exec weather Tokyo
 *
 * Security: City name validated by Shield. Uses free wttr.in API.
 */

#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_http.h"
#include "seaclaw/sea_shield.h"
#include <stdio.h>
#include <string.h>

SeaError tool_weather(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Usage: <city_name>");
        return SEA_OK;
    }

    char city[128];
    u32 clen = args.len < sizeof(city) - 1 ? args.len : (u32)(sizeof(city) - 1);
    memcpy(city, args.data, clen);
    city[clen] = '\0';
    char* c = city;
    while (*c == ' ') c++;
    char* end = c + strlen(c) - 1;
    while (end > c && (*end == ' ' || *end == '\n')) *end-- = '\0';

    SeaSlice cs = { .data = (const u8*)c, .len = (u32)strlen(c) };
    if (sea_shield_detect_injection(cs)) {
        *output = SEA_SLICE_LIT("Error: city name rejected by Shield");
        return SEA_OK;
    }

    /* URL-encode spaces */
    char encoded[128];
    u32 ei = 0;
    for (u32 i = 0; c[i] && ei < sizeof(encoded) - 4; i++) {
        if (c[i] == ' ') { encoded[ei++] = '+'; }
        else encoded[ei++] = c[i];
    }
    encoded[ei] = '\0';

    char url[256];
    int ul = snprintf(url, sizeof(url), "http://wttr.in/%s?format=%%l:+%%c+%%t+%%h+%%w+%%p", encoded);
    (void)ul;

    SeaHttpResponse resp;
    SeaError err = sea_http_get(url, arena, &resp);
    if (err != SEA_OK || resp.status_code != 200) {
        *output = SEA_SLICE_LIT("Error: weather lookup failed");
        return SEA_OK;
    }

    /* wttr.in returns a compact one-line format */
    char buf[512];
    int len = snprintf(buf, sizeof(buf), "Weather for %s:\n  %.*s",
                       c, (int)(resp.body.len > 300 ? 300 : resp.body.len),
                       (const char*)resp.body.data);

    u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)len);
    if (!dst) return SEA_ERR_ARENA_FULL;
    output->data = dst; output->len = (u32)len;
    return SEA_OK;
}
