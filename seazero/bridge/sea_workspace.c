/*
 * sea_workspace.c — SeaZero Shared Workspace Manager
 *
 * Manages task directories under ~/.seazero/workspace/<task-id>/
 * Pure POSIX: dirent.h, stat, mkdir. No new dependencies.
 */

#include "sea_workspace.h"
#include "seaclaw/sea_log.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <limits.h>

#define WS_DEFAULT_BASE     ".seazero/workspace"
#define WS_DEFAULT_RETENTION 7       /* days */
#define WS_DEFAULT_MAX_FILE  (10 * 1024 * 1024)   /* 10MB */
#define WS_DEFAULT_MAX_TOTAL (100 * 1024 * 1024)   /* 100MB */
#define WS_PATH_MAX          1024

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* ── Global State ──────────────────────────────────────────── */

static SeaWorkspaceConfig s_ws_cfg = {0};
static char s_base_path[WS_PATH_MAX] = {0};
static bool s_ws_init = false;

/* ── Helpers ───────────────────────────────────────────────── */

static bool ensure_dir(const char* path) {
    struct stat st;
    if (stat(path, &st) == 0) return S_ISDIR(st.st_mode);
    return mkdir(path, 0755) == 0;
}

static bool ensure_dir_recursive(const char* path) {
    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char* p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (!ensure_dir(tmp)) return false;
            *p = '/';
        }
    }
    return ensure_dir(tmp);
}

/* Recursive directory removal */
static int remove_dir_recursive(const char* path) {
    DIR* d = opendir(path);
    if (!d) return -1;

    struct dirent* ent;
    int removed = 0;
    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;

        char full[PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", path, ent->d_name);

        struct stat st;
        if (stat(full, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                removed += remove_dir_recursive(full);
            } else {
                if (unlink(full) == 0) removed++;
            }
        }
    }
    closedir(d);
    rmdir(path);
    return removed;
}

/* ── Public API ────────────────────────────────────────────── */

SeaError sea_workspace_init(const SeaWorkspaceConfig* cfg) {
    if (cfg) {
        s_ws_cfg = *cfg;
    }

    /* Resolve base directory */
    if (s_ws_cfg.base_dir && s_ws_cfg.base_dir[0]) {
        snprintf(s_base_path, sizeof(s_base_path), "%s", s_ws_cfg.base_dir);
    } else {
        const char* home = getenv("HOME");
        if (!home) home = "/tmp";
        snprintf(s_base_path, sizeof(s_base_path), "%s/%s", home, WS_DEFAULT_BASE);
    }

    /* Apply defaults */
    if (s_ws_cfg.retention_days == 0) s_ws_cfg.retention_days = WS_DEFAULT_RETENTION;
    if (s_ws_cfg.max_file_size == 0)  s_ws_cfg.max_file_size  = WS_DEFAULT_MAX_FILE;
    if (s_ws_cfg.max_total_size == 0) s_ws_cfg.max_total_size = WS_DEFAULT_MAX_TOTAL;

    /* Create base directory */
    if (!ensure_dir_recursive(s_base_path)) {
        SEA_LOG_ERROR("WORKSPACE", "Cannot create base dir: %s (%s)",
                      s_base_path, strerror(errno));
        return SEA_ERR_IO;
    }

    s_ws_init = true;
    SEA_LOG_INFO("WORKSPACE", "Workspace ready: %s (retention: %ud, max: %lluMB)",
                 s_base_path, s_ws_cfg.retention_days,
                 (unsigned long long)(s_ws_cfg.max_total_size / (1024 * 1024)));

    return SEA_OK;
}

const char* sea_workspace_create(const char* task_id, SeaArena* arena) {
    if (!s_ws_init || !task_id || !arena) return NULL;

    /* Validate task_id (alphanumeric + hyphens only) */
    for (const char* p = task_id; *p; p++) {
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
              (*p >= '0' && *p <= '9') || *p == '-' || *p == '_')) {
            SEA_LOG_ERROR("WORKSPACE", "Invalid task_id: %s", task_id);
            return NULL;
        }
    }

    char path[PATH_MAX];
    int len = snprintf(path, sizeof(path), "%s/%s", s_base_path, task_id);
    if (len <= 0 || len >= (int)sizeof(path)) return NULL;

    if (!ensure_dir(path)) {
        SEA_LOG_ERROR("WORKSPACE", "Cannot create task dir: %s", path);
        return NULL;
    }

    /* Copy path to arena */
    char* result = (char*)sea_arena_push(arena, (u64)len + 1);
    if (!result) return NULL;
    memcpy(result, path, (size_t)len + 1);

    SEA_LOG_DEBUG("WORKSPACE", "Created workspace: %s", path);
    return result;
}

