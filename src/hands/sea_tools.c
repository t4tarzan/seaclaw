/*
 * sea_tools.c — Static tool registry implementation
 *
 * All tools are compiled in. AI cannot invent new tools at runtime.
 */

#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_log.h"
#include <stdio.h>
#include <string.h>

/* ── Forward declarations for tool implementations ────────── */

extern SeaError tool_echo(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_system_status(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_file_read(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_file_write(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_shell_exec(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_web_fetch(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_task_manage(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_db_query(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_exa_search(SeaSlice args, SeaArena* arena, SeaSlice* output);

/* ── The Static Registry ──────────────────────────────────── */

static const SeaTool s_registry[] = {
    { 1, "echo",          "Echo text back",                              tool_echo },
    { 2, "system_status", "Report memory usage and uptime",              tool_system_status },
    { 3, "file_read",     "Read a file. Args: file_path",                tool_file_read },
    { 4, "file_write",    "Write a file. Args: path|content",            tool_file_write },
    { 5, "shell_exec",    "Run a shell command. Args: command",          tool_shell_exec },
    { 6, "web_fetch",     "Fetch a URL. Args: url",                      tool_web_fetch },
    { 7, "task_manage",   "Manage tasks. Args: list|create|title|desc|done|id", tool_task_manage },
    { 8, "db_query",      "Query database (read-only). Args: SELECT SQL",        tool_db_query },
    { 9, "exa_search",    "Web search via Exa. Args: search query",              tool_exa_search },
};

static const u32 s_registry_count = sizeof(s_registry) / sizeof(s_registry[0]);

/* ── API ──────────────────────────────────────────────────── */

void sea_tools_init(void) {
    SEA_LOG_INFO("HANDS", "Tool registry loaded: %u tools", s_registry_count);
}

u32 sea_tools_count(void) {
    return s_registry_count;
}

const SeaTool* sea_tool_by_name(const char* name) {
    for (u32 i = 0; i < s_registry_count; i++) {
        if (strcmp(s_registry[i].name, name) == 0) {
            return &s_registry[i];
        }
    }
    return NULL;
}

const SeaTool* sea_tool_by_id(u32 id) {
    for (u32 i = 0; i < s_registry_count; i++) {
        if (s_registry[i].id == id) {
            return &s_registry[i];
        }
    }
    return NULL;
}

SeaError sea_tool_exec(const char* name, SeaSlice args, SeaArena* arena, SeaSlice* output) {
    const SeaTool* tool = sea_tool_by_name(name);
    if (!tool) return SEA_ERR_TOOL_NOT_FOUND;

    SEA_LOG_INFO("HANDS", "Executing tool: %s", tool->name);
    SeaError err = tool->func(args, arena, output);

    if (err != SEA_OK) {
        SEA_LOG_ERROR("HANDS", "Tool '%s' failed: %s", tool->name, sea_error_str(err));
    }
    return err;
}

void sea_tools_list(void) {
    printf("  %-4s %-20s %s\n", "ID", "Name", "Description");
    printf("  %-4s %-20s %s\n", "──", "────────────────────", "───────────────────────────");
    for (u32 i = 0; i < s_registry_count; i++) {
        printf("  %-4u %-20s %s\n",
               s_registry[i].id,
               s_registry[i].name,
               s_registry[i].description);
    }
}
