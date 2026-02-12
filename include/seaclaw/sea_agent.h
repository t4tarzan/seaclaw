/*
 * sea_agent.h — The Brain
 *
 * Agent loop: takes natural language input, routes to an LLM API,
 * parses the response for tool calls, executes them, and returns
 * the final answer. Supports OpenAI-compatible APIs.
 *
 * "The genius in the straightjacket — constrained by grammar,
 *  limited to registered tools, but brilliant within those bounds."
 */

#ifndef SEA_AGENT_H
#define SEA_AGENT_H

#include "sea_types.h"
#include "sea_arena.h"

/* ── LLM Provider ─────────────────────────────────────────── */

typedef enum {
    SEA_LLM_OPENAI = 0,    /* OpenAI API (gpt-4, gpt-3.5-turbo)    */
    SEA_LLM_ANTHROPIC,     /* Anthropic API (claude-3)              */
    SEA_LLM_GEMINI,        /* Google Gemini API (gemini-2.5-pro)    */
    SEA_LLM_OPENROUTER,    /* OpenRouter API (any model)            */
    SEA_LLM_LOCAL,         /* Local OpenAI-compatible (ollama, etc) */
} SeaLlmProvider;

/* ── Think Level ─────────────────────────────────────────── */

typedef enum {
    SEA_THINK_OFF    = 0,   /* Minimal thinking, fast responses     */
    SEA_THINK_LOW    = 1,   /* Brief reasoning                      */
    SEA_THINK_MEDIUM = 2,   /* Balanced (default)                   */
    SEA_THINK_HIGH   = 3,   /* Deep reasoning, longer responses     */
} SeaThinkLevel;

/* ── Smart Router Hint ───────────────────────────────────── */

typedef enum {
    SEA_ROUTE_AUTO   = 0,   /* Auto-detect based on input           */
    SEA_ROUTE_FAST   = 1,   /* Prefer fastest/cheapest provider     */
    SEA_ROUTE_SMART  = 2,   /* Prefer most capable provider         */
    SEA_ROUTE_LOCAL  = 3,   /* Force local LLM only                 */
} SeaRouteHint;

/* ── SSE Streaming Callback ──────────────────────────────── */

/* Called for each token/chunk during streaming. Return false to abort. */
typedef bool (*SeaStreamCallback)(const char* chunk, u32 chunk_len, void* user_data);

/* ── Agent Configuration ──────────────────────────────────── */

/* Fallback provider entry */
typedef struct {
    SeaLlmProvider provider;
    const char*    api_key;
    const char*    api_url;
    const char*    model;
} SeaLlmFallback;

#define SEA_MAX_FALLBACKS 4

typedef struct {
    SeaLlmProvider provider;
    const char*    api_key;
    const char*    api_url;       /* Override base URL (for local) */
    const char*    model;         /* e.g. "gpt-4o", "claude-3-sonnet" */
    const char*    system_prompt;
    u32            max_tokens;
    f64            temperature;
    u32            max_tool_rounds; /* Max tool-call iterations */

    /* Fallback chain: tried in order if primary fails */
    SeaLlmFallback fallbacks[SEA_MAX_FALLBACKS];
    u32            fallback_count;

    /* Think level: controls temperature and max_tokens */
    SeaThinkLevel  think_level;

    /* Smart router: hint for provider selection */
    SeaRouteHint   route_hint;

    /* SSE streaming: if set, stream tokens to callback */
    SeaStreamCallback stream_cb;
    void*             stream_user_data;

    /* PII firewall: bitmask of SeaPiiCategory to redact (0 = disabled) */
    u32            pii_categories;
} SeaAgentConfig;

/* ── Chat Message ─────────────────────────────────────────── */

typedef enum {
    SEA_ROLE_SYSTEM = 0,
    SEA_ROLE_USER,
    SEA_ROLE_ASSISTANT,
    SEA_ROLE_TOOL,
} SeaRole;

typedef struct {
    SeaRole     role;
    const char* content;
    const char* tool_call_id;  /* For tool result messages */
    const char* tool_name;     /* For tool result messages */
} SeaChatMsg;

/* ── Agent Response ───────────────────────────────────────── */

typedef struct {
    const char* text;          /* Final text response */
    u32         tool_calls;    /* Number of tool calls made */
    u32         tokens_used;   /* Approximate token usage */
    SeaError    error;
} SeaAgentResult;

/* ── API ──────────────────────────────────────────────────── */

/* Initialize agent with config. */
void sea_agent_init(SeaAgentConfig* cfg);

/* Set default config values. */
void sea_agent_defaults(SeaAgentConfig* cfg);

/* Process a user message through the agent loop.
 * May make multiple LLM calls if tool calls are requested.
 * All allocations go into arena. */
SeaAgentResult sea_agent_chat(SeaAgentConfig* cfg,
                              SeaChatMsg* history, u32 history_count,
                              const char* user_input,
                              SeaArena* arena);

/* Build the system prompt with tool descriptions. */
const char* sea_agent_build_system_prompt(SeaArena* arena);

/* Hot-swap the model at runtime. Thread-safe. */
void sea_agent_set_model(SeaAgentConfig* cfg, const char* model);

/* Hot-swap the provider at runtime. */
void sea_agent_set_provider(SeaAgentConfig* cfg, SeaLlmProvider provider,
                             const char* api_key, const char* api_url);

/* Set think level (adjusts temperature + max_tokens). */
void sea_agent_set_think_level(SeaAgentConfig* cfg, SeaThinkLevel level);

/* Get think level name. */
const char* sea_agent_think_level_name(SeaThinkLevel level);

/* Compact: summarize a conversation history into a single message.
 * Returns the summary string allocated in arena. */
const char* sea_agent_compact(SeaAgentConfig* cfg,
                               SeaChatMsg* history, u32 history_count,
                               SeaArena* arena);

#endif /* SEA_AGENT_H */
