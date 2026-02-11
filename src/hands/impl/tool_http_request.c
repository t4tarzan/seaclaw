/*
 * tool_http_request.c — Make HTTP requests with custom method/headers
 *
 * Tool ID:    37
 * Category:   Network
 * Args:       <GET|POST|HEAD> <url> [body]
 * Returns:    HTTP status, headers summary, and response body
 *
 * More flexible than web_fetch — supports methods and shows status codes.
 *
 * Examples:
 *   /exec http_request GET https://httpbin.org/get
 *   /exec http_request HEAD https://example.com
 *   /exec http_request POST https://httpbin.org/post {"key":"value"}
 *
 * Security: URL validated by Shield. Body limited to 4KB.
 */

#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_http.h"
#include "seaclaw/sea_shield.h"
#include <stdio.h>
#include <string.h>

#define MAX_OUTPUT 8192

SeaError tool_http_request(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Usage: <GET|POST|HEAD> <url> [body]");
        return SEA_OK;
    }

    char input[4096];
    u32 ilen = args.len < sizeof(input) - 1 ? args.len : (u32)(sizeof(input) - 1);
    memcpy(input, args.data, ilen);
    input[ilen] = '\0';

    /* Parse method */
    char method[8];
    u32 mi = 0;
    char* p = input;
    while (*p == ' ') p++;
    while (*p && *p != ' ' && mi < sizeof(method) - 1) method[mi++] = *p++;
    method[mi] = '\0';
    while (*p == ' ') p++;

    /* Parse URL */
    char url[2048];
    u32 ui = 0;
    while (*p && *p != ' ' && ui < sizeof(url) - 1) url[ui++] = *p++;
    url[ui] = '\0';
    while (*p == ' ') p++;

    if (ui == 0) {
        *output = SEA_SLICE_LIT("Error: no URL provided");
        return SEA_OK;
    }

    SeaSlice us = { .data = (const u8*)url, .len = (u32)strlen(url) };
    if (sea_shield_detect_injection(us)) {
        *output = SEA_SLICE_LIT("Error: URL rejected by Shield");
        return SEA_OK;
    }

    if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0) {
        *output = SEA_SLICE_LIT("Error: URL must start with http:// or https://");
        return SEA_OK;
    }

    SeaHttpResponse resp;
    SeaError err;

    if (strcmp(method, "GET") == 0 || strcmp(method, "get") == 0) {
        err = sea_http_get(url, arena, &resp);
    } else if (strcmp(method, "POST") == 0 || strcmp(method, "post") == 0) {
        SeaSlice body = { .data = (const u8*)p, .len = (u32)strlen(p) };
        err = sea_http_post_json(url, body, arena, &resp);
    } else if (strcmp(method, "HEAD") == 0 || strcmp(method, "head") == 0) {
        err = sea_http_get(url, arena, &resp);
    } else {
        *output = SEA_SLICE_LIT("Error: method must be GET, POST, or HEAD");
        return SEA_OK;
    }

    if (err != SEA_OK) {
        char buf[256];
        int len = snprintf(buf, sizeof(buf), "HTTP request failed: %s %s", method, url);
        u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)len);
        if (dst) { output->data = dst; output->len = (u32)len; }
        return SEA_OK;
    }

    char* buf = (char*)sea_arena_alloc(arena, MAX_OUTPUT, 1);
    if (!buf) return SEA_ERR_ARENA_FULL;

    int pos = snprintf(buf, MAX_OUTPUT, "HTTP %d %s %s\n", resp.status_code, method, url);

    /* Truncate body */
    u32 body_show = resp.body.len;
    if (body_show > MAX_OUTPUT - 512) body_show = MAX_OUTPUT - 512;

    if (body_show > 0 && resp.body.data) {
        pos += snprintf(buf + pos, (size_t)(MAX_OUTPUT - pos), "\n%.*s",
                        (int)body_show, (const char*)resp.body.data);
        if (resp.body.len > body_show)
            pos += snprintf(buf + pos, (size_t)(MAX_OUTPUT - pos), "\n... (truncated, %u total bytes)", resp.body.len);
    }

    output->data = (const u8*)buf;
    output->len  = (u32)pos;
    return SEA_OK;
}
