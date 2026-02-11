/*
 * tool_uuid_gen.c — Generate UUID v4 (random)
 *
 * Args: optional count (default 1, max 10)
 * Returns: UUID(s)
 */

#include "seaclaw/sea_tools.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void gen_uuid4(char* buf) {
    u8 bytes[16];
    /* Use /dev/urandom for randomness */
    FILE* f = fopen("/dev/urandom", "rb");
    if (f) {
        if (fread(bytes, 1, 16, f) != 16) {
            /* fallback to rand */
            for (int i = 0; i < 16; i++) bytes[i] = (u8)(rand() & 0xFF);
        }
        fclose(f);
    } else {
        for (int i = 0; i < 16; i++) bytes[i] = (u8)(rand() & 0xFF);
    }
    /* Set version 4 and variant bits */
    bytes[6] = (bytes[6] & 0x0F) | 0x40;
    bytes[8] = (bytes[8] & 0x3F) | 0x80;

    sprintf(buf, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            bytes[0], bytes[1], bytes[2], bytes[3],
            bytes[4], bytes[5], bytes[6], bytes[7],
            bytes[8], bytes[9], bytes[10], bytes[11],
            bytes[12], bytes[13], bytes[14], bytes[15]);
}

SeaError tool_uuid_gen(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    u32 count = 1;
    if (args.len > 0) {
        char num[16];
        u32 nlen = args.len < sizeof(num) - 1 ? args.len : (u32)(sizeof(num) - 1);
        memcpy(num, args.data, nlen);
        num[nlen] = '\0';
        int n = atoi(num);
        if (n > 0 && n <= 10) count = (u32)n;
    }

    char buf[512];
    int pos = 0;
    for (u32 i = 0; i < count && pos < (int)sizeof(buf) - 48; i++) {
        char uuid[40];
        gen_uuid4(uuid);
        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "%s%s",
                        uuid, (i + 1 < count) ? "\n" : "");
    }

    u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)pos);
    if (!dst) return SEA_ERR_ARENA_FULL;
    output->data = dst; output->len = (u32)pos;
    return SEA_OK;
}
