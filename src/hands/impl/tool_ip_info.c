/*
 * tool_ip_info.c — IP address information and geolocation
 *
 * Tool ID:    43
 * Category:   Network
 * Args:       [ip_address] (default: public IP)
 * Returns:    IP geolocation data from ip-api.com
 *
 * Examples:
 *   /exec ip_info
 *   /exec ip_info 8.8.8.8
 *   /exec ip_info 1.1.1.1
 *
 * Security: IP validated by Shield. Uses free ip-api.com (no key needed).
 */

#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_http.h"
#include "seaclaw/sea_json.h"
#include "seaclaw/sea_shield.h"
#include <stdio.h>
#include <string.h>

SeaError tool_ip_info(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    char ip[64] = "";
    if (args.len > 0) {
        u32 ilen = args.len < sizeof(ip) - 1 ? args.len : (u32)(sizeof(ip) - 1);
        memcpy(ip, args.data, ilen);
        ip[ilen] = '\0';
        char* p = ip; while (*p == ' ') p++;
        char* e = p + strlen(p) - 1;
        while (e > p && (*e == ' ' || *e == '\n')) *e-- = '\0';
        memmove(ip, p, strlen(p) + 1);

        SeaSlice is = { .data = (const u8*)ip, .len = (u32)strlen(ip) };
        if (sea_shield_detect_injection(is)) {
            *output = SEA_SLICE_LIT("Error: IP rejected by Shield");
            return SEA_OK;
        }
    }

    char url[128];
    if (ip[0])
        snprintf(url, sizeof(url), "http://ip-api.com/json/%s", ip);
    else
        snprintf(url, sizeof(url), "http://ip-api.com/json/");

    SeaHttpResponse resp;
    SeaError err = sea_http_get(url, arena, &resp);
    if (err != SEA_OK || resp.status_code != 200) {
        *output = SEA_SLICE_LIT("Error: IP lookup failed");
        return SEA_OK;
    }

    SeaJsonValue root;
    err = sea_json_parse(resp.body, arena, &root);
    if (err != SEA_OK) {
        *output = SEA_SLICE_LIT("Error: failed to parse response");
        return SEA_OK;
    }

    SeaSlice query   = sea_json_get_string(&root, "query");
    SeaSlice country = sea_json_get_string(&root, "country");
    SeaSlice region  = sea_json_get_string(&root, "regionName");
    SeaSlice city    = sea_json_get_string(&root, "city");
    SeaSlice isp     = sea_json_get_string(&root, "isp");
    SeaSlice org     = sea_json_get_string(&root, "org");
    SeaSlice tz      = sea_json_get_string(&root, "timezone");

    char buf[512];
    int len = snprintf(buf, sizeof(buf),
        "IP: %.*s\n"
        "  Country:  %.*s\n"
        "  Region:   %.*s\n"
        "  City:     %.*s\n"
        "  ISP:      %.*s\n"
        "  Org:      %.*s\n"
        "  Timezone: %.*s",
        (int)query.len, query.data ? (const char*)query.data : "?",
        (int)country.len, country.data ? (const char*)country.data : "?",
        (int)region.len, region.data ? (const char*)region.data : "?",
        (int)city.len, city.data ? (const char*)city.data : "?",
        (int)isp.len, isp.data ? (const char*)isp.data : "?",
        (int)org.len, org.data ? (const char*)org.data : "?",
        (int)tz.len, tz.data ? (const char*)tz.data : "?");

    u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)len);
    if (!dst) return SEA_ERR_ARENA_FULL;
    output->data = dst; output->len = (u32)len;
    return SEA_OK;
}
