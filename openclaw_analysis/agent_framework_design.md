# OpenClaw-C: Secure Agentic Framework Design

## Executive Summary

This document presents a comprehensive design for a C-based reimplementation of the OpenClaw personal AI assistant, featuring Agent-0 style autonomous capabilities, strict grammar-constrained tool calling, multi-model orchestration, and secure credential management.

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Tool Calling Grammar Specification](#tool-calling-grammar-specification)
3. [Skill Registry System](#skill-registry-system)
4. [Multi-Model Routing](#multi-model-routing)
5. [Secure Credential Storage](#secure-credential-storage)
6. [Agent Loop & Persistence](#agent-loop--persistence)
7. [Agent-to-Agent Communication](#agent-to-agent-communication)
8. [Agent State Machine](#agent-state-machine)
9. [Implementation Roadmap](#implementation-roadmap)

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         OPENCLAW-C AGENT FRAMEWORK                          │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐         │
│  │   TUI Dashboard │    │  Agent Runtime  │    │  Skill Registry │         │
│  │   (ncurses)     │◄──►│   (Agent-0)     │◄──►│   (SQLite)      │         │
│  └─────────────────┘    └────────┬────────┘    └─────────────────┘         │
│                                  │                                          │
│                    ┌─────────────┼─────────────┐                           │
│                    ▼             ▼             ▼                           │
│  ┌─────────────────────┐ ┌──────────────┐ ┌─────────────────┐              │
│  │  Grammar-Constrained │ │  Model Router │ │  Secure Keyring │              │
│  │  Tool Parser (GLL)   │ │  (Multi-LLM)  │ │  (libsecret)    │              │
│  └─────────────────────┘ └──────┬───────┘ └─────────────────┘              │
│                                 │                                          │
│         ┌───────────────────────┼───────────────────────┐                  │
│         ▼                       ▼                       ▼                  │
│  ┌─────────────┐        ┌─────────────┐        ┌─────────────────┐         │
│  │  Local Qwen │        │  Ollama API │        │  External APIs  │         │
│  │  (7B, GGUF) │        │  (Local)    │        │  (RunPod/Kimi)  │         │
│  └─────────────┘        └─────────────┘        └─────────────────┘         │
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                    Persistence Layer (LMDB)                          │   │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐            │   │
│  │  │ Agent    │  │ Session  │  │ Tool     │  │ Vector   │            │   │
│  │  │ State    │  │ History  │  │ Results  │  │ Memory   │            │   │
│  │  └──────────┘  └──────────┘  └──────────┘  └──────────┘            │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Tool Calling Grammar Specification

### 1. BNF Grammar Definition

```bnf
;; ============================================================
;; OpenClaw-C Tool Calling Grammar (Strict, No Free-Form JSON)
;; ============================================================

<tool-call>        ::= <tool-invoke> | <tool-chain> | <tool-parallel>
<tool-invoke>      ::= "@" <tool-name> "{" <param-list> "}"
<tool-chain>       ::= <tool-invoke> ("->" <tool-invoke>)*
<tool-parallel>    ::= "[" <tool-invoke> ("," <tool-invoke>)* "]"
<tool-name>        ::= <identifier> | <namespaced-tool>
<namespaced-tool>  ::= <namespace> "/" <identifier>
<namespace>        ::= "fs" | "net" | "sys" | "code" | "data" | "ui"
<param-list>       ::= <param> (";" <param>)* | ""
<param>            ::= <param-key> "=" <param-value>
<param-key>        ::= <identifier>
<param-value>      ::= <string-literal> | <number> | <boolean> | <array> | <reference>
<string-literal>   ::= '"' <string-content> '"' | "'" <string-content> "'"
<number>           ::= <integer> | <float> | <hex>
<integer>          ::= ["-"] <digit>+
<float>            ::= ["-"] <digit>+ "." <digit>+
<hex>              ::= "0x" <hex-digit>+
<boolean>          ::= "true" | "false" | "yes" | "no"
<array>            ::= "(" <param-value> ("," <param-value>)* ")"
<reference>        ::= "$" <reference-type> "." <identifier>
<reference-type>   ::= "prev" | "ctx" | "env" | "self"
<identifier>       ::= <alpha> (<alpha> | <digit> | "_" | "-")*
<alpha>            ::= "a"-"z" | "A"-"Z"
<digit>            ::= "0"-"9"
<hex-digit>        ::= <digit> | "a"-"f" | "A"-"F"
```

### 2. Example Tool Calls

```
;; Single tool invocation
@fs/read{path="/etc/passwd";offset=0;limit=100}

;; Tool with namespace
@net/http{url="https://api.example.com/data";method="GET"}

;; Tool chaining
@fs/read{path="data.json"}->@data/transform{operation="parse_json"}

;; Reference to previous result
@fs/write{path="output.txt";content=$prev.result}

;; Context reference
@sys/exec{command="git";args=("status");cwd=$ctx.working_dir}
```

### 3. Grammar Parser Header (C)

```c
/* tool_grammar.h - Strict Grammar-Constrained Tool Parser */
#ifndef TOOL_GRAMMAR_H
#define TOOL_GRAMMAR_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    TOK_AT, TOK_LBRACE, TOK_RBRACE, TOK_LPAREN, TOK_RPAREN,
    TOK_LBRACKET, TOK_RBRACKET, TOK_ARROW, TOK_SEMICOLON, TOK_COMMA,
    TOK_EQUALS, TOK_DOT, TOK_SLASH, TOK_DOLLAR, TOK_STRING,
    TOK_NUMBER, TOK_BOOL, TOK_IDENTIFIER, TOK_EOF, TOK_ERROR
} TokenType;

typedef struct {
    TokenType type;
    const char *start;
    size_t length;
    uint32_t line;
    uint32_t column;
} Token;

typedef enum {
    PARAM_STRING, PARAM_NUMBER_INT, PARAM_NUMBER_FLOAT,
    PARAM_BOOL, PARAM_ARRAY, PARAM_REFERENCE
} ParamValueType;

typedef struct ParamValue {
    ParamValueType type;
    union {
        struct { char *data; size_t len; } string;
        int64_t int_val; double float_val; bool bool_val;
        struct { struct ParamValue *items; size_t count; } array;
        struct { char *ref_type; char *ref_name; } reference;
    };
} ParamValue;

typedef struct { char *key; ParamValue value; } Parameter;

typedef struct ToolInvoke {
    char *namespace; char *name;
    Parameter *params; size_t param_count;
    struct ToolInvoke *next_chain;
} ToolInvoke;

typedef struct {
    bool success; ToolInvoke *invocations; size_t invocation_count;
    char *error_message; uint32_t error_line; uint32_t error_column;
} ParseResult;

ParseResult *tool_grammar_parse(const char *input, size_t len);
void tool_grammar_free_result(ParseResult *result);
bool tool_grammar_validate(const char *input, size_t len);

#endif
```

---

## Skill Registry System

### 1. Data Structure Design

```c
/* skill_registry.h - Skill Registration and Discovery */
#ifndef SKILL_REGISTRY_H
#define SKILL_REGISTRY_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#define SKILL_MAX_NAME_LEN 64
#define SKILL_MAX_TOOLS 32
#define SKILL_MAX_PARAMS 16

typedef enum {
    SKILL_CAP_FILESYSTEM = 1 << 0, SKILL_CAP_NETWORK = 1 << 1,
    SKILL_CAP_EXEC = 1 << 2, SKILL_CAP_UI = 1 << 3,
    SKILL_CAP_MEMORY = 1 << 4, SKILL_CAP_EXTERNAL = 1 << 5
} SkillCapability;

typedef enum {
    SKILL_TRUST_UNVERIFIED = 0, SKILL_TRUST_COMMUNITY = 1,
    SKILL_TRUST_VERIFIED = 2, SKILL_TRUST_OFFICIAL = 3
} SkillTrustLevel;

typedef enum {
    SKILL_PARAM_STRING, SKILL_PARAM_INT, SKILL_PARAM_FLOAT,
    SKILL_PARAM_BOOL, SKILL_PARAM_ENUM, SKILL_PARAM_PATH
} SkillParamType;

typedef struct {
    char name[SKILL_MAX_NAME_LEN];
    char description[512];
    SkillParamType type;
    bool required; bool sensitive;
} SkillParamDef;

typedef struct {
    char name[SKILL_MAX_NAME_LEN]; char description[512];
    char return_type[SKILL_MAX_NAME_LEN];
    SkillParamDef params[SKILL_MAX_PARAMS]; size_t param_count;
    uint32_t permission_mask; bool idempotent;
    bool async_supported; uint32_t timeout_ms;
} SkillToolDef;

typedef struct {
    char name[SKILL_MAX_NAME_LEN]; char version[32];
    char description[512]; char author[128];
    char tags[16][32]; size_t tag_count; char category[64];
    uint32_t required_caps; SkillTrustLevel trust_level;
    SkillToolDef tools[SKILL_MAX_TOOLS]; size_t tool_count;
    char entry_point[256]; bool is_builtin; bool is_enabled;
} SkillManifest;

typedef struct SkillRegistry SkillRegistry;

SkillRegistry *skill_registry_create(const char *db_path);
int skill_registry_register(SkillRegistry *reg, const SkillManifest *manifest);
SkillManifest *skill_registry_get(SkillRegistry *reg, const char *name);
int skill_registry_execute_tool(SkillRegistry *reg, const char *ns,
    const char *name, const Parameter *params, size_t count,
    char **result, size_t *result_len);

#endif
```

### 2. SQLite Schema

```sql
-- Skills table
CREATE TABLE skills (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT UNIQUE NOT NULL, version TEXT NOT NULL,
    description TEXT, author TEXT, category TEXT,
    required_caps INTEGER DEFAULT 0, trust_level INTEGER DEFAULT 0,
    signature TEXT, entry_point TEXT, is_builtin BOOLEAN DEFAULT 0,
    is_enabled BOOLEAN DEFAULT 1, manifest_json TEXT NOT NULL
);

-- Skill tags
CREATE TABLE skill_tags (
    skill_id INTEGER REFERENCES skills(id) ON DELETE CASCADE,
    tag TEXT NOT NULL, PRIMARY KEY (skill_id, tag)
);
CREATE INDEX idx_skill_tags_tag ON skill_tags(tag);

-- Skill tools
CREATE TABLE skill_tools (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    skill_id INTEGER REFERENCES skills(id) ON DELETE CASCADE,
    name TEXT NOT NULL, description TEXT, return_type TEXT,
    permission_mask INTEGER DEFAULT 0, idempotent BOOLEAN DEFAULT 0,
    timeout_ms INTEGER DEFAULT 30000, definition_json TEXT NOT NULL,
    UNIQUE(skill_id, name)
);
CREATE INDEX idx_skill_tools_name ON skill_tools(name);

-- Full-text search
CREATE VIRTUAL TABLE skills_fts USING fts5(name, description);
```

---

## Multi-Model Routing

### 1. Router Architecture

```c
/* model_router.h - Multi-Model Orchestration */
#ifndef MODEL_ROUTER_H
#define MODEL_ROUTER_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    MODEL_CAP_CHAT = 1 << 0, MODEL_CAP_TOOLS = 1 << 4,
    MODEL_CAP_JSON = 1 << 5, MODEL_CAP_STREAM = 1 << 6,
    MODEL_CAP_REASONING = 1 << 7, MODEL_CAP_CODE = 1 << 8,
    MODEL_CAP_LONG_CTX = 1 << 9
} ModelCapability;

typedef enum {
    PROVIDER_LOCAL, PROVIDER_OLLAMA, PROVIDER_OPENAI,
    PROVIDER_ANTHROPIC, PROVIDER_RUNPOD, PROVIDER_KIMI
} ModelProviderType;

typedef enum {
    TASK_CHAT, TASK_REASONING, TASK_CODE, TASK_SUMMARIZE,
    TASK_EXTRACT, TASK_CLASSIFY, TASK_EMBED, TASK_VISION,
    TASK_TOOL_USE, TASK_AGENT_LOOP
} TaskType;

typedef enum {
    ROUTE_QUALITY, ROUTE_SPEED, ROUTE_COST, ROUTE_BALANCED,
    ROUTE_FALLBACK, ROUTE_PARALLEL, ROUTE_CASCADE
} RoutingStrategy;

typedef struct {
    char name[64]; char display_name[64];
    ModelProviderType provider; char endpoint_url[512];
    char api_key_ref[64]; uint32_t capabilities;
    uint32_t context_window; uint32_t max_output_tokens;
    float cost_per_1k_input; float cost_per_1k_output;
    uint32_t typical_latency_ms; float quality_score;
    float speed_score; float cost_score; float reliability_score;
    bool is_enabled; bool is_local; uint32_t priority;
} ModelConfig;

typedef struct {
    TaskType task; uint32_t estimated_tokens;
    bool requires_tools; bool requires_vision; bool requires_json;
    float budget_usd; uint32_t timeout_ms;
} RequestContext;

typedef struct {
    bool success; char *content; size_t content_len;
    uint32_t tokens_input; uint32_t tokens_output;
    float cost_usd; uint32_t latency_ms;
    char model_used[64]; char *error_message;
} ModelResponse;

typedef struct ModelRouter ModelRouter;

ModelRouter *model_router_create(void);
int model_router_register(ModelRouter *router, const ModelConfig *config);
ModelConfig *model_router_select(ModelRouter *router, const RequestContext *ctx);
int model_router_route(ModelRouter *router, const RequestContext *ctx,
    const char *prompt, size_t len, ModelResponse *response);

#endif
```

### 2. Routing Algorithm

```c
/* Score model for request context */
static float score_model(const ModelConfig *model, 
    const RequestContext *ctx, RoutingStrategy strategy) {
    
    /* Check capability requirements */
    if (ctx->requires_tools && !(model->capabilities & MODEL_CAP_TOOLS))
        return -1.0f;
    if (ctx->requires_vision && !(model->capabilities & MODEL_CAP_VISION))
        return -1.0f;
    if (ctx->estimated_tokens > model->context_window)
        return -1.0f;
    
    float score = 0.0f;
    switch (strategy) {
        case ROUTE_QUALITY:
            score = model->quality_score * 0.6f + 
                    model->reliability_score * 0.4f;
            break;
        case ROUTE_SPEED:
            score = model->speed_score * 0.6f +
                    (1.0f / (1.0f + model->typical_latency_ms/1000.0f)) * 0.4f;
            break;
        case ROUTE_COST:
            score = (1.0f - model->cost_score) * 0.6f +
                    model->reliability_score * 0.4f;
            break;
        case ROUTE_BALANCED:
        default:
            score = model->quality_score * 0.4f +
                    model->speed_score * 0.3f +
                    (1.0f - model->cost_score) * 0.3f;
            if (model->is_local && ctx->task == TASK_AGENT_LOOP)
                score *= 1.2f;
    }
    return score;
}
```

### 3. Default Model Configuration

```json
{
  "models": [
    {
      "name": "qwen-7b-local",
      "display_name": "Qwen 2.5 7B (Local)",
      "provider": "local",
      "endpoint_url": "file:///opt/openclaw/models/qwen2.5-7b.gguf",
      "capabilities": ["chat", "tools", "json", "stream", "code"],
      "context_window": 32768,
      "cost_per_1k_input": 0.0,
      "quality_score": 0.75,
      "speed_score": 0.85,
      "is_local": true,
      "priority": 1
    },
    {
      "name": "kimi-k2.5",
      "display_name": "Kimi K2.5",
      "provider": "kimi",
      "endpoint_url": "https://api.moonshot.cn/v1/chat/completions",
      "api_key_ref": "kimi_api_key",
      "capabilities": ["chat", "tools", "json", "reasoning", "long_context"],
      "context_window": 256000,
      "cost_per_1k_input": 0.003,
      "quality_score": 0.92,
      "is_local": false,
      "priority": 3
    },
    {
      "name": "claude-opus",
      "display_name": "Claude 3.5 Opus",
      "provider": "anthropic",
      "endpoint_url": "https://api.anthropic.com/v1/messages",
      "api_key_ref": "anthropic_api_key",
      "capabilities": ["chat", "tools", "json", "reasoning", "vision", "code"],
      "context_window": 200000,
      "cost_per_1k_input": 0.015,
      "quality_score": 0.95,
      "is_local": false,
      "priority": 4
    }
  ]
}
```

---

## Secure Credential Storage

### 1. Keyring Integration Architecture

```c
/* secure_keyring.h - Secure API Key Management */
#ifndef SECURE_KEYRING_H
#define SECURE_KEYRING_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    KEY_TYPE_API, KEY_TYPE_TOKEN, KEY_TYPE_PASSWORD,
    KEY_TYPE_CERT, KEY_TYPE_SSH, KEY_TYPE_CUSTOM
} KeyType;

typedef enum {
    SEC_LEVEL_MEMORY, SEC_LEVEL_USER, SEC_LEVEL_SYSTEM, SEC_LEVEL_HARDWARE
} SecurityLevel;

typedef enum {
    BACKEND_LIBSECRET, BACKEND_KEYCHAIN, BACKEND_CREDENTIAL,
    BACKEND_FILE, BACKEND_TPM, BACKEND_MEMORY
} KeyringBackend;

typedef struct {
    char name[128]; char service[64];
    KeyType type; SecurityLevel level;
    time_t created_at; time_t expires_at;
    bool auto_rotate; uint32_t rotation_days;
} KeyAttributes;

typedef struct SecureKeyring SecureKeyring;

SecureKeyring *keyring_create(KeyringBackend backend, const char *app_name);
int keyring_set(SecureKeyring *kr, const char *name, const char *service,
    const uint8_t *value, size_t len, const KeyAttributes *attrs);
int keyring_get(SecureKeyring *kr, const char *name, const char *service,
    uint8_t **value, size_t *len, KeyAttributes *attrs);
void keyring_secure_zero(void *ptr, size_t len);

#endif
```

### 2. File-Based Encrypted Backend

```c
#define FILE_HEADER_MAGIC "OCKR"
#define KDF_ITERATIONS 100000
#define AES_KEY_SIZE 32
#define AES_IV_SIZE 12
#define AES_TAG_SIZE 16

typedef struct {
    char magic[4]; uint32_t version;
    uint32_t entry_count; uint8_t salt[32];
} FileHeader;

static int derive_key(const char *password, const uint8_t *salt,
    uint8_t *key, size_t key_len) {
    return PKCS5_PBKDF2_HMAC(password, strlen(password),
        salt, 32, KDF_ITERATIONS, EVP_sha256(), key_len, key);
}

static int encrypt_data(const uint8_t *key, const uint8_t *plaintext,
    size_t len, uint8_t *ciphertext, uint8_t *nonce, uint8_t *tag) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    RAND_bytes(nonce, AES_IV_SIZE);
    EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, AES_IV_SIZE, NULL);
    EVP_EncryptInit_ex(ctx, NULL, NULL, key, nonce);
    int outlen; EVP_EncryptUpdate(ctx, ciphertext, &outlen, plaintext, len);
    EVP_EncryptFinal_ex(ctx, ciphertext + outlen, &outlen);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, AES_TAG_SIZE, tag);
    EVP_CIPHER_CTX_free(ctx);
    return 0;
}
```

---

## Agent Loop & Persistence

### 1. Agent State Machine

```c
/* agent_runtime.h - Agent-0 Style Autonomous Agent Loop */
#ifndef AGENT_RUNTIME_H
#define AGENT_RUNTIME_H

typedef enum {
    AGENT_STATE_IDLE = 0, AGENT_STATE_PLANNING,
    AGENT_STATE_EXECUTING, AGENT_STATE_WAITING_INPUT,
    AGENT_STATE_WAITING_TOOL, AGENT_STATE_REFLECTING,
    AGENT_STATE_HALTED, AGENT_STATE_ERROR
} AgentState;

typedef enum {
    OBS_USER_INPUT, OBS_TOOL_OUTPUT, OBS_SYSTEM_EVENT,
    OBS_AGENT_ACTION, OBS_REFLECTION, OBS_PLAN_UPDATE
} ObservationType;

typedef struct {
    uint64_t id; ObservationType type; time_t timestamp;
    char *content; size_t content_len; float importance;
} Observation;

typedef struct {
    uint32_t step_number; char *description;
    ToolInvoke *tool_call; bool completed; bool failed;
    char *result; time_t started_at;
} PlanStep;

typedef struct {
    char *goal; PlanStep steps[64]; size_t step_count;
    size_t current_step; bool is_revised;
} AgentPlan;

typedef struct {
    char session_id[64]; char user_id[64];
    AgentState state; AgentPlan *current_plan;
    Observation *observations[1024]; size_t observation_count;
    time_t created_at; uint32_t iteration_count;
} AgentSession;

typedef struct Agent Agent;

Agent *agent_create(const AgentConfig *config, SkillRegistry *skills,
    ModelRouter *router, SecureKeyring *keyring);
int agent_process_message(Agent *agent, AgentSession *session,
    const char *message, char **response);
int agent_run_iteration(Agent *agent, AgentSession *session);
int agent_save_state(Agent *agent, const char *path);
int agent_load_state(Agent *agent, const char *path);

#endif
```

### 2. Agent Loop Implementation

```c
static void *agent_loop(void *arg) {
    Agent *agent = arg;
    while (agent->running) {
        for (size_t i = 0; i < agent->session_count; i++) {
            AgentSession *session = agent->sessions[i];
            if (session->state == AGENT_STATE_IDLE) continue;
            agent_run_iteration(agent, session);
            if (session->iteration_count >= agent->config.max_iterations)
                session->state = AGENT_STATE_HALTED;
        }
        usleep(10000); /* 10ms */
    }
    return NULL;
}

int agent_run_iteration(Agent *agent, AgentSession *session) {
    session->iteration_count++;
    switch (session->state) {
        case AGENT_STATE_PLANNING:
            if (!session->current_plan)
                agent_create_plan(agent, session, extract_goal(session));
            session->state = AGENT_STATE_EXECUTING;
            break;
        case AGENT_STATE_EXECUTING:
            if (session->current_plan->current_step < 
                session->current_plan->step_count)
                agent_execute_step(agent, session);
            else
                session->state = AGENT_STATE_REFLECTING;
            break;
        case AGENT_STATE_REFLECTING:
            session->state = should_continue(session) ? 
                AGENT_STATE_PLANNING : AGENT_STATE_IDLE;
            break;
        default: break;
    }
    return 0;
}
```

---

## Agent-to-Agent Communication

### 1. ACP Protocol

```c
/* agent_protocol.h - Inter-Agent Communication */
#ifndef AGENT_PROTOCOL_H
#define AGENT_PROTOCOL_H

typedef enum {
    ACP_MSG_REQUEST, ACP_MSG_RESPONSE, ACP_MSG_NOTIFY,
    ACP_MSG_BROADCAST, ACP_MSG_DELEGATE, ACP_MSG_RESULT,
    ACP_MSG_HEARTBEAT, ACP_MSG_DISCOVER
} AcpMessageType;

typedef struct {
    uint32_t magic; /* 'ACP\0' */
    uint32_t version; AcpMessageType type;
    char sender[64]; char recipient[64];
    char message_id[64]; char correlation_id[64];
    uint64_t timestamp; uint32_t payload_size; uint32_t flags;
} AcpHeader;

typedef struct { AcpHeader header; uint8_t *payload; } AcpMessage;

typedef struct AcpChannel AcpChannel;

AcpChannel *acp_channel_create(const char *agent_name);
int acp_channel_listen(AcpChannel *channel, const char *endpoint);
int acp_channel_connect(AcpChannel *channel, const char *endpoint);
int acp_send_delegate(AcpChannel *channel, const char *recipient,
    const char *task_desc, char **task_id);

#endif
```

---

## Agent State Machine Diagram

```
                              ┌─────────┐
                              │  START  │
                              └────┬────┘
                                   │
                                   ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                              ┌─────────┐                                    │
│     ┌───────────────────────►│  IDLE   │◄────────────────────────┐          │
│     │                        └────┬────┘                         │          │
│     │              user_message   │   task_complete              │          │
│     │                             ▼                              │          │
│     │                        ┌─────────┐                         │          │
│     │                        │PLANNING │                         │          │
│     │                        └────┬────┘                         │          │
│     │              plan_created   │   plan_failed                │          │
│     │                             ▼                              │          │
│     │                        ┌─────────┐                         │          │
│     │              ┌────────►│EXECUTING│◄────────┐               │          │
│     │              │         └────┬────┘         │               │          │
│     │              │  step_complete│  tool_needed │               │          │
│     │              │              ▼              │               │          │
│     │              │    ┌─────────┐    ┌─────────┴──┐            │          │
│     │              └────┤  step   │◄───┤WAITING_TOOL│            │          │
│     │                   │  next   │    └─────┬──────┘            │          │
│     │                   └─────────┘          │ tool_result        │          │
│     │                                        ▼                    │          │
│     │                                   ┌─────────┐               │          │
│     │                                   │ tool    │───────────────┘          │
│     │                                   │ success │                          │
│     │                                   └─────────┘                          │
│     │              plan_complete                                             │
│     │                             ▼                                          │
│     │                        ┌──────────┐                                    │
│     │                        │REFLECTING│                                    │
│     │                        └────┬─────┘                                    │
│     │              need_more_work │   satisfied                              │
│     └─────────────────────────────┘                                          │
│  ┌──────────────────────────────────────────────────────────────────────┐    │
│  │                           ERROR STATES                                │    │
│  │  ┌─────────┐      ┌─────────┐      ┌─────────┐      ┌─────────┐      │    │
│  │  │  ERROR  │◄────►│  HALT   │◄────►│ TIMEOUT │◄────►│  DENIED │      │    │
│  │  └────┬────┘      └─────────┘      └─────────┘      └─────────┘      │    │
│  │       └───────────────────────────────────────────────────────────────┘    │
│  │                           (can recover to IDLE)                            │
│  └───────────────────────────────────────────────────────────────────────────┘
└─────────────────────────────────────────────────────────────────────────────┘

STATES:
  IDLE          - Waiting for input
  PLANNING      - Creating execution plan
  EXECUTING     - Executing plan step
  WAITING_TOOL  - Waiting for tool result
  REFLECTING    - Evaluating progress
  WAITING_INPUT - Waiting for user input
  ERROR/HALTED  - Error/recovery states
```

---

## TUI Dashboard Concept

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ OpenClaw-C Agent Framework v1.0.0                              [AGENTS: 3]  │
├─────────────────────────────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────┐  ┌─────────────────────────────┐   │
│  │ CHAT                                │  │ AGENTS                      │   │
│  │ User: Analyze codebase              │  │ ▶ main-agent    [EXECUTING] │   │
│  │ Agent: I'll analyze...              │  │   sub-agent-1   [IDLE]      │   │
│  │ @fs/list{path="."}                  │  │   sub-agent-2   [PLANNING]  │   │
│  │ [142 files found]                   │  │ Load: 45% | Mem: 234MB      │   │
│  │                                     │  │ ─────────────────────────── │   │
│  │                                     │  │ ACTIVE TOOLS                │   │
│  │                                     │  │ ▶ fs/list    Duration: 0.3s │   │
│  │                                     │  │ ▶ code/parse Duration: 1.2s │   │
│  └─────────────────────────────────────┘  └─────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │ > analyze the codebase and find potential security issues          │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
├─────────────────────────────────────────────────────────────────────────────┤
│ Model: kimi-k2.5 | Tokens: 4,231/200K | Cost: $0.023 | Latency: 1.2s       │
│ [F1]Help [F2]Agents [F3]Tools [F4]Memory [F5]Logs [F10]Config [Ctrl+C]Quit │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Implementation Roadmap

### Phase 1: Core Foundation (Weeks 1-4)
- [ ] Tool grammar parser (GLL-based)
- [ ] Basic skill registry (SQLite backend)
- [ ] Model router with local Qwen support
- [ ] File-based secure keyring

### Phase 2: Agent Runtime (Weeks 5-8)
- [ ] Agent state machine
- [ ] Planning and execution loop
- [ ] Observation and memory system
- [ ] Persistence layer (LMDB)

### Phase 3: External Integrations (Weeks 9-12)
- [ ] Ollama API client
- [ ] RunPod serverless integration
- [ ] Kimi K2.5 API client
- [ ] libsecret keyring backend

### Phase 4: Communication & UI (Weeks 13-16)
- [ ] ACP protocol implementation
- [ ] TUI dashboard (ncurses)
- [ ] Agent-to-agent communication
- [ ] Configuration management

### Phase 5: Hardening & Optimization (Weeks 17-20)
- [ ] Security audit
- [ ] Performance optimization
- [ ] Error recovery
- [ ] Documentation

---

## Summary

This design provides a comprehensive foundation for a secure, C-based agentic framework with:

1. **Strict Grammar-Constrained Tool Calling** - BNF-based parser preventing injection attacks
2. **Hierarchical Skill Registry** - SQLite-backed with full-text search
3. **Intelligent Multi-Model Routing** - Task-based routing with fallback
4. **Hardware-Backed Credential Storage** - libsecret with encrypted file fallback
5. **Persistent Agent Loop** - State machine with checkpoint/restore
6. **Inter-Agent Communication** - ACP protocol for distributed coordination

The framework is designed for production use with security, performance, and extensibility as primary concerns.
