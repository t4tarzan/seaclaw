/*
 * sea_tools.h — The Static Tool Registry
 *
 * "A genius in a straightjacket."
 * AI can ONLY press buttons we wired at compile time.
 * No dynamic code execution, no eval, no exec.
 * Tools are function pointers — one CPU JUMP instruction.
 */

#ifndef SEA_TOOLS_H
#define SEA_TOOLS_H

#include "sea_types.h"
#include "sea_arena.h"

/* Tool function signature: takes args, arena, returns output slice */
typedef SeaError (*SeaToolFunc)(SeaSlice args, SeaArena* arena, SeaSlice* output);

typedef struct {
    u32           id;
    const char*   name;
    const char*   description;
    SeaToolFunc   func;
} SeaTool;

/* SEA_MAX_TOOL_NAME is defined in sea_types.h (64) */
#define SEA_TOOL_HASH_SIZE  128   /* Must be power of 2 */
#define SEA_TOOL_DYNAMIC_MAX 64   /* Max runtime-registered tools */

/* Initialize the tool registry. Call once at startup. */
void sea_tools_init(void);

/* Get tool count (static + dynamic) */
u32 sea_tools_count(void);

/* Lookup by name — O(1) hash table */
const SeaTool* sea_tool_by_name(const char* name);

/* Lookup by id */
const SeaTool* sea_tool_by_id(u32 id);

/* Execute a tool by name */
SeaError sea_tool_exec(const char* name, SeaSlice args, SeaArena* arena, SeaSlice* output);

/* List all tools (for /tools command) */
void sea_tools_list(void);

/* ── Dynamic Tool Registration (v2) ───────────────────────── */

/* Register a tool at runtime. Returns SEA_OK or SEA_ERR_FULL.
 * The tool is added to the hash table for O(1) lookup.
 * id is auto-assigned (next available after static tools). */
SeaError sea_tool_register(const char* name, const char* description,
                            SeaToolFunc func);

/* Unregister a dynamic tool by name. Returns SEA_OK or SEA_ERR_NOT_FOUND. */
SeaError sea_tool_unregister(const char* name);

/* Get count of dynamic (runtime-registered) tools only. */
u32 sea_tools_dynamic_count(void);

#endif /* SEA_TOOLS_H */
