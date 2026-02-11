/*
 * tool_text_summarize.c — Summarize text statistics
 *
 * Args: text to analyze
 * Returns: character count, word count, line count, preview
 */

#include "seaclaw/sea_tools.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

SeaError tool_text_summarize(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Error: no text provided");
        return SEA_OK;
    }

    u32 chars = args.len;
    u32 words = 0, lines = 1, sentences = 0;
    bool in_word = false;

    for (u32 i = 0; i < args.len; i++) {
        char c = (char)args.data[i];
        if (c == '\n') lines++;
        if (c == '.' || c == '!' || c == '?') sentences++;
        if (isspace((unsigned char)c)) {
            in_word = false;
        } else if (!in_word) {
            in_word = true;
            words++;
        }
    }

    /* Preview: first 100 chars */
    u32 preview_len = chars > 100 ? 100 : chars;

    char buf[512];
    int len = snprintf(buf, sizeof(buf),
        "Text Summary:\n"
        "  Characters: %u\n"
        "  Words:      %u\n"
        "  Lines:      %u\n"
        "  Sentences:  %u\n"
        "  Preview:    %.*s%s",
        chars, words, lines, sentences,
        (int)preview_len, (const char*)args.data,
        chars > 100 ? "..." : "");

    u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)len);
    if (!dst) return SEA_ERR_ARENA_FULL;
    output->data = dst; output->len = (u32)len;
    return SEA_OK;
}
