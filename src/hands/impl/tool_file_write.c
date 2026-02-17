/*
 * tool_file_write.c — Write content to a file
 *
 * Args: "path|content" (pipe-separated)
 * Creates parent dirs if needed. Overwrites existing files.
 */

#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_shield.h"
#include <stdio.h>
#include <string.h>

SeaError tool_file_write(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    (void)arena;

    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Error: usage: path|content");
        return SEA_OK;
    }

    /* Find the pipe separator */
    const u8* sep = NULL;
    for (u32 i = 0; i < args.len; i++) {
        if (args.data[i] == '|') { sep = &args.data[i]; break; }
    }
    if (!sep) {
        *output = SEA_SLICE_LIT("Error: usage: path|content (pipe separator required)");
        return SEA_OK;
    }

    /* Extract path */
    u32 path_len = (u32)(sep - args.data);
    char path[1024];
    u32 plen = path_len < sizeof(path) - 1 ? path_len : (u32)(sizeof(path) - 1);
    memcpy(path, args.data, plen);
    path[plen] = '\0';

    /* Strip whitespace from path */
    char* p = path;
    while (*p == ' ' || *p == '\t') p++;
    char* end = p + strlen(p) - 1;
    while (end > p && (*end == ' ' || *end == '\t')) *end-- = '\0';

    /* Shield: validate path */
    SeaSlice path_slice = { .data = (const u8*)p, .len = (u32)strlen(p) };
    if (sea_shield_detect_injection(path_slice)) {
        *output = SEA_SLICE_LIT("Error: path rejected by Shield");
        return SEA_OK;
    }

    /* Canonicalize path and check for symlink escape */
    char resolved_path[4096];
    const char* workspace = ".";  /* Use current directory as workspace */
    if (!sea_shield_canonicalize_path(p, workspace, resolved_path, sizeof(resolved_path))) {
        *output = SEA_SLICE_LIT("Error: path escape detected (symlink or traversal blocked)");
        return SEA_OK;
    }

    /* Extract content */
    const u8* content = sep + 1;
    u32 content_len = args.len - path_len - 1;

    FILE* f = fopen(resolved_path, "w");
    if (!f) {
        char err[256];
        int len = snprintf(err, sizeof(err), "Error: cannot open '%s' for writing", p);
        u8* dst = (u8*)sea_arena_push_bytes(arena, err, (u64)len);
        if (dst) { output->data = dst; output->len = (u32)len; }
        return SEA_OK;
    }

    fwrite(content, 1, content_len, f);
    fclose(f);

    char msg[256];
    int mlen = snprintf(msg, sizeof(msg), "Wrote %u bytes to '%s'", content_len, p);
    u8* dst = (u8*)sea_arena_push_bytes(arena, msg, (u64)mlen);
    if (dst) { output->data = dst; output->len = (u32)mlen; }
    return SEA_OK;
}
