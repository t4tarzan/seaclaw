/*
 * tool_disk_usage.c — Disk usage and filesystem information
 *
 * Tool ID:    34
 * Category:   System
 * Args:       [path] (default: /)
 * Returns:    Disk usage for the given path and overall filesystem stats
 *
 * Examples:
 *   /exec disk_usage
 *   /exec disk_usage /root/seaclaw
 *   /exec disk_usage /var/log
 *
 * Security: Path validated by Shield. Read-only operation.
 */

#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_shield.h"
#include <stdio.h>
#include <string.h>
#include <sys/statvfs.h>

#define MAX_OUTPUT 2048

static const char* human_size(u64 bytes, char* buf, u32 blen) {
    if (bytes >= (u64)1024*1024*1024)
        snprintf(buf, blen, "%.1f GB", (double)bytes / (1024.0*1024.0*1024.0));
    else if (bytes >= 1024*1024)
        snprintf(buf, blen, "%.1f MB", (double)bytes / (1024.0*1024.0));
    else if (bytes >= 1024)
        snprintf(buf, blen, "%.1f KB", (double)bytes / 1024.0);
    else
        snprintf(buf, blen, "%lu B", (unsigned long)bytes);
    return buf;
}

SeaError tool_disk_usage(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    char path[512] = "/";
    if (args.len > 0) {
        u32 plen = args.len < sizeof(path) - 1 ? args.len : (u32)(sizeof(path) - 1);
        memcpy(path, args.data, plen);
        path[plen] = '\0';
        char* p = path;
        while (*p == ' ') p++;
        char* end = p + strlen(p) - 1;
        while (end > p && (*end == ' ' || *end == '\n')) *end-- = '\0';
        memmove(path, p, strlen(p) + 1);
    }

    SeaSlice ps = { .data = (const u8*)path, .len = (u32)strlen(path) };
    if (sea_shield_detect_injection(ps)) {
        *output = SEA_SLICE_LIT("Error: path rejected by Shield");
        return SEA_OK;
    }

    struct statvfs st;
    if (statvfs(path, &st) != 0) {
        char buf[256];
        int len = snprintf(buf, sizeof(buf), "Error: cannot stat filesystem at '%s'", path);
        u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)len);
        if (dst) { output->data = dst; output->len = (u32)len; }
        return SEA_OK;
    }

    u64 total = st.f_blocks * st.f_frsize;
    u64 free_space = st.f_bfree * st.f_frsize;
    u64 avail = st.f_bavail * st.f_frsize;
    u64 used = total - free_space;
    double pct = total > 0 ? (double)used / (double)total * 100.0 : 0;

    char t[32], u_s[32], f[32], a[32];

    char buf[MAX_OUTPUT];
    int len = snprintf(buf, sizeof(buf),
        "Filesystem: %s\n"
        "  Total:     %s\n"
        "  Used:      %s (%.1f%%)\n"
        "  Free:      %s\n"
        "  Available: %s\n"
        "  Inodes:    %lu / %lu (%.1f%% used)",
        path,
        human_size(total, t, sizeof(t)),
        human_size(used, u_s, sizeof(u_s)), pct,
        human_size(free_space, f, sizeof(f)),
        human_size(avail, a, sizeof(a)),
        (unsigned long)(st.f_files - st.f_ffree),
        (unsigned long)st.f_files,
        st.f_files > 0 ? (double)(st.f_files - st.f_ffree) / (double)st.f_files * 100.0 : 0);

    u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)len);
    if (!dst) return SEA_ERR_ARENA_FULL;
    output->data = dst; output->len = (u32)len;
    return SEA_OK;
}
