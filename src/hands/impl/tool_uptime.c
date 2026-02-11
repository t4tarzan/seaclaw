/*
 * tool_uptime.c — System uptime and load averages
 *
 * Tool ID:    42
 * Category:   System
 * Args:       (none)
 * Returns:    System uptime, load averages, logged-in users
 *
 * Examples:
 *   /exec uptime
 *
 * Security: Read-only system information. No arguments needed.
 */

#include "seaclaw/sea_tools.h"
#include <stdio.h>
#include <string.h>
#include <sys/sysinfo.h>

SeaError tool_uptime(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    (void)args;

    struct sysinfo si;
    if (sysinfo(&si) != 0) {
        *output = SEA_SLICE_LIT("Error: cannot read system info");
        return SEA_OK;
    }

    long days = si.uptime / 86400;
    long hours = (si.uptime % 86400) / 3600;
    long mins = (si.uptime % 3600) / 60;

    u64 total_ram = si.totalram * si.mem_unit;
    u64 free_ram = si.freeram * si.mem_unit;
    u64 used_ram = total_ram - free_ram;

    char buf[512];
    int len = snprintf(buf, sizeof(buf),
        "System Uptime:\n"
        "  Up:       %ld days, %ld hours, %ld minutes\n"
        "  Load:     %.2f, %.2f, %.2f (1m, 5m, 15m)\n"
        "  Procs:    %u running\n"
        "  RAM:      %lu / %lu MB (%.1f%% used)",
        days, hours, mins,
        si.loads[0] / 65536.0, si.loads[1] / 65536.0, si.loads[2] / 65536.0,
        si.procs,
        (unsigned long)(used_ram / (1024*1024)),
        (unsigned long)(total_ram / (1024*1024)),
        total_ram > 0 ? (double)used_ram / (double)total_ram * 100.0 : 0);

    u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)len);
    if (!dst) return SEA_ERR_ARENA_FULL;
    output->data = dst; output->len = (u32)len;
    return SEA_OK;
}
