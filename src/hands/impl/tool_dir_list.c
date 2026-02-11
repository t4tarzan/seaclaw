/*
 * tool_dir_list.c — List directory contents
 *
 * Args: directory path
 * Returns: list of files with sizes
 */

#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_shield.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAX_OUTPUT 8192

SeaError tool_dir_list(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Error: no directory path provided");
        return SEA_OK;
    }

    char path[1024];
    u32 plen = args.len < sizeof(path) - 1 ? args.len : (u32)(sizeof(path) - 1);
    memcpy(path, args.data, plen);
    path[plen] = '\0';

    /* Trim */
    char* p = path;
    while (*p == ' ') p++;
    char* end = p + strlen(p) - 1;
    while (end > p && (*end == ' ' || *end == '\n')) *end-- = '\0';

    /* Shield check */
    SeaSlice ps = { .data = (const u8*)p, .len = (u32)strlen(p) };
    if (sea_shield_detect_injection(ps)) {
        *output = SEA_SLICE_LIT("Error: path rejected by Shield");
        return SEA_OK;
    }

    DIR* dir = opendir(p);
    if (!dir) {
        char buf[256];
        int len = snprintf(buf, sizeof(buf), "Error: cannot open directory '%s'", p);
        u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)len);
        if (dst) { output->data = dst; output->len = (u32)len; }
        return SEA_OK;
    }

    char* buf = (char*)sea_arena_alloc(arena, MAX_OUTPUT, 1);
    if (!buf) { closedir(dir); return SEA_ERR_ARENA_FULL; }

    int pos = snprintf(buf, MAX_OUTPUT, "Directory: %s\n", p);
    u32 count = 0;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL && pos < MAX_OUTPUT - 256) {
        if (entry->d_name[0] == '.' && (entry->d_name[1] == '\0' ||
            (entry->d_name[1] == '.' && entry->d_name[2] == '\0')))
            continue;

        char fullpath[2048];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", p, entry->d_name);

        struct stat st;
        const char* type = "?";
        u64 size = 0;
        if (stat(fullpath, &st) == 0) {
            if (S_ISDIR(st.st_mode)) type = "DIR";
            else if (S_ISREG(st.st_mode)) type = "FILE";
            else if (S_ISLNK(st.st_mode)) type = "LINK";
            size = (u64)st.st_size;
        }

        pos += snprintf(buf + pos, (size_t)(MAX_OUTPUT - pos),
            "  %-4s %8lu  %s\n", type, (unsigned long)size, entry->d_name);
        count++;
    }
    closedir(dir);

    pos += snprintf(buf + pos, (size_t)(MAX_OUTPUT - pos), "(%u entries)", count);

    output->data = (const u8*)buf;
    output->len  = (u32)pos;
    return SEA_OK;
}
