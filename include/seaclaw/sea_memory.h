/*
 * sea_memory.h — Long-Term Memory
 *
 * Persistent memory stored as markdown files in the workspace.
 * Includes long-term memory (MEMORY.md), daily notes
 * (YYYYMM/YYYYMMDD.md), and bootstrap personality files.
 *
 * The agent reads and writes memory through file tools,
 * and memory context is injected into the system prompt.
 *
 * "The Vault remembers everything worth remembering."
 */

#ifndef SEA_MEMORY_H
#define SEA_MEMORY_H

#include "sea_types.h"
#include "sea_arena.h"

/* ── Workspace Layout ─────────────────────────────────────── */
/*
 * ~/.seaclaw/
 *   MEMORY.md          — Long-term memory (facts, preferences)
 *   IDENTITY.md        — Agent identity and personality
 *   USER.md            — User profile and preferences
 *   SOUL.md            — Agent behavioral guidelines
 *   AGENTS.md          — Known agents and capabilities
 *   notes/
 *     202602/
 *       20260211.md    — Daily notes for Feb 11, 2026
 *       20260212.md    — Daily notes for Feb 12, 2026
 */

#define SEA_MEMORY_DIR          ".seaclaw"
#define SEA_MEMORY_FILE         "MEMORY.md"
#define SEA_MEMORY_IDENTITY     "IDENTITY.md"
#define SEA_MEMORY_USER         "USER.md"
#define SEA_MEMORY_SOUL         "SOUL.md"
#define SEA_MEMORY_AGENTS       "AGENTS.md"
#define SEA_MEMORY_NOTES_DIR    "notes"

/* ── Memory Manager ───────────────────────────────────────── */

typedef struct {
    char        workspace[4096];    /* Full path to workspace dir  */
    SeaArena    arena;              /* Arena for loaded content     */
    bool        initialized;
} SeaMemory;

/* ── API ──────────────────────────────────────────────────── */

/* Initialize memory system. Creates workspace dir if needed. */
SeaError sea_memory_init(SeaMemory* mem, const char* workspace_path, u64 arena_size);

/* Destroy memory system. */
void sea_memory_destroy(SeaMemory* mem);

/* Read long-term memory (MEMORY.md). Returns content or NULL. */
const char* sea_memory_read(SeaMemory* mem);

/* Write/overwrite long-term memory (MEMORY.md). */
SeaError sea_memory_write(SeaMemory* mem, const char* content);

/* Append to long-term memory. */
SeaError sea_memory_append(SeaMemory* mem, const char* content);

/* Read a bootstrap file (IDENTITY.md, USER.md, etc.). */
const char* sea_memory_read_bootstrap(SeaMemory* mem, const char* filename);

/* Write a bootstrap file. */
SeaError sea_memory_write_bootstrap(SeaMemory* mem, const char* filename,
                                     const char* content);

/* Read today's daily note. Returns content or NULL. */
const char* sea_memory_read_daily(SeaMemory* mem);

/* Append to today's daily note. Creates file/dirs if needed. */
SeaError sea_memory_append_daily(SeaMemory* mem, const char* content);

/* Read a specific day's note (YYYYMMDD format). */
const char* sea_memory_read_daily_for(SeaMemory* mem, const char* date_str);

/* Read recent N days of notes. Returns concatenated content. */
const char* sea_memory_read_recent_notes(SeaMemory* mem, u32 days);

/* Build the full memory context string for injection into system prompt.
 * Includes: IDENTITY + SOUL + USER + MEMORY + recent daily notes.
 * All allocated in the provided arena. */
const char* sea_memory_build_context(SeaMemory* mem, SeaArena* arena);

/* Create default bootstrap files if they don't exist. */
SeaError sea_memory_create_defaults(SeaMemory* mem);

/* Get workspace path. */
const char* sea_memory_workspace(SeaMemory* mem);

#endif /* SEA_MEMORY_H */
