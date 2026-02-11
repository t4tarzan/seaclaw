/*
 * sea_memory.c — Long-Term Memory Implementation
 *
 * File-based persistent memory using markdown files.
 * Workspace lives at ~/.seaclaw/ by default.
 */

#include "seaclaw/sea_memory.h"
#include "seaclaw/sea_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>

/* ── Helpers ──────────────────────────────────────────────── */

static void ensure_dir(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        mkdir(path, 0755);
    }
}

static const char* read_file_to_arena(SeaArena* arena, const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) { fclose(f); return NULL; }
    if (size > 1024 * 1024) size = 1024 * 1024; /* Cap at 1MB */

    char* buf = (char*)sea_arena_alloc(arena, (u64)size + 1, 1);
    if (!buf) { fclose(f); return NULL; }

    size_t read = fread(buf, 1, (size_t)size, f);
    buf[read] = '\0';
    fclose(f);
    return buf;
}

static SeaError write_file_content(const char* path, const char* content, bool append) {
    FILE* f = fopen(path, append ? "a" : "w");
    if (!f) return SEA_ERR_IO;
    if (content) {
        fputs(content, f);
    }
    fclose(f);
    return SEA_OK;
}

static void build_path(char* out, u32 out_size, const char* dir, const char* file) {
    snprintf(out, out_size, "%s/%s", dir, file);
}

/* ── Init / Destroy ───────────────────────────────────────── */

SeaError sea_memory_init(SeaMemory* mem, const char* workspace_path, u64 arena_size) {
    if (!mem) return SEA_ERR_INVALID_INPUT;

    memset(mem, 0, sizeof(SeaMemory));

    /* Determine workspace path */
    if (workspace_path) {
        strncpy(mem->workspace, workspace_path, sizeof(mem->workspace) - 1);
    } else {
        const char* home = getenv("HOME");
        if (!home) home = "/tmp";
        snprintf(mem->workspace, sizeof(mem->workspace), "%s/%s", home, SEA_MEMORY_DIR);
    }

    /* Create workspace directory */
    ensure_dir(mem->workspace);

    /* Create notes subdirectory */
    char notes_dir[4096];
    build_path(notes_dir, sizeof(notes_dir), mem->workspace, SEA_MEMORY_NOTES_DIR);
    ensure_dir(notes_dir);

    SeaError err = sea_arena_create(&mem->arena, arena_size);
    if (err != SEA_OK) return err;

    mem->initialized = true;

    SEA_LOG_INFO("MEMORY", "Workspace: %s", mem->workspace);
    return SEA_OK;
}

void sea_memory_destroy(SeaMemory* mem) {
    if (!mem) return;
    sea_arena_destroy(&mem->arena);
    mem->initialized = false;
}

/* ── Long-Term Memory ─────────────────────────────────────── */

const char* sea_memory_read(SeaMemory* mem) {
    if (!mem || !mem->initialized) return NULL;
    char path[4096];
    build_path(path, sizeof(path), mem->workspace, SEA_MEMORY_FILE);
    sea_arena_reset(&mem->arena);
    return read_file_to_arena(&mem->arena, path);
}

SeaError sea_memory_write(SeaMemory* mem, const char* content) {
    if (!mem || !mem->initialized) return SEA_ERR_INVALID_INPUT;
    char path[4096];
    build_path(path, sizeof(path), mem->workspace, SEA_MEMORY_FILE);
    return write_file_content(path, content, false);
}

SeaError sea_memory_append(SeaMemory* mem, const char* content) {
    if (!mem || !mem->initialized) return SEA_ERR_INVALID_INPUT;
    char path[4096];
    build_path(path, sizeof(path), mem->workspace, SEA_MEMORY_FILE);
    return write_file_content(path, content, true);
}

/* ── Bootstrap Files ──────────────────────────────────────── */

const char* sea_memory_read_bootstrap(SeaMemory* mem, const char* filename) {
    if (!mem || !mem->initialized || !filename) return NULL;
    char path[4096];
    build_path(path, sizeof(path), mem->workspace, filename);
    return read_file_to_arena(&mem->arena, path);
}

SeaError sea_memory_write_bootstrap(SeaMemory* mem, const char* filename,
                                     const char* content) {
    if (!mem || !mem->initialized || !filename) return SEA_ERR_INVALID_INPUT;
    char path[4096];
    build_path(path, sizeof(path), mem->workspace, filename);
    return write_file_content(path, content, false);
}

/* ── Daily Notes ──────────────────────────────────────────── */

static void today_str(char* buf, u32 buf_size) {
    time_t now = time(NULL);
    struct tm* tm = localtime(&now);
    snprintf(buf, buf_size, "%04d%02d%02d",
             (int)(tm->tm_year + 1900), (int)(tm->tm_mon + 1), (int)tm->tm_mday);
}

/* today_month_str removed — unused, month extracted from date_str in daily_note_path */

