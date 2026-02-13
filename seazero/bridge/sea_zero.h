/*
 * sea_zero.h — SeaZero Bridge API
 *
 * Thin IPC layer between SeaClaw (C11) and Agent Zero (Python/Docker).
 * Agent Zero runs in a Docker container exposing HTTP on localhost:8080.
 * SeaClaw calls it like any other HTTP tool using sea_http.
 *
 * Design:
 *   - SeaClaw stays sovereign: no new dependencies, uses existing sea_http
 *   - Agent Zero stays isolated: Docker container, capability-dropped
 *   - Bridge is stateless: each call is an independent HTTP request
 *   - All responses pass through Grammar Shield before reaching the user
 */

#ifndef SEA_ZERO_H
#define SEA_ZERO_H

#include "seaclaw/sea_types.h"
#include "seaclaw/sea_arena.h"

/* ── Agent Zero Connection ─────────────────────────────────── */

typedef struct {
    const char* agent_url;      /* e.g. "http://localhost:8080"       */
    const char* agent_id;       /* e.g. "agent-0"                     */
    u32         timeout_sec;    /* HTTP timeout (default: 120)        */
    bool        enabled;        /* false = SeaZero disabled, no-op    */
} SeaZeroConfig;

/* Initialize SeaZero config with defaults.
 * Reads from config JSON if seazero_enabled is present.
 * Returns false if SeaZero is disabled (not an error). */
bool sea_zero_init(SeaZeroConfig* cfg, const char* agent_url);

/* ── Task Delegation ───────────────────────────────────────── */

typedef struct {
    const char* task;           /* Natural language task description   */
    const char* context;        /* Optional: conversation context      */
    u32         max_steps;      /* Max autonomous steps (default: 10)  */
    u32         timeout_sec;    /* Per-task timeout override           */
} SeaZeroTask;

typedef struct {
    bool        success;        /* true if Agent Zero completed task   */
    const char* result;         /* Agent Zero's response (arena-alloc) */
    const char* error;          /* Error message if !success           */
    u32         steps_taken;    /* How many steps Agent Zero took      */
    f64         elapsed_sec;    /* Wall-clock time                     */
} SeaZeroResult;

/* Send a task to Agent Zero and wait for the result.
 * The result string is allocated in the provided arena.
 * Returns SeaZeroResult with success=false on error. */
SeaZeroResult sea_zero_delegate(
    const SeaZeroConfig* cfg,
    const SeaZeroTask*   task,
    SeaArena*            arena
);

/* ── Health Check ──────────────────────────────────────────── */

typedef struct {
    bool        reachable;      /* Agent Zero container is up          */
    const char* agent_id;       /* Reported agent ID                   */
    const char* status;         /* "ready", "busy", "error"            */
    u32         active_tasks;   /* Number of tasks in progress         */
} SeaZeroHealth;

/* Check if Agent Zero is reachable and ready.
 * Non-blocking, fast timeout (5 seconds). */
SeaZeroHealth sea_zero_health(const SeaZeroConfig* cfg, SeaArena* arena);

/* ── Tool Registration ─────────────────────────────────────── */

/* Register "agent_zero" as a SeaClaw tool.
 * After this, the LLM can call it like any other tool:
 *   {"tool": "agent_zero", "params": {"task": "..."}}
 *
 * This is the main integration point — called once at startup
 * if SeaZero is enabled. */
int sea_zero_register_tool(const SeaZeroConfig* cfg);

#endif /* SEA_ZERO_H */
