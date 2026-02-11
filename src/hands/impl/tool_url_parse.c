/*
 * tool_url_parse.c — Parse URL into components
 *
 * Args: URL string
 * Returns: scheme, host, port, path, query, fragment
 */

#include "seaclaw/sea_tools.h"
#include <stdio.h>
#include <string.h>

SeaError tool_url_parse(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Error: no URL provided");
        return SEA_OK;
    }

    char url[2048];
    u32 ulen = args.len < sizeof(url) - 1 ? args.len : (u32)(sizeof(url) - 1);
    memcpy(url, args.data, ulen);
    url[ulen] = '\0';
    char* u = url;
    while (*u == ' ') u++;
    char* end = u + strlen(u) - 1;
    while (end > u && (*end == ' ' || *end == '\n')) *end-- = '\0';

    /* Parse scheme */
    char scheme[16] = "";
    char* p = strstr(u, "://");
    if (p) {
        u32 slen = (u32)(p - u);
        if (slen < sizeof(scheme)) { memcpy(scheme, u, slen); scheme[slen] = '\0'; }
        p += 3;
    } else {
        p = u;
    }

    /* Parse host[:port] */
    char host[256] = "";
    char port[8] = "";
    char* path_start = strchr(p, '/');
    char* host_end = path_start ? path_start : p + strlen(p);
    char* colon = NULL;
    for (char* c = p; c < host_end; c++) { if (*c == ':') colon = c; }

    if (colon) {
        u32 hlen = (u32)(colon - p);
        if (hlen < sizeof(host)) { memcpy(host, p, hlen); host[hlen] = '\0'; }
        u32 plen = (u32)(host_end - colon - 1);
        if (plen < sizeof(port)) { memcpy(port, colon + 1, plen); port[plen] = '\0'; }
    } else {
        u32 hlen = (u32)(host_end - p);
        if (hlen < sizeof(host)) { memcpy(host, p, hlen); host[hlen] = '\0'; }
    }

    /* Parse path, query, fragment */
    char path[512] = "/";
    char query[512] = "";
    char fragment[128] = "";

    if (path_start) {
        char* q = strchr(path_start, '?');
        char* f = strchr(path_start, '#');

        if (q) {
            u32 plen = (u32)(q - path_start);
            if (plen < sizeof(path)) { memcpy(path, path_start, plen); path[plen] = '\0'; }
            char* qend = f ? f : path_start + strlen(path_start);
            u32 qlen = (u32)(qend - q - 1);
            if (qlen < sizeof(query)) { memcpy(query, q + 1, qlen); query[qlen] = '\0'; }
        } else if (f) {
            u32 plen = (u32)(f - path_start);
            if (plen < sizeof(path)) { memcpy(path, path_start, plen); path[plen] = '\0'; }
        } else {
            u32 plen = (u32)strlen(path_start);
            if (plen < sizeof(path)) { memcpy(path, path_start, plen); path[plen] = '\0'; }
        }

        if (f) {
            u32 flen = (u32)strlen(f + 1);
            if (flen < sizeof(fragment)) { memcpy(fragment, f + 1, flen); fragment[flen] = '\0'; }
        }
    }

    char buf[1024];
    int len = snprintf(buf, sizeof(buf),
        "URL: %s\n"
        "  Scheme:   %s\n"
        "  Host:     %s\n"
        "  Port:     %s\n"
        "  Path:     %s\n"
        "  Query:    %s\n"
        "  Fragment: %s",
        u, scheme[0] ? scheme : "(none)",
        host[0] ? host : "(none)",
        port[0] ? port : "(default)",
        path, query[0] ? query : "(none)",
        fragment[0] ? fragment : "(none)");

    u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)len);
    if (!dst) return SEA_ERR_ARENA_FULL;
    output->data = dst; output->len = (u32)len;
    return SEA_OK;
}
