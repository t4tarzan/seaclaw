/*
 * tool_timestamp.c — Current time, unix timestamp, date formatting
 *
 * Args: optional format (unix|iso|utc|date) or empty for all
 * Returns: timestamp information
 */

#include "seaclaw/sea_tools.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

SeaError tool_timestamp(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    time_t now = time(NULL);
    struct tm* local = localtime(&now);
    struct tm* utc_tm = gmtime(&now);

    char local_str[64], utc_str[64];
    strftime(local_str, sizeof(local_str), "%Y-%m-%d %H:%M:%S %Z", local);
    strftime(utc_str, sizeof(utc_str), "%Y-%m-%dT%H:%M:%SZ", utc_tm);

    char fmt[32] = "";
    if (args.len > 0) {
        u32 flen = args.len < sizeof(fmt) - 1 ? args.len : (u32)(sizeof(fmt) - 1);
        memcpy(fmt, args.data, flen);
        fmt[flen] = '\0';
        char* f = fmt;
        while (*f == ' ') f++;
        memmove(fmt, f, strlen(f) + 1);
    }

    char buf[256];
    int len;

    if (strcmp(fmt, "unix") == 0) {
        len = snprintf(buf, sizeof(buf), "%lld", (long long)now);
    } else if (strcmp(fmt, "iso") == 0 || strcmp(fmt, "utc") == 0) {
        len = snprintf(buf, sizeof(buf), "%s", utc_str);
    } else if (strcmp(fmt, "date") == 0) {
        char date_str[32];
        strftime(date_str, sizeof(date_str), "%Y-%m-%d", local);
        len = snprintf(buf, sizeof(buf), "%s", date_str);
    } else {
        len = snprintf(buf, sizeof(buf),
            "Time:\n"
            "  Local: %s\n"
            "  UTC:   %s\n"
            "  Unix:  %lld",
            local_str, utc_str, (long long)now);
    }

    u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)len);
    if (!dst) return SEA_ERR_ARENA_FULL;
    output->data = dst; output->len = (u32)len;
    return SEA_OK;
}
