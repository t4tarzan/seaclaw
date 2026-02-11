/*
 * tool_memory_manage.c — Read/write long-term memory from agent
 *
 * Args:
 *   read                — Read MEMORY.md
 *   write <content>     — Overwrite MEMORY.md
 *   append <content>    — Append to MEMORY.md
 *   daily <content>     — Append to today's daily note
 *   daily_read          — Read today's daily note
 *   bootstrap <file>    — Read a bootstrap file (IDENTITY.md, etc.)
 */

#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_memory.h"
#include <stdio.h>
#include <string.h>

/* External memory manager from main.c (may be NULL) */
extern SeaMemory* s_memory;

SeaError tool_memory_manage(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT(
            "Usage: read | write <content> | append <content> | "
            "daily <content> | daily_read | bootstrap <file>");
        return SEA_OK;
    }

    if (!s_memory) {
        *output = SEA_SLICE_LIT("Error: memory system not initialized");
        return SEA_OK;
    }

    char buf[4096];
    u32 len = args.len < sizeof(buf) - 1 ? args.len : (u32)sizeof(buf) - 1;
    memcpy(buf, args.data, len);
    buf[len] = '\0';

    if (strcmp(buf, "read") == 0) {
        const char* content = sea_memory_read(s_memory);
        if (!content) {
            *output = SEA_SLICE_LIT("(empty — no long-term memory yet)");
        } else {
            u32 clen = (u32)strlen(content);
            u8* out = (u8*)sea_arena_push_bytes(arena, (const u8*)content, clen);
            if (!out) return SEA_ERR_ARENA_FULL;
            output->data = out;
            output->len = clen;
        }
    } else if (strncmp(buf, "write ", 6) == 0) {
        SeaError err = sea_memory_write(s_memory, buf + 6);
        if (err == SEA_OK) {
            *output = SEA_SLICE_LIT("Long-term memory updated.");
        } else {
            *output = SEA_SLICE_LIT("Error: failed to write memory");
        }
    } else if (strncmp(buf, "append ", 7) == 0) {
        SeaError err = sea_memory_append(s_memory, buf + 7);
        if (err == SEA_OK) {
            *output = SEA_SLICE_LIT("Appended to long-term memory.");
        } else {
            *output = SEA_SLICE_LIT("Error: failed to append memory");
        }
    } else if (strncmp(buf, "daily ", 6) == 0) {
        SeaError err = sea_memory_append_daily(s_memory, buf + 6);
        if (err == SEA_OK) {
            *output = SEA_SLICE_LIT("Appended to today's daily note.");
        } else {
            *output = SEA_SLICE_LIT("Error: failed to append daily note");
        }
    } else if (strcmp(buf, "daily_read") == 0) {
        const char* content = sea_memory_read_daily(s_memory);
        if (!content) {
            *output = SEA_SLICE_LIT("(no daily note for today)");
        } else {
            u32 clen = (u32)strlen(content);
            u8* out = (u8*)sea_arena_push_bytes(arena, (const u8*)content, clen);
            if (!out) return SEA_ERR_ARENA_FULL;
            output->data = out;
            output->len = clen;
        }
    } else if (strncmp(buf, "bootstrap ", 10) == 0) {
        const char* content = sea_memory_read_bootstrap(s_memory, buf + 10);
        if (!content) {
            *output = SEA_SLICE_LIT("(bootstrap file not found)");
        } else {
            u32 clen = (u32)strlen(content);
            u8* out = (u8*)sea_arena_push_bytes(arena, (const u8*)content, clen);
            if (!out) return SEA_ERR_ARENA_FULL;
            output->data = out;
            output->len = clen;
        }
    } else {
        *output = SEA_SLICE_LIT(
            "Unknown subcommand. Use: read | write | append | daily | daily_read | bootstrap");
    }

    return SEA_OK;
}
