/*
 * sea_api.h — Lightweight HTTP API Server
 *
 * REST endpoint for external integrations:
 *   POST /api/chat    — Send message, get agent response
 *   GET  /api/health  — Health check
 *
 * Listens on localhost:8899 by default (SEA_API_PORT env var).
 */

#ifndef SEA_API_H
#define SEA_API_H

#include "sea_types.h"
#include "sea_agent.h"

typedef struct {
    u16             port;       /* Listen port (default: 8899) */
    SeaAgentConfig* agent_cfg;  /* Pointer to agent config     */
} SeaApiConfig;

/* Start the API server in a background thread. Returns 0 on success. */
int sea_api_start(const SeaApiConfig* cfg);

/* Stop the API server gracefully. */
void sea_api_stop(void);

/* Check if the API server is running. */
bool sea_api_running(void);

#endif /* SEA_API_H */