static void daily_note_path(SeaMemory* mem, const char* date_str, char* out, u32 out_size) {
    /* Extract month dir (first 6 chars of YYYYMMDD) */
    char month[8];
    strncpy(month, date_str, 6);
    month[6] = '\0';

    char month_dir[4200];
    snprintf(month_dir, sizeof(month_dir), "%.*s/%s/%s",
             (int)(sizeof(mem->workspace) - 1), mem->workspace,
             SEA_MEMORY_NOTES_DIR, month);
    ensure_dir(month_dir);

    snprintf(out, out_size, "%s/%s.md", month_dir, date_str);
}

const char* sea_memory_read_daily(SeaMemory* mem) {
    if (!mem || !mem->initialized) return NULL;
    char date[16];
    today_str(date, sizeof(date));
    return sea_memory_read_daily_for(mem, date);
}

SeaError sea_memory_append_daily(SeaMemory* mem, const char* content) {
    if (!mem || !mem->initialized || !content) return SEA_ERR_INVALID_INPUT;

    char date[16];
    today_str(date, sizeof(date));

    char path[4096];
    daily_note_path(mem, date, path, sizeof(path));

    /* Check if file exists; if not, add header */
    struct stat st;
    bool is_new = (stat(path, &st) != 0);

    FILE* f = fopen(path, "a");
    if (!f) return SEA_ERR_IO;

    if (is_new) {
        fprintf(f, "# Daily Notes — %c%c%c%c-%c%c-%c%c\n\n",
                date[0], date[1], date[2], date[3],
                date[4], date[5], date[6], date[7]);
    }

    /* Add timestamp */
    time_t now = time(NULL);
    struct tm* tm = localtime(&now);
    fprintf(f, "## %02d:%02d\n\n%s\n\n", tm->tm_hour, tm->tm_min, content);

    fclose(f);
    return SEA_OK;
}

const char* sea_memory_read_daily_for(SeaMemory* mem, const char* date_str) {
    if (!mem || !mem->initialized || !date_str) return NULL;
    char path[4096];
    daily_note_path(mem, date_str, path, sizeof(path));
    return read_file_to_arena(&mem->arena, path);
}

const char* sea_memory_read_recent_notes(SeaMemory* mem, u32 days) {
    if (!mem || !mem->initialized || days == 0) return NULL;

    /* Build concatenated notes for last N days */
    char* result = (char*)sea_arena_alloc(&mem->arena, 64 * 1024, 1); /* 64KB max */
    if (!result) return NULL;
    u32 pos = 0;

    time_t now = time(NULL);
    for (u32 d = 0; d < days; d++) {
        time_t day = now - (time_t)d * 86400;
        struct tm* tm = localtime(&day);
        char date[32];
        snprintf(date, sizeof(date), "%04d%02d%02d",
                 (int)(tm->tm_year + 1900), (int)(tm->tm_mon + 1), (int)tm->tm_mday);

        char path[4096];
        daily_note_path(mem, date, path, sizeof(path));

        FILE* f = fopen(path, "r");
        if (!f) continue;

        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);

        if (size > 0 && pos + (u32)size < 64 * 1024 - 1) {
            size_t read = fread(result + pos, 1, (size_t)size, f);
            pos += (u32)read;
            result[pos++] = '\n';
        }
        fclose(f);
    }
    result[pos] = '\0';

    return pos > 0 ? result : NULL;
}

/* ── Build Context ────────────────────────────────────────── */

