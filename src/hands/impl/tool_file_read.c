/*
 * tool_file_read.c — Read a file from disk
 *
 * Args: file path (string)
 * Returns: file contents (truncated to 8KB if larger)
 */

#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_shield.h"
#include <stdio.h>
#include <string.h>

#define MAX_READ_SIZE (8 * 1024)

SeaError tool_file_read(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Error: no file path provided");
        return SEA_OK;
    }

    /* Null-terminate the path */
    char path[1024];
    u32 plen = args.len < sizeof(path) - 1 ? args.len : (u32)(sizeof(path) - 1);
    memcpy(path, args.data, plen);
    path[plen] = '\0';

    /* Strip leading/trailing whitespace */
    char* p = path;
    while (*p == ' ' || *p == '\t') p++;
    char* end = p + strlen(p) - 1;
    while (end > p && (*end == ' ' || *end == '\t' || *end == '\n')) *end-- = '\0';

    /* Shield: validate path */
    SeaSlice path_slice = { .data = (const u8*)p, .len = (u32)strlen(p) };
    if (sea_shield_detect_injection(path_slice)) {
        *output = SEA_SLICE_LIT("Error: path rejected by Shield (injection detected)");
        return SEA_OK;
    }

    /* Reject path traversal */
    if (strstr(p, "..")) {
        *output = SEA_SLICE_LIT("Error: path traversal not allowed");
        return SEA_OK;
    }

    FILE* f = fopen(p, "r");
    if (!f) {
        char err[256];
        int len = snprintf(err, sizeof(err), "Error: cannot open '%s'", p);
        u8* dst = (u8*)sea_arena_push_bytes(arena, err, (u64)len);
        if (dst) { output->data = dst; output->len = (u32)len; }
        return SEA_OK;
    }

    /* Read up to MAX_READ_SIZE */
    u8* buf = (u8*)sea_arena_alloc(arena, MAX_READ_SIZE + 64, 1);
    if (!buf) { fclose(f); return SEA_ERR_ARENA_FULL; }

    size_t nread = fread(buf, 1, MAX_READ_SIZE, f);
    fclose(f);

    if (nread == MAX_READ_SIZE) {
        memcpy(buf + nread, "\n... (truncated at 8KB)", 23);
        nread += 23;
    }

    output->data = buf;
    output->len  = (u32)nread;
    return SEA_OK;
}
