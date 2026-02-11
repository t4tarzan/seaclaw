/*
 * tool_file_info.c — File metadata: size, permissions, modification time
 *
 * Args: file path
 * Returns: file stats
 */

#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_shield.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

SeaError tool_file_info(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Error: no file path provided");
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

    SeaSlice ps = { .data = (const u8*)p, .len = (u32)strlen(p) };
    if (sea_shield_detect_injection(ps)) {
        *output = SEA_SLICE_LIT("Error: path rejected by Shield");
        return SEA_OK;
    }

    struct stat st;
    if (stat(p, &st) != 0) {
        char buf[256];
        int len = snprintf(buf, sizeof(buf), "Error: cannot stat '%s'", p);
        u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)len);
        if (dst) { output->data = dst; output->len = (u32)len; }
        return SEA_OK;
    }

    const char* type = "unknown";
    if (S_ISREG(st.st_mode)) type = "regular file";
    else if (S_ISDIR(st.st_mode)) type = "directory";
    else if (S_ISLNK(st.st_mode)) type = "symlink";
    else if (S_ISFIFO(st.st_mode)) type = "fifo";
    else if (S_ISSOCK(st.st_mode)) type = "socket";

    char mtime[64];
    struct tm* tm = localtime(&st.st_mtime);
    strftime(mtime, sizeof(mtime), "%Y-%m-%d %H:%M:%S", tm);

    char perms[11] = "----------";
    if (S_ISDIR(st.st_mode)) perms[0] = 'd';
    if (st.st_mode & S_IRUSR) perms[1] = 'r';
    if (st.st_mode & S_IWUSR) perms[2] = 'w';
    if (st.st_mode & S_IXUSR) perms[3] = 'x';
    if (st.st_mode & S_IRGRP) perms[4] = 'r';
    if (st.st_mode & S_IWGRP) perms[5] = 'w';
    if (st.st_mode & S_IXGRP) perms[6] = 'x';
    if (st.st_mode & S_IROTH) perms[7] = 'r';
    if (st.st_mode & S_IWOTH) perms[8] = 'w';
    if (st.st_mode & S_IXOTH) perms[9] = 'x';

    char buf[512];
    int len = snprintf(buf, sizeof(buf),
        "File: %s\n"
        "  Type:        %s\n"
        "  Size:        %lu bytes\n"
        "  Permissions: %s (%04o)\n"
        "  Modified:    %s\n"
        "  Inode:       %lu\n"
        "  Links:       %lu",
        p, type, (unsigned long)st.st_size,
        perms, (unsigned)(st.st_mode & 07777),
        mtime, (unsigned long)st.st_ino,
        (unsigned long)st.st_nlink);

    u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)len);
    if (!dst) return SEA_ERR_ARENA_FULL;
    output->data = dst; output->len = (u32)len;
    return SEA_OK;
}
