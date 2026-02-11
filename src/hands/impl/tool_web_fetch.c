/*
 * tool_web_fetch.c — Fetch a URL and return its content
 *
 * Args: URL string
 * Returns: HTTP response body (truncated to 8KB)
 */

#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_http.h"
#include "seaclaw/sea_shield.h"
#include <stdio.h>
#include <string.h>

#define MAX_FETCH_SIZE (8 * 1024)

SeaError tool_web_fetch(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Error: no URL provided");
        return SEA_OK;
    }

    /* Null-terminate the URL */
    char url[2048];
    u32 ulen = args.len < sizeof(url) - 1 ? args.len : (u32)(sizeof(url) - 1);
    memcpy(url, args.data, ulen);
    url[ulen] = '\0';

    /* Strip whitespace */
    char* u = url;
    while (*u == ' ' || *u == '\t') u++;
    char* end = u + strlen(u) - 1;
    while (end > u && (*end == ' ' || *end == '\t' || *end == '\n')) *end-- = '\0';

    /* Shield: validate URL */
    SeaSlice url_slice = { .data = (const u8*)u, .len = (u32)strlen(u) };
    if (sea_shield_detect_injection(url_slice)) {
        *output = SEA_SLICE_LIT("Error: URL rejected by Shield");
        return SEA_OK;
    }

    /* Must start with http:// or https:// */
    if (strncmp(u, "http://", 7) != 0 && strncmp(u, "https://", 8) != 0) {
        *output = SEA_SLICE_LIT("Error: URL must start with http:// or https://");
        return SEA_OK;
    }

    SeaHttpResponse resp;
    SeaError err = sea_http_get(u, arena, &resp);
    if (err != SEA_OK) {
        *output = SEA_SLICE_LIT("Error: HTTP request failed");
        return SEA_OK;
    }

    if (resp.status_code != 200) {
        char msg[128];
        int mlen = snprintf(msg, sizeof(msg), "HTTP %d from %s", resp.status_code, u);
        u8* dst = (u8*)sea_arena_push_bytes(arena, msg, (u64)mlen);
        if (dst) { output->data = dst; output->len = (u32)mlen; }
        return SEA_OK;
    }

    /* Truncate if needed */
    if (resp.body.len > MAX_FETCH_SIZE) {
        resp.body.len = MAX_FETCH_SIZE;
    }

    *output = resp.body;
    return SEA_OK;
}