const char* sea_memory_build_context(SeaMemory* mem, SeaArena* arena) {
    if (!mem || !mem->initialized || !arena) return NULL;

    char* ctx = (char*)sea_arena_alloc(arena, 32 * 1024, 1); /* 32KB max */
    if (!ctx) return NULL;
    int pos = 0;

    /* Read each bootstrap file */
    char path[4096];

    /* IDENTITY.md */
    build_path(path, sizeof(path), mem->workspace, SEA_MEMORY_IDENTITY);
    const char* identity = read_file_to_arena(arena, path);
    if (identity) {
        pos += snprintf(ctx + pos, 32 * 1024 - (size_t)pos,
            "## Identity\n%s\n\n", identity);
    }

    /* SOUL.md */
    build_path(path, sizeof(path), mem->workspace, SEA_MEMORY_SOUL);
    const char* soul = read_file_to_arena(arena, path);
    if (soul) {
        pos += snprintf(ctx + pos, 32 * 1024 - (size_t)pos,
            "## Behavioral Guidelines\n%s\n\n", soul);
    }

    /* USER.md */
    build_path(path, sizeof(path), mem->workspace, SEA_MEMORY_USER);
    const char* user = read_file_to_arena(arena, path);
    if (user) {
        pos += snprintf(ctx + pos, 32 * 1024 - (size_t)pos,
            "## User Profile\n%s\n\n", user);
    }

    /* MEMORY.md */
    build_path(path, sizeof(path), mem->workspace, SEA_MEMORY_FILE);
    const char* memory = read_file_to_arena(arena, path);
    if (memory) {
        pos += snprintf(ctx + pos, 32 * 1024 - (size_t)pos,
            "## Long-Term Memory\n%s\n\n", memory);
    }

    /* AGENTS.md */
    build_path(path, sizeof(path), mem->workspace, SEA_MEMORY_AGENTS);
    const char* agents = read_file_to_arena(arena, path);
    if (agents) {
        pos += snprintf(ctx + pos, 32 * 1024 - (size_t)pos,
            "## Known Agents\n%s\n\n", agents);
    }

    /* Recent daily notes (last 3 days) */
    char date[32];
    time_t now = time(NULL);
    for (u32 d = 0; d < 3 && pos < 30 * 1024; d++) {
        time_t day = now - (time_t)d * 86400;
        struct tm* tm = localtime(&day);
        snprintf(date, sizeof(date), "%04d%02d%02d",
                 (int)(tm->tm_year + 1900), (int)(tm->tm_mon + 1), (int)tm->tm_mday);

        char note_path[4096];
        daily_note_path(mem, date, note_path, sizeof(note_path));
        const char* note = read_file_to_arena(arena, note_path);
        if (note) {
            pos += snprintf(ctx + pos, 32 * 1024 - (size_t)pos,
                "## Daily Notes (%c%c%c%c-%c%c-%c%c)\n%s\n\n",
                date[0], date[1], date[2], date[3],
                date[4], date[5], date[6], date[7], note);
        }
    }

    return pos > 0 ? ctx : NULL;
}

/* ── Create Defaults ──────────────────────────────────────── */

SeaError sea_memory_create_defaults(SeaMemory* mem) {
    if (!mem || !mem->initialized) return SEA_ERR_INVALID_INPUT;

    char path[4096];
    struct stat st;

    /* IDENTITY.md */
    build_path(path, sizeof(path), mem->workspace, SEA_MEMORY_IDENTITY);
    if (stat(path, &st) != 0) {
        write_file_content(path,
            "# Identity\n\n"
            "I am Sea-Claw, a sovereign AI assistant built in pure C11.\n"
            "I run on minimal resources with maximum security.\n"
            "I use arena allocation (zero memory leaks), grammar-constrained\n"
            "input validation, and a static tool registry.\n\n"
            "My core philosophy: \"We stop building software that breaks.\n"
            "We start building logic that survives.\"\n",
            false);
        SEA_LOG_INFO("MEMORY", "Created default %s", SEA_MEMORY_IDENTITY);
    }

    /* SOUL.md */
    build_path(path, sizeof(path), mem->workspace, SEA_MEMORY_SOUL);
    if (stat(path, &st) != 0) {
        write_file_content(path,
            "# Soul\n\n"
            "## Principles\n"
            "- Be concise and direct\n"
            "- Prefer action over explanation\n"
            "- Use tools when they help\n"
            "- Remember context from previous conversations\n"
            "- Protect user data and privacy\n"
            "- Admit uncertainty rather than guess\n\n"
            "## Tone\n"
            "- Professional but approachable\n"
            "- Technical when needed, simple when possible\n"
            "- No unnecessary filler or pleasantries\n",
            false);
        SEA_LOG_INFO("MEMORY", "Created default %s", SEA_MEMORY_SOUL);
    }

    /* USER.md */
    build_path(path, sizeof(path), mem->workspace, SEA_MEMORY_USER);
    if (stat(path, &st) != 0) {
        write_file_content(path,
            "# User Profile\n\n"
            "No user profile configured yet.\n"
            "I will learn about you as we interact.\n",
            false);
        SEA_LOG_INFO("MEMORY", "Created default %s", SEA_MEMORY_USER);
    }

    /* AGENTS.md */
    build_path(path, sizeof(path), mem->workspace, SEA_MEMORY_AGENTS);
    if (stat(path, &st) != 0) {
        write_file_content(path,
            "# Known Agents\n\n"
            "No remote agents configured yet.\n"
            "Use /delegate <endpoint> <task> to connect to other agents.\n",
            false);
        SEA_LOG_INFO("MEMORY", "Created default %s", SEA_MEMORY_AGENTS);
    }

    /* MEMORY.md */
    build_path(path, sizeof(path), mem->workspace, SEA_MEMORY_FILE);
    if (stat(path, &st) != 0) {
        write_file_content(path,
            "# Long-Term Memory\n\n"
            "This file stores persistent facts and context.\n"
            "I will update it as I learn important information.\n",
            false);
        SEA_LOG_INFO("MEMORY", "Created default %s", SEA_MEMORY_FILE);
    }

    return SEA_OK;
}

const char* sea_memory_workspace(SeaMemory* mem) {
    return mem ? mem->workspace : NULL;
}
