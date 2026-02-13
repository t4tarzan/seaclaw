/*
 * sea_workspace.h — SeaZero Shared Workspace Manager
 *
 * Manages the shared filesystem between SeaClaw and Agent Zero.
 * Each task gets its own directory under ~/.seazero/workspace/<task-id>/
 * Agent Zero writes files there; SeaClaw reads, sanitizes, and delivers.
 *
 * Design:
 *   - No new dependencies: uses POSIX dirent.h + stat
 *   - Arena-allocated file lists
 *   - Auto-cleanup of old workspaces (configurable retention)
 */

#ifndef SEA_WORKSPACE_H
#define SEA_WORKSPACE_H

#include "seaclaw/sea_types.h"
#include "seaclaw/sea_arena.h"

/* ── Workspace File Info ───────────────────────────────────── */

typedef struct {
    const char* name;       /* Filename (relative to task dir)  */
    const char* path;       /* Full absolute path               */
    u64         size;       /* File size in bytes               */
    i64         mtime;      /* Last modification time (epoch)   */
} SeaWorkspaceFile;

/* ── Workspace Task Info ───────────────────────────────────── */

typedef struct {
    const char*       task_id;      /* Task ID (directory name)       */
    const char*       path;         /* Full path to task directory    */
    i64               created;      /* Creation time (epoch)          */
    u32               file_count;   /* Number of files in workspace   */
    u64               total_size;   /* Total size of all files        */
} SeaWorkspaceTask;

/* ── Configuration ─────────────────────────────────────────── */

typedef struct {
    const char* base_dir;       /* Base workspace dir (default: ~/.seazero/workspace) */
    u32         retention_days; /* Auto-cleanup after N days (0 = never)              */
    u64         max_file_size;  /* Max single file size (default: 10MB)               */
    u64         max_total_size; /* Max total workspace size (default: 100MB)          */
} SeaWorkspaceConfig;

/* ── API ───────────────────────────────────────────────────── */

/* Initialize workspace manager. Creates base_dir if needed.
 * Returns SEA_OK on success. */
SeaError sea_workspace_init(const SeaWorkspaceConfig* cfg);

/* Create a workspace directory for a task.
 * Returns the full path (arena-allocated) or NULL on error. */
const char* sea_workspace_create(const char* task_id, SeaArena* arena);

/* List files in a task's workspace.
 * Returns number of files found, fills `files` array up to `max`. */
i32 sea_workspace_list_files(const char* task_id,
                              SeaWorkspaceFile* files, u32 max,
                              SeaArena* arena);

/* Read a file from workspace into arena. Returns slice or empty on error.
 * Enforces max_file_size limit. */
SeaSlice sea_workspace_read_file(const char* task_id, const char* filename,
                                  SeaArena* arena);

/* List all task workspaces.
 * Returns number of tasks found, fills `tasks` array up to `max`. */
i32 sea_workspace_list_tasks(SeaWorkspaceTask* tasks, u32 max,
                              SeaArena* arena);

/* Clean up workspaces older than retention_days.
 * Returns number of workspaces removed. */
u32 sea_workspace_cleanup(void);

/* Get the base workspace directory path. */
const char* sea_workspace_base_dir(void);

#endif /* SEA_WORKSPACE_H */
