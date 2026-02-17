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

/* Tool categories for selective injection */
typedef enum {
    SEA_TOOL_CAT_CORE     = 1 << 0,  /* Always included: echo, system_status */
    SEA_TOOL_CAT_FILE     = 1 << 1,  /* File operations: read, write, info */
    SEA_TOOL_CAT_SHELL    = 1 << 2,  /* Shell execution */
    SEA_TOOL_CAT_WEB      = 1 << 3,  /* HTTP, web fetch, search */
    SEA_TOOL_CAT_TEXT     = 1 << 4,  /* Text processing: grep, diff, transform */
    SEA_TOOL_CAT_DATA     = 1 << 5,  /* JSON, CSV, database */
    SEA_TOOL_CAT_SYSTEM   = 1 << 6,  /* Process, network, disk info */
    SEA_TOOL_CAT_TIME     = 1 << 7,  /* Timestamp, calendar, cron */
    SEA_TOOL_CAT_CRYPTO   = 1 << 8,  /* Hash, encode, password gen */
    SEA_TOOL_CAT_TASK     = 1 << 9,  /* Task management, memory */
} SeaToolCategory;

typedef struct {
    u32              id;
    const char*      name;
    const char*      description;
    SeaToolFunc      func;
    SeaToolCategory  category;  /* Tool category for selective injection */
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
