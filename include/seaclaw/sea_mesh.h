/*
 * sea_mesh.h — Distributed Agent Mesh
 *
 * Captain/Crew architecture for local-network agent coordination.
 * Captain: runs LLM, routes tasks, manages node registry.
 * Crew: lightweight worker nodes that execute tools locally.
 *
 * All communication stays within the LAN. Zero data leakage.
 *
 * "The fleet moves as one. The Captain thinks. The Crew acts."
 */

#ifndef SEA_MESH_H
#define SEA_MESH_H

#include "sea_types.h"
#include "sea_arena.h"
#include "sea_db.h"

/* ── Node Role ───────────────────────────────────────────── */

typedef enum {
    SEA_MESH_CAPTAIN = 0,   /* Central node: LLM + routing          */
    SEA_MESH_CREW    = 1,   /* Worker node: tool execution           */
} SeaMeshRole;

/* ── Node Info ───────────────────────────────────────────── */

#define SEA_MESH_MAX_CAPABILITIES 32
#define SEA_MESH_MAX_NODES        64
#define SEA_MESH_NODE_NAME_MAX    64

typedef struct {
    char        name[SEA_MESH_NODE_NAME_MAX];
    char        endpoint[256];          /* http://ip:port              */
    char        capabilities[SEA_MESH_MAX_CAPABILITIES][64]; /* tool names */
    u32         capability_count;
    bool        healthy;
    u64         last_heartbeat;         /* ms since epoch              */
    u64         registered_at;
    u32         tasks_completed;
    u32         tasks_failed;
} SeaMeshNode;

/* ── Mesh Configuration ──────────────────────────────────── */

typedef struct {
    SeaMeshRole role;
    char        node_name[SEA_MESH_NODE_NAME_MAX];
    u16         port;                   /* Listen port (9100/9101)     */
    char        captain_url[256];       /* Crew: captain endpoint      */
    char        shared_secret[128];     /* HMAC auth secret            */
    char        allowed_subnet[64];     /* e.g. "192.168.1.0/24"      */
    u32         heartbeat_interval_ms;  /* Default: 30000              */
    u32         task_timeout_ms;        /* Default: 60000              */
} SeaMeshConfig;

/* ── Mesh Engine ─────────────────────────────────────────── */

typedef struct {
    SeaMeshConfig config;
    SeaMeshNode   nodes[SEA_MESH_MAX_NODES];
    u32           node_count;
    SeaDb*        db;
    bool          running;
    bool          initialized;
} SeaMesh;

/* ── Task Dispatch ───────────────────────────────────────── */

typedef struct {
    const char* task_id;
    const char* tool_name;
    const char* tool_args;
    const char* requester;      /* Node that requested this */
    u32         timeout_ms;
} SeaMeshTask;

typedef struct {
    const char* task_id;
    bool        success;
    const char* output;
    const char* node_name;      /* Which node executed */
    u32         latency_ms;
    const char* error;
} SeaMeshResult;

/* ── API ──────────────────────────────────────────────────── */

/* Initialize mesh engine. */
SeaError sea_mesh_init(SeaMesh* mesh, SeaMeshConfig* config, SeaDb* db);

/* Destroy mesh engine. */
void sea_mesh_destroy(SeaMesh* mesh);

/* Captain: register a crew node. */
SeaError sea_mesh_register_node(SeaMesh* mesh, const char* name,
                                 const char* endpoint,
                                 const char** capabilities, u32 cap_count);

/* Captain: remove a node. */
SeaError sea_mesh_remove_node(SeaMesh* mesh, const char* name);

/* Captain: find the best node for a tool. Returns node or NULL. */
const SeaMeshNode* sea_mesh_route_tool(SeaMesh* mesh, const char* tool_name);

/* Captain: dispatch a tool call to the best node. */
SeaMeshResult sea_mesh_dispatch(SeaMesh* mesh, SeaMeshTask* task,
                                 SeaArena* arena);

/* Crew: register with captain. */
SeaError sea_mesh_crew_register(SeaMesh* mesh, SeaArena* arena);

/* Crew: send heartbeat to captain. */
SeaError sea_mesh_crew_heartbeat(SeaMesh* mesh, SeaArena* arena);

/* Captain: process heartbeat from a node. */
SeaError sea_mesh_process_heartbeat(SeaMesh* mesh, const char* node_name);

/* Captain: get list of healthy nodes. */
u32 sea_mesh_healthy_nodes(SeaMesh* mesh, const SeaMeshNode** out, u32 max);

/* Captain: broadcast a message to all nodes. */
SeaError sea_mesh_broadcast(SeaMesh* mesh, const char* message, SeaArena* arena);

/* Get node count. */
u32 sea_mesh_node_count(SeaMesh* mesh);

/* Build mesh status string. */
const char* sea_mesh_status(SeaMesh* mesh, SeaArena* arena);

/* Validate HMAC token for a request. */
bool sea_mesh_validate_token(SeaMesh* mesh, const char* token);

/* Generate HMAC token for outgoing request. */
const char* sea_mesh_generate_token(SeaMesh* mesh, SeaArena* arena);

#endif /* SEA_MESH_H */
