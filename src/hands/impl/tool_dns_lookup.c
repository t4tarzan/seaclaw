/*
 * tool_dns_lookup.c — Resolve hostname to IP address
 *
 * Args: hostname
 * Returns: IP addresses
 */

#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_shield.h"
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>

SeaError tool_dns_lookup(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Error: no hostname provided");
        return SEA_OK;
    }

    char host[256];
    u32 hlen = args.len < sizeof(host) - 1 ? args.len : (u32)(sizeof(host) - 1);
    memcpy(host, args.data, hlen);
    host[hlen] = '\0';
    char* h = host;
    while (*h == ' ') h++;
    char* end = h + strlen(h) - 1;
    while (end > h && (*end == ' ' || *end == '\n')) *end-- = '\0';

    SeaSlice hs = { .data = (const u8*)h, .len = (u32)strlen(h) };
    if (sea_shield_detect_injection(hs)) {
        *output = SEA_SLICE_LIT("Error: hostname rejected by Shield");
        return SEA_OK;
    }

    struct addrinfo hints, *res, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(h, NULL, &hints, &res);
    if (status != 0) {
        char buf[256];
        int len = snprintf(buf, sizeof(buf), "DNS lookup failed for '%s': %s", h, gai_strerror(status));
        u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)len);
        if (dst) { output->data = dst; output->len = (u32)len; }
        return SEA_OK;
    }

    char buf[2048];
    int pos = snprintf(buf, sizeof(buf), "DNS: %s\n", h);
    u32 count = 0;

    for (p = res; p != NULL && pos < (int)sizeof(buf) - 128; p = p->ai_next) {
        char ip[INET6_ADDRSTRLEN];
        void* addr;
        const char* family;

        if (p->ai_family == AF_INET) {
            struct sockaddr_in* ipv4 = (struct sockaddr_in*)p->ai_addr;
            addr = &(ipv4->sin_addr);
            family = "IPv4";
        } else {
            struct sockaddr_in6* ipv6 = (struct sockaddr_in6*)p->ai_addr;
            addr = &(ipv6->sin6_addr);
            family = "IPv6";
        }

        inet_ntop(p->ai_family, addr, ip, sizeof(ip));
        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "  %s: %s\n", family, ip);
        count++;
    }
    freeaddrinfo(res);

    pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "(%u addresses)", count);

    u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)pos);
    if (!dst) return SEA_ERR_ARENA_FULL;
    output->data = dst; output->len = (u32)pos;
    return SEA_OK;
}