i32 sea_workspace_list_files(const char* task_id,
                              SeaWorkspaceFile* files, u32 max,
                              SeaArena* arena) {
    if (!s_ws_init || !task_id || !files || max == 0 || !arena) return 0;

    char dir_path[PATH_MAX];
    snprintf(dir_path, sizeof(dir_path), "%s/%s", s_base_path, task_id);

    DIR* d = opendir(dir_path);
    if (!d) return 0;

    i32 count = 0;
    struct dirent* ent;
    while ((ent = readdir(d)) != NULL && (u32)count < max) {
        if (ent->d_name[0] == '.') continue; /* Skip hidden + . + .. */

        char full[PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", dir_path, ent->d_name);

        struct stat st;
        if (stat(full, &st) != 0 || !S_ISREG(st.st_mode)) continue;

        /* Copy name to arena */
        u64 nlen = strlen(ent->d_name);
        char* name = (char*)sea_arena_push(arena, nlen + 1);
        if (!name) break;
        memcpy(name, ent->d_name, nlen + 1);

        /* Copy full path to arena */
        u64 plen = strlen(full);
        char* path = (char*)sea_arena_push(arena, plen + 1);
        if (!path) break;
        memcpy(path, full, plen + 1);

        files[count].name  = name;
        files[count].path  = path;
        files[count].size  = (u64)st.st_size;
        files[count].mtime = (i64)st.st_mtime;
        count++;
    }
    closedir(d);
    return count;
}

SeaSlice sea_workspace_read_file(const char* task_id, const char* filename,
                                  SeaArena* arena) {
    SeaSlice empty = {0};
    if (!s_ws_init || !task_id || !filename || !arena) return empty;

    /* Prevent path traversal */
    if (strstr(filename, "..") || filename[0] == '/') {
        SEA_LOG_WARN("WORKSPACE", "Path traversal attempt blocked: %s", filename);
        return empty;
    }

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s/%s", s_base_path, task_id, filename);

    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) return empty;

    /* Enforce file size limit */
    if ((u64)st.st_size > s_ws_cfg.max_file_size) {
        SEA_LOG_WARN("WORKSPACE", "File too large: %s (%lld bytes, max %llu)",
                     filename, (long long)st.st_size,
                     (unsigned long long)s_ws_cfg.max_file_size);
        return empty;
    }

    FILE* f = fopen(path, "rb");
    if (!f) return empty;

    u8* buf = (u8*)sea_arena_push(arena, (u64)st.st_size);
    if (!buf) { fclose(f); return empty; }

    size_t nread = fread(buf, 1, (size_t)st.st_size, f);
    fclose(f);

    return (SeaSlice){ .data = buf, .len = (u32)nread };
}

i32 sea_workspace_list_tasks(SeaWorkspaceTask* tasks, u32 max,
                              SeaArena* arena) {
    if (!s_ws_init || !tasks || max == 0 || !arena) return 0;

    DIR* d = opendir(s_base_path);
    if (!d) return 0;

    i32 count = 0;
    struct dirent* ent;
    while ((ent = readdir(d)) != NULL && (u32)count < max) {
        if (ent->d_name[0] == '.') continue;

        char full[PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", s_base_path, ent->d_name);

        struct stat st;
        if (stat(full, &st) != 0 || !S_ISDIR(st.st_mode)) continue;

        /* Copy task_id to arena */
        u64 nlen = strlen(ent->d_name);
        char* tid = (char*)sea_arena_push(arena, nlen + 1);
        if (!tid) break;
        memcpy(tid, ent->d_name, nlen + 1);

        /* Copy path to arena */
        u64 plen = strlen(full);
        char* path = (char*)sea_arena_push(arena, plen + 1);
        if (!path) break;
        memcpy(path, full, plen + 1);

        /* Count files and total size */
        DIR* td = opendir(full);
        u32 fc = 0;
        u64 ts = 0;
        if (td) {
            struct dirent* tent;
            while ((tent = readdir(td)) != NULL) {
                if (tent->d_name[0] == '.') continue;
                char fp[PATH_MAX];
                snprintf(fp, sizeof(fp), "%s/%s", full, tent->d_name);
                struct stat fst;
                if (stat(fp, &fst) == 0 && S_ISREG(fst.st_mode)) {
                    fc++;
                    ts += (u64)fst.st_size;
                }
            }
            closedir(td);
        }

        tasks[count].task_id    = tid;
        tasks[count].path       = path;
        tasks[count].created    = (i64)st.st_mtime;
        tasks[count].file_count = fc;
        tasks[count].total_size = ts;
        count++;
    }
    closedir(d);
    return count;
}

u32 sea_workspace_cleanup(void) {
    if (!s_ws_init) return 0;

    time_t cutoff = time(NULL) - (time_t)(s_ws_cfg.retention_days * 86400);

    DIR* d = opendir(s_base_path);
    if (!d) return 0;

    u32 removed = 0;
    struct dirent* ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;

        char full[PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", s_base_path, ent->d_name);

        struct stat st;
        if (stat(full, &st) != 0 || !S_ISDIR(st.st_mode)) continue;

        if (st.st_mtime < cutoff) {
            SEA_LOG_INFO("WORKSPACE", "Cleaning up old workspace: %s", ent->d_name);
            remove_dir_recursive(full);
            removed++;
        }
    }
    closedir(d);

    if (removed > 0) {
        SEA_LOG_INFO("WORKSPACE", "Cleaned %u old workspace(s)", removed);
    }

    return removed;
}

const char* sea_workspace_base_dir(void) {
    return s_ws_init ? s_base_path : NULL;
}
