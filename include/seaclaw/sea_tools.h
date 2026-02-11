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

/* Initialize the tool registry. Call once at startup. */
void sea_tools_init(void);

/* Get tool count */
u32 sea_tools_count(void);

/* Lookup by name — O(1) scan of static array */
const SeaTool* sea_tool_by_name(const char* name);

/* Lookup by id */
const SeaTool* sea_tool_by_id(u32 id);

/* Execute a tool by name */
SeaError sea_tool_exec(const char* name, SeaSlice args, SeaArena* arena, SeaSlice* output);

/* List all tools (for /tools command) */
void sea_tools_list(void);

#endif /* SEA_TOOLS_H */
