/*
 * tool_checksum_file.c — Compute checksum of a file
 *
 * Tool ID:    40
 * Category:   File I/O / Security
 * Args:       <filepath>
 * Returns:    CRC32 and FNV-1a checksums of the file contents
 *
 * Useful for verifying file integrity and detecting changes.
 *
 * Examples:
 *   /exec checksum_file /root/seaclaw/sea_claw
 *   /exec checksum_file /etc/hostname
 *
 * Security: File path validated by Shield. Read-only operation.
 */

#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_shield.h"
#include <stdio.h>
#include <string.h>

SeaError tool_checksum_file(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Usage: <filepath>");
        return SEA_OK;
    }

    char path[1024];
    u32 plen = args.len < sizeof(path) - 1 ? args.len : (u32)(sizeof(path) - 1);
    memcpy(path, args.data, plen);
    path[plen] = '\0';
    char* p = path;
    while (*p == ' ') p++;
    char* end = p + strlen(p) - 1;
    while (end > p && (*end == ' ' || *end == '\n')) *end-- = '\0';

    SeaSlice ps = { .data = (const u8*)p, .len = (u32)strlen(p) };
    if (sea_shield_detect_injection(ps)) {
        *output = SEA_SLICE_LIT("Error: path rejected by Shield");
        return SEA_OK;
    }

    FILE* fp = fopen(p, "rb");
    if (!fp) {
        char buf[256];
        int len = snprintf(buf, sizeof(buf), "Error: cannot open '%s'", p);
        u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)len);
        if (dst) { output->data = dst; output->len = (u32)len; }
        return SEA_OK;
    }

    u32 crc = 0xFFFFFFFF;
    u64 fnv = 0xcbf29ce484222325ULL;
    u64 total = 0;
    u8 buf[4096];
    size_t n;

    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        total += n;
        for (size_t i = 0; i < n; i++) {
            crc ^= buf[i];
            for (int j = 0; j < 8; j++)
                crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
            fnv ^= buf[i];
            fnv *= 0x100000001b3ULL;
        }
    }
    fclose(fp);
    crc = ~crc;

    char out[256];
    int len = snprintf(out, sizeof(out),
        "File: %s\n"
        "  Size:   %lu bytes\n"
        "  CRC32:  %08x\n"
        "  FNV-1a: %016llx",
        p, (unsigned long)total, crc, (unsigned long long)fnv);

    u8* dst = (u8*)sea_arena_push_bytes(arena, out, (u64)len);
    if (!dst) return SEA_ERR_ARENA_FULL;
    output->data = dst; output->len = (u32)len;
    return SEA_OK;
}
