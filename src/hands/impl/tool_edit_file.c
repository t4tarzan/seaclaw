/*
 * tool_edit_file.c — Surgical find-and-replace within files
 *
 * Args: <filepath>|||<find>|||<replace>
 * Reads the file, replaces first occurrence of <find> with <replace>,
 * writes back. Returns confirmation or error.
 */

#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_shield.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

SeaError tool_edit_file(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Usage: <filepath>|||<find>|||<replace>");
        return SEA_OK;
    }

    char buf[4096];
    u32 len = args.len < sizeof(buf) - 1 ? args.len : (u32)sizeof(buf) - 1;
    memcpy(buf, args.data, len);
    buf[len] = '\0';

    /* Parse three fields separated by ||| */
    char* sep1 = strstr(buf, "|||");
    if (!sep1) {
        *output = SEA_SLICE_LIT("Error: expected <filepath>|||<find>|||<replace>");
        return SEA_OK;
    }
    *sep1 = '\0';
    char* find_str = sep1 + 3;

    char* sep2 = strstr(find_str, "|||");
    if (!sep2) {
        *output = SEA_SLICE_LIT("Error: expected <filepath>|||<find>|||<replace>");
        return SEA_OK;
    }
    *sep2 = '\0';
    char* replace_str = sep2 + 3;

    char* filepath = buf;

    /* Security: check path */
    SeaSlice path_slice = { .data = (const u8*)filepath, .len = (u32)strlen(filepath) };
    if (sea_shield_detect_injection(path_slice)) {
        *output = SEA_SLICE_LIT("Error: path injection detected");
        return SEA_OK;
    }

    /* Read file */
    FILE* f = fopen(filepath, "r");
    if (!f) {
        *output = SEA_SLICE_LIT("Error: cannot open file");
        return SEA_OK;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize <= 0 || fsize > 512 * 1024) {
        fclose(f);
        *output = SEA_SLICE_LIT("Error: file too large or empty (max 512KB)");
        return SEA_OK;
    }

    char* content = (char*)sea_arena_alloc(arena, (u64)fsize + 1, 1);
    if (!content) { fclose(f); return SEA_ERR_ARENA_FULL; }
    size_t rd = fread(content, 1, (size_t)fsize, f);
    content[rd] = '\0';
    fclose(f);

    /* Find the substring */
    char* match = strstr(content, find_str);
    if (!match) {
        *output = SEA_SLICE_LIT("Error: find string not found in file");
        return SEA_OK;
    }

    /* Build new content */
    size_t find_len = strlen(find_str);
    size_t replace_len = strlen(replace_str);
    size_t new_size = rd - find_len + replace_len;

    char* new_content = (char*)sea_arena_alloc(arena, (u64)new_size + 1, 1);
    if (!new_content) return SEA_ERR_ARENA_FULL;

    size_t prefix_len = (size_t)(match - content);
    memcpy(new_content, content, prefix_len);
    memcpy(new_content + prefix_len, replace_str, replace_len);
    memcpy(new_content + prefix_len + replace_len,
           match + find_len, rd - prefix_len - find_len);
    new_content[new_size] = '\0';

    /* Write back */
    f = fopen(filepath, "w");
    if (!f) {
        *output = SEA_SLICE_LIT("Error: cannot write file");
        return SEA_OK;
    }
    fwrite(new_content, 1, new_size, f);
    fclose(f);

    /* Result */
    char* result = (char*)sea_arena_alloc(arena, 256, 1);
    if (!result) return SEA_ERR_ARENA_FULL;
    int rlen = snprintf(result, 256,
        "Edited %s: replaced %zu bytes with %zu bytes",
        filepath, find_len, replace_len);
    output->data = (const u8*)result;
    output->len = (u32)rlen;
    return SEA_OK;
}
