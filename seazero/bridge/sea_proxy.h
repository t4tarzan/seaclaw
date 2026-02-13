/*
 * sea_proxy.h — SeaZero LLM Proxy
 *
 * Lightweight HTTP server on port 7432 that proxies LLM requests
 * from Agent Zero to the real LLM provider. Agent Zero thinks it's
 * talking to OpenAI; SeaClaw validates, budgets, and forwards.
 *
 * Design:
 *   - POSIX sockets, single-threaded accept loop in a background thread
 *   - Validates internal token on every request
 *   - Checks daily token budget before forwarding
 *   - Forwards to real LLM using sea_http (existing, no new deps)
 *   - Logs all usage to seazero_llm_usage table
 *   - No new dependencies: uses sys/socket.h + pthread (already linked)
 */

#ifndef SEA_PROXY_H
#define SEA_PROXY_H

#include "seaclaw/sea_types.h"
#include "seaclaw/sea_arena.h"
#include "seaclaw/sea_db.h"

/* ── Proxy Configuration ───────────────────────────────────── */

typedef struct {
    u16         port;               /* Listen port (default: 7432)         */
    const char* internal_token;     /* Token Agent Zero uses to auth       */
    const char* real_api_url;       /* Real LLM endpoint to forward to     */
    const char* real_api_key;       /* Real API key (never exposed)        */
    const char* real_provider;      /* Provider name for logging           */
    const char* real_model;         /* Model name for logging              */
    i64         daily_token_budget; /* Max tokens/day for agents (0=unlimited) */
    SeaDb*      db;                 /* Database handle for usage logging   */
    bool        enabled;            /* false = proxy not started           */
} SeaProxyConfig;

/* ── Lifecycle ─────────────────────────────────────────────── */

/* Start the proxy server in a background thread.
 * Returns 0 on success, -1 on error. */
int sea_proxy_start(const SeaProxyConfig* cfg);

/* Stop the proxy server gracefully. */
void sea_proxy_stop(void);

/* Check if the proxy is running. */
bool sea_proxy_running(void);

#endif /* SEA_PROXY_H */
