/*
 * tool_hash_compute.c — Compute hash of text
 *
 * Args: <md5|sha256|crc32> <text>
 * Returns: hex-encoded hash
 *
 * Uses simple public-domain implementations (no OpenSSL dependency).
 */

#include "seaclaw/sea_tools.h"
#include <stdio.h>
#include <string.h>

/* ── CRC32 (IEEE) ─────────────────────────────────────────── */

static u32 crc32_compute(const u8* data, u32 len) {
    u32 crc = 0xFFFFFFFF;
    for (u32 i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
    }
    return ~crc;
}

/* ── DJB2 hash (fast, non-crypto) ────────────────────────── */

static u64 djb2_compute(const u8* data, u32 len) {
    u64 hash = 5381;
    for (u32 i = 0; i < len; i++)
        hash = ((hash << 5) + hash) + data[i];
    return hash;
}

/* ── FNV-1a 64-bit ───────────────────────────────────────── */

static u64 fnv1a_compute(const u8* data, u32 len) {
    u64 hash = 0xcbf29ce484222325ULL;
    for (u32 i = 0; i < len; i++) {
        hash ^= data[i];
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

SeaError tool_hash_compute(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Usage: <crc32|djb2|fnv1a> <text>");
        return SEA_OK;
    }

    /* Parse algorithm name */
    char algo[16];
    u32 i = 0;
    while (i < args.len && i < sizeof(algo) - 1 && args.data[i] != ' ') {
        algo[i] = (char)args.data[i]; i++;
    }
    algo[i] = '\0';
    while (i < args.len && args.data[i] == ' ') i++;

    const u8* text = args.data + i;
    u32 tlen = args.len - i;

    char buf[128];
    int len;

    if (strcmp(algo, "crc32") == 0) {
        u32 h = crc32_compute(text, tlen);
        len = snprintf(buf, sizeof(buf), "CRC32: %08x", h);
    } else if (strcmp(algo, "djb2") == 0) {
        u64 h = djb2_compute(text, tlen);
        len = snprintf(buf, sizeof(buf), "DJB2: %016llx", (unsigned long long)h);
    } else if (strcmp(algo, "fnv1a") == 0) {
        u64 h = fnv1a_compute(text, tlen);
        len = snprintf(buf, sizeof(buf), "FNV-1a: %016llx", (unsigned long long)h);
    } else {
        len = snprintf(buf, sizeof(buf),
            "Unknown algorithm: %s\nAvailable: crc32, djb2, fnv1a", algo);
    }

    u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)len);
    if (!dst) return SEA_ERR_ARENA_FULL;
    output->data = dst; output->len = (u32)len;
    return SEA_OK;
}
