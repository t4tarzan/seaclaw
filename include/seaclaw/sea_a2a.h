/*
 * sea_a2a.h — Agent-to-Agent Communication Protocol
 *
 * Delegate tasks to remote agents (OpenClaw, Agent-0, other Sea-Claw)
 * over HTTP JSON-RPC. Verify results via Shield before returning.
 *
 * "No agent is an island. The sovereign delegates, but always verifies."
 */

#ifndef SEA_A2A_H
#define SEA_A2A_H

#include "sea_types.h"
#include "sea_arena.h"

/* ── Message Types ───────────────────────────────────────── */

typedef enum {
    SEA_A2A_DELEGATE  = 0,  /* Send task to remote agent */
    SEA_A2A_RESULT    = 1,  /* Result from remote agent */
    SEA_A2A_HEARTBEAT = 2,  /* Health check */
    SEA_A2A_DISCOVER  = 3,  /* Find available agents */
    SEA_A2A_CANCEL    = 4,  /* Cancel a delegated task */
} SeaA2aType;

/* ── Agent Peer ──────────────────────────────────────────── */

typedef struct {
    const char* name;       /* "openclaw-vps1", "agent0-docker" */
    const char* endpoint;   /* "https://vps.example.com:8080/a2a" */
    const char* api_key;    /* Optional auth token */
    bool        healthy;    /* Last heartbeat status */
    u64         last_seen;  /* Timestamp of last heartbeat */
} SeaA2aPeer;

/* ── Delegation Request ──────────────────────────────────── */

typedef struct {
    const char* task_id;    /* UUID for tracking */
    const char* task_desc;  /* Natural language task description */
    const char* context;    /* Optional context/data */
    u32         timeout_ms; /* Max wait time (default: 30000) */
} SeaA2aRequest;

/* ── Delegation Result ───────────────────────────────────── */

typedef struct {
    const char* task_id;
    bool        success;
    const char* output;     /* Result text */
    u32         latency_ms;
    const char* agent_name; /* Which agent handled it */
    bool        verified;   /* Shield-verified output */
    const char* error;      /* Error message if !success */
} SeaA2aResult;

/* ── API ──────────────────────────────────────────────────── */

/* Delegate a task to a remote agent.
 * Sends HTTP POST to peer endpoint, waits for result.
 * Shield-validates the response before returning. */
SeaA2aResult sea_a2a_delegate(const SeaA2aPeer* peer,
                               const SeaA2aRequest* req,
                               SeaArena* arena);

/* Send heartbeat to a peer. Returns true if peer is alive. */
bool sea_a2a_heartbeat(const SeaA2aPeer* peer, SeaArena* arena);

/* Discover agents on a network endpoint.
 * Returns count of peers found. */
i32 sea_a2a_discover(const char* discovery_url,
                     SeaA2aPeer* out, i32 max_peers,
                     SeaArena* arena);

#endif /* SEA_A2A_H */
