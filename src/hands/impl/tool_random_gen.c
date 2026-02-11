/*
 * tool_random_gen.c — Generate random numbers or strings
 *
 * Args: <number [min] [max]> | <string [length]> | <hex [length]> | <coin> | <dice [sides]>
 * Returns: random value
 */

#include "seaclaw/sea_tools.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static u32 secure_random(void) {
    u32 val;
    FILE* f = fopen("/dev/urandom", "rb");
    if (f) {
        if (fread(&val, sizeof(val), 1, f) != 1) val = (u32)rand();
        fclose(f);
    } else {
        val = (u32)rand();
    }
    return val;
}

SeaError tool_random_gen(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Usage: <number [min max]|string [len]|hex [len]|coin|dice [sides]>");
        return SEA_OK;
    }

    char input[256];
    u32 ilen = args.len < sizeof(input) - 1 ? args.len : (u32)(sizeof(input) - 1);
    memcpy(input, args.data, ilen);
    input[ilen] = '\0';

    char type[32] = "";
    int arg1 = 0, arg2 = 100;
    sscanf(input, "%31s %d %d", type, &arg1, &arg2);

    char buf[256];
    int len;

    if (strcmp(type, "number") == 0) {
        if (arg2 <= arg1) arg2 = arg1 + 100;
        i64 val = (i64)arg1 + (i64)(secure_random() % (u32)(arg2 - arg1 + 1));
        len = snprintf(buf, sizeof(buf), "%lld", (long long)val);
    } else if (strcmp(type, "string") == 0) {
        u32 slen = (arg1 > 0 && arg1 <= 128) ? (u32)arg1 : 16;
        static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        for (u32 i = 0; i < slen; i++)
            buf[i] = charset[secure_random() % (sizeof(charset) - 1)];
        buf[slen] = '\0';
        len = (int)slen;
    } else if (strcmp(type, "hex") == 0) {
        u32 slen = (arg1 > 0 && arg1 <= 64) ? (u32)arg1 : 16;
        len = 0;
        for (u32 i = 0; i < slen; i++)
            len += snprintf(buf + len, sizeof(buf) - (size_t)len, "%02x", secure_random() & 0xFF);
    } else if (strcmp(type, "coin") == 0) {
        len = snprintf(buf, sizeof(buf), "%s", (secure_random() & 1) ? "Heads" : "Tails");
    } else if (strcmp(type, "dice") == 0) {
        u32 sides = (arg1 > 1 && arg1 <= 100) ? (u32)arg1 : 6;
        len = snprintf(buf, sizeof(buf), "%u", (secure_random() % sides) + 1);
    } else {
        len = snprintf(buf, sizeof(buf),
            "Unknown type: %s\nAvailable: number [min max], string [len], hex [len], coin, dice [sides]", type);
    }

    u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)len);
    if (!dst) return SEA_ERR_ARENA_FULL;
    output->data = dst; output->len = (u32)len;
    return SEA_OK;
}
