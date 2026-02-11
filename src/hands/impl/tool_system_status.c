/*
 * tool_system_status.c — Report memory usage and uptime
 */

#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_log.h"
#include <stdio.h>
#include <string.h>

SeaError tool_system_status(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    (void)args;

    u64 used = sea_arena_used(arena);
    u64 total = arena->size;
    u64 high = arena->high_water;
    u64 uptime_ms = sea_log_elapsed_ms();

    u64 up_sec = uptime_ms / 1000;
    u64 up_min = up_sec / 60;
    u64 up_hr  = up_min / 60;

    char buf[512];
    int len = snprintf(buf, sizeof(buf),
        "Sea-Claw v%s\n"
        "  Uptime:     %luh %lum %lus\n"
        "  Arena:      %lu / %lu bytes (%.1f%%)\n"
        "  Peak:       %lu bytes (%.1f%%)\n"
        "  Tools:      %u registered",
        SEA_VERSION_STRING,
        (unsigned long)(up_hr),
        (unsigned long)(up_min % 60),
        (unsigned long)(up_sec % 60),
        (unsigned long)used,
        (unsigned long)total,
        (double)used / (double)total * 100.0,
        (unsigned long)high,
        (double)high / (double)total * 100.0,
        sea_tools_count());

    if (len <= 0 || (u32)len >= sizeof(buf)) return SEA_ERR_OOM;

    u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)len);
    if (!dst) return SEA_ERR_ARENA_FULL;

    output->data = dst;
    output->len  = (u32)len;
    return SEA_OK;
}
