/*
 * tool_wc.c — Word count utility (like Unix wc)
 *
 * Tool ID:    29
 * Category:   Text Processing
 * Args:       <filepath_or_text>
 * Returns:    Lines, words, characters, bytes count
 *
 * If argument is a valid file path, reads from file.
 * Otherwise treats it as inline text.
 *
 * Examples:
 *   /exec wc /root/seaclaw/src/main.c
 *   /exec wc "hello world\nfoo bar baz"
 *
 * Security: File paths validated by Shield.
 */

#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_shield.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

SeaError tool_wc(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Usage: <filepath_or_text>");
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

    const char* data = NULL;
    u32 data_len = 0;
    char* file_buf = NULL;
    const char* source = "text";

    /* Try as file */
    if (*p == '/' || (*p == '.' && *(p+1) == '/')) {
        SeaSlice ps = { .data = (const u8*)p, .len = (u32)strlen(p) };
        if (!sea_shield_detect_injection(ps)) {
            FILE* fp = fopen(p, "r");
            if (fp) {
                fseek(fp, 0, SEEK_END);
                long sz = ftell(fp);
                fseek(fp, 0, SEEK_SET);
                if (sz > 0 && sz < 1048576) {
                    file_buf = (char*)sea_arena_alloc(arena, (u32)sz + 1, 1);
                    if (file_buf) {
                        data_len = (u32)fread(file_buf, 1, (size_t)sz, fp);
                        file_buf[data_len] = '\0';
                        data = file_buf;
                        source = p;
                    }
                }
                fclose(fp);
            }
        }
    }

    /* Fallback: treat as inline text */
    if (!data) {
        data = (const char*)args.data;
        data_len = args.len;
    }

    u32 lines = 0, words = 0, chars = 0, bytes = data_len;
    bool in_word = false;

    for (u32 i = 0; i < data_len; i++) {
        chars++;
        if (data[i] == '\n') lines++;
        if (isspace((unsigned char)data[i])) {
            in_word = false;
        } else if (!in_word) {
            in_word = true;
            words++;
        }
    }
    if (data_len > 0 && data[data_len - 1] != '\n') lines++;

    char buf[256];
    int len = snprintf(buf, sizeof(buf),
        "%7u %7u %7u %7u %s",
        lines, words, chars, bytes, source);

    u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)len);
    if (!dst) return SEA_ERR_ARENA_FULL;
    output->data = dst; output->len = (u32)len;
    return SEA_OK;
}
