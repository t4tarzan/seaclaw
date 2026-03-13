# Sea-Claw API Reference

> This document covers SeaClaw v2.0.0. See [CHANGELOG.md](../CHANGELOG.md) for migration from v1.0.0.

---

## Table of Contents

1. [sea_types.h — Foundation Types](#1-sea_typesh--foundation-types)
2. [sea_arena.h — Arena Allocator](#2-sea_arenah--arena-allocator)
3. [sea_log.h — Structured Logging](#3-sea_logh--structured-logging)
4. [sea_json.h — Zero-Copy JSON Parser](#4-sea_jsonh--zero-copy-json-parser)
5. [sea_shield.h — Grammar Filter](#5-sea_shieldh--grammar-filter)
6. [sea_http.h — HTTP Client](#6-sea_httph--http-client)
7. [sea_db.h — SQLite Database](#7-sea_dbh--sqlite-database)
8. [sea_config.h — Configuration Loader](#8-sea_configh--configuration-loader)
9. [sea_tools.h — Tool Registry](#9-sea_toolsh--tool-registry)
10. [sea_agent.h — LLM Agent](#10-sea_agenth--llm-agent)
11. [sea_telegram.h — Telegram Bot](#11-sea_telegramh--telegram-bot)
12. [sea_a2a.h — Agent-to-Agent Protocol](#12-sea_a2ah--agent-to-agent-protocol)
13. [sea_bus.h — Message Bus](#13-sea_bush--message-bus)
14. [sea_channel.h — Channel Interface](#14-sea_channelh--channel-interface)
15. [sea_session.h — Session Management](#15-sea_sessionh--session-management)
16. [sea_memory.h — Long-Term Memory](#16-sea_memoryh--long-term-memory)
17. [sea_cron.h — Cron Scheduler](#17-sea_cronh--cron-scheduler)
18. [sea_skill.h — Skills & Plugins](#18-sea_skillh--skills--plugins)
19. [sea_usage.h — Usage Tracking](#19-sea_usageh--usage-tracking)
20. [sea_pii.h — PII Firewall](#20-sea_piih--pii-firewall)
21. [sea_recall.h — Memory Retrieval](#21-sea_recallh--memory-retrieval)

---

## 1. `sea_types.h` — Foundation Types

**File:** `include/seaclaw/sea_types.h` (168 lines)  
**Dependencies:** `<stdint.h>`, `<stddef.h>`, `<stdbool.h>`  
**Depended on by:** Every other header

### Type Aliases

| Alias | Underlying | Size |
|-------|-----------|------|
| `u8` | `uint8_t` | 1 byte |
| `u16` | `uint16_t` | 2 bytes |
| `u32` | `uint32_t` | 4 bytes |
| `u64` | `uint64_t` | 8 bytes |
| `i8` | `int8_t` | 1 byte |
| `i16` | `int16_t` | 2 bytes |
| `i32` | `int32_t` | 4 bytes |
| `i64` | `int64_t` | 8 bytes |
| `f32` | `float` | 4 bytes |
| `f64` | `double` | 8 bytes |

### `SeaSlice` — Zero-Copy String View

```c
typedef struct {
    const u8* data;   // Pointer into existing buffer (never owns memory)
    u32       len;    // Length in bytes
} SeaSlice;
```

| Macro | Description |
|-------|-------------|
| `SEA_SLICE_EMPTY` | `{NULL, 0}` — empty slice |
| `SEA_SLICE_LIT(s)` | Create slice from string literal (compile-time length) |

| Function | Signature | Description |
|----------|-----------|-------------|
| `sea_slice_eq` | `bool (SeaSlice a, SeaSlice b)` | Byte-by-byte equality |
| `sea_slice_eq_cstr` | `bool (SeaSlice s, const char* cstr)` | Compare slice to C string |

### `SeaError` — Error Codes (22 values)

| Code | Value | Category | Description |
|------|-------|----------|-------------|
| `SEA_OK` | 0 | Success | No error |
| `SEA_ERR_OOM` | 1 | Memory | Out of memory |
| `SEA_ERR_ARENA_FULL` | 2 | Memory | Arena capacity exceeded |
| `SEA_ERR_IO` | 3 | I/O | General I/O error |
| `SEA_ERR_EOF` | 4 | I/O | End of file |
| `SEA_ERR_TIMEOUT` | 5 | I/O | Operation timed out |
| `SEA_ERR_CONNECT` | 6 | I/O | Connection failed |
| `SEA_ERR_PARSE` | 7 | Parsing | General parse error |
| `SEA_ERR_INVALID_JSON` | 8 | Parsing | Invalid JSON |
| `SEA_ERR_UNEXPECTED_TOKEN` | 9 | Parsing | Unexpected token |
| `SEA_ERR_INVALID_INPUT` | 10 | Security | Invalid input |
| `SEA_ERR_GRAMMAR_REJECT` | 11 | Security | Grammar rejected |
| `SEA_ERR_SANDBOX_FAIL` | 12 | Security | Sandbox failure |
| `SEA_ERR_PERMISSION` | 13 | Security | Permission denied |
| `SEA_ERR_TOOL_NOT_FOUND` | 14 | Tools | Tool not found |
| `SEA_ERR_TOOL_FAILED` | 15 | Tools | Tool execution failed |
| `SEA_ERR_MODEL_LOAD` | 16 | Model | Model load failed |
| `SEA_ERR_INFERENCE` | 17 | Model | Inference error |
| `SEA_ERR_CONFIG` | 18 | Config | Config error |
| `SEA_ERR_MISSING_KEY` | 19 | Config | Missing key |
| `SEA_ERR_NOT_FOUND` | 20 | General | Not found |
| `SEA_ERR_ALREADY_EXISTS` | 21 | General | Already exists |
| `SEA_ERR_NOT_IMPLEMENTED` | 22 | General | Not implemented |

| Function | Signature | Description |
|----------|-----------|-------------|
| `sea_error_str` | `const char* (SeaError err)` | Error code to human string |

### `SeaAgentState` — Agent States

```c
typedef enum {
    SEA_AGENT_IDLE = 0,
    SEA_AGENT_PLANNING,
    SEA_AGENT_EXECUTING,
    SEA_AGENT_STREAMING,
    SEA_AGENT_REFLECTING,
    SEA_AGENT_HALTED,
} SeaAgentState;
```

### Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `SEA_VERSION_STRING` | `"1.0.0"` | Version string |
| `SEA_MAX_TOOLS` | 256 | Max tools in registry |
| `SEA_MAX_TOOL_NAME` | 64 | Max tool name length |
| `SEA_MAX_PATH` | 4096 | Max file path length |
| `SEA_MAX_LINE` | 8192 | Max line length |
| `SEA_MAX_JSON_DEPTH` | 32 | Max JSON nesting depth |

---

## 2. `sea_arena.h` — Arena Allocator

**File:** `include/seaclaw/sea_arena.h` (65 lines)  
**Dependencies:** `sea_types.h`  
**Implementation:** `src/core/sea_arena.c`  
**Tests:** `tests/test_arena.c` (9 tests)  
**Performance:** 11ns per allocation

### Struct

```c
typedef struct {
    u8* base;          // mmap'd memory block
    u64 size;          // Total capacity in bytes
    u64 offset;        // Current bump pointer position
    u64 high_water;    // Peak usage tracker
} SeaArena;
```

### Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `sea_arena_create` | `SeaError (SeaArena* arena, u64 size)` | Create arena via mmap. Returns `SEA_OK` or `SEA_ERR_OOM`. |
| `sea_arena_destroy` | `void (SeaArena* arena)` | Destroy arena, munmap backing memory. |
| `sea_arena_alloc` | `void* (SeaArena* arena, u64 size, u64 align)` | Allocate `size` bytes with alignment. Returns NULL if full. |
| `sea_arena_push` | `void* (SeaArena* arena, u64 size)` | Allocate with default 8-byte alignment. Inline. |
| `sea_arena_push_cstr` | `SeaSlice (SeaArena* arena, const char* cstr)` | Copy C string into arena. Returns slice. |
| `sea_arena_push_bytes` | `void* (SeaArena* arena, const void* data, u64 len)` | Copy raw bytes into arena. Returns pointer. |
| `sea_arena_reset` | `void (SeaArena* arena)` | Reset to empty. O(1) — one pointer move. Inline. |
| `sea_arena_used` | `u64 (const SeaArena* arena)` | Bytes currently used. Inline. |
| `sea_arena_remaining` | `u64 (const SeaArena* arena)` | Bytes remaining. Inline. |
| `sea_arena_usage_pct` | `f64 (const SeaArena* arena)` | Usage as percentage (0.0-100.0). Inline. |

### Usage Pattern

```c
SeaArena arena;
sea_arena_create(&arena, 1024 * 1024);  // 1 MB

// Allocate — always forward, never free individual items
char* buf = sea_arena_alloc(&arena, 256, 1);
void* data = sea_arena_push(&arena, 1024);
SeaSlice s = sea_arena_push_cstr(&arena, "hello");

// Reset — instant, all memory reusable
sea_arena_reset(&arena);

// Cleanup
sea_arena_destroy(&arena);
```

---

## 3. `sea_log.h` — Structured Logging

**File:** `include/seaclaw/sea_log.h` (37 lines)  
**Dependencies:** `sea_types.h`  
**Implementation:** `src/core/sea_log.c`

### Enum

```c
typedef enum {
    SEA_LOG_DEBUG = 0,
    SEA_LOG_INFO,
    SEA_LOG_WARN,
    SEA_LOG_ERROR,
} SeaLogLevel;
```

### Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `sea_log_init` | `void (SeaLogLevel min_level)` | Initialize logging. Call once at startup. Starts the ms clock. |
| `sea_log_elapsed_ms` | `u64 (void)` | Milliseconds since `sea_log_init()`. |
| `sea_log` | `void (SeaLogLevel level, const char* tag, const char* fmt, ...)` | Core log function. printf-style. |

### Macros

```c
SEA_LOG_DEBUG("TAG", "format %s %d", str, num);
SEA_LOG_INFO("TAG", "format %s %d", str, num);
SEA_LOG_WARN("TAG", "format %s %d", str, num);
SEA_LOG_ERROR("TAG", "format %s %d", str, num);
```

**Output format:** `T+<ms> [<TAG>] <LVL>: <message>`

---

## 4. `sea_json.h` — Zero-Copy JSON Parser

**File:** `include/seaclaw/sea_json.h` (82 lines)  
**Dependencies:** `sea_types.h`, `sea_arena.h`  
**Implementation:** `src/senses/sea_json.c`  
**Tests:** `tests/test_json.c` (17 tests)  
**Performance:** 5.4 μs per parse

### Types

```c
typedef enum {
    SEA_JSON_NULL = 0,
    SEA_JSON_BOOL,
    SEA_JSON_NUMBER,
    SEA_JSON_STRING,
    SEA_JSON_ARRAY,
    SEA_JSON_OBJECT,
} SeaJsonType;

typedef struct SeaJsonValue {
    SeaJsonType type;
    SeaSlice    raw;        // Raw bytes in source buffer
    union {
        bool     boolean;   // SEA_JSON_BOOL
        f64      number;    // SEA_JSON_NUMBER
        SeaSlice string;    // SEA_JSON_STRING (without quotes)
        struct { struct SeaJsonValue* items; u32 count; } array;
        struct { SeaSlice* keys; struct SeaJsonValue* values; u32 count; } object;
    };
} SeaJsonValue;
```

### Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `sea_json_parse` | `SeaError (SeaSlice input, SeaArena* arena, SeaJsonValue* out)` | Parse JSON. Nodes allocated in arena. |
| `sea_json_get` | `const SeaJsonValue* (const SeaJsonValue* obj, const char* key)` | Find key in object. NULL if not found. |
| `sea_json_get_string` | `SeaSlice (const SeaJsonValue* obj, const char* key)` | Get string value. Empty slice if missing. |
| `sea_json_get_number` | `f64 (const SeaJsonValue* obj, const char* key, f64 fallback)` | Get number. Returns fallback if missing. |
| `sea_json_get_bool` | `bool (const SeaJsonValue* obj, const char* key, bool fallback)` | Get bool. Returns fallback if missing. |
| `sea_json_array_get` | `const SeaJsonValue* (const SeaJsonValue* arr, u32 index)` | Get array item by index. NULL if OOB. |
| `sea_json_debug_print` | `void (const SeaJsonValue* val, int indent)` | Print JSON tree for debugging. |

---

## 5. `sea_shield.h` — Grammar Filter

**File:** `include/seaclaw/sea_shield.h` (70 lines)  
**Dependencies:** `sea_types.h`  
**Implementation:** `src/shield/sea_shield.c`  
**Tests:** `tests/test_shield.c` (19 tests)  
**Performance:** 0.97 μs per check

### Types

```c
typedef enum {
    SEA_GRAMMAR_SAFE_TEXT = 0,  // Printable ASCII, no control chars
    SEA_GRAMMAR_NUMERIC,        // Digits, dot, minus, plus, e/E
    SEA_GRAMMAR_ALPHA,          // Letters only
    SEA_GRAMMAR_ALPHANUM,       // Letters + digits
    SEA_GRAMMAR_FILENAME,       // Alphanumeric + . - _ /
    SEA_GRAMMAR_URL,            // URL-safe characters
    SEA_GRAMMAR_JSON,           // Valid JSON characters
    SEA_GRAMMAR_COMMAND,        // / prefix + alphanumeric + space
    SEA_GRAMMAR_HEX,            // 0-9, a-f, A-F
    SEA_GRAMMAR_BASE64,         // A-Z, a-z, 0-9, +, /, =
    SEA_GRAMMAR_COUNT           // Sentinel
} SeaGrammarType;

typedef struct {
    bool        valid;
    u32         fail_pos;   // Position of first invalid byte
    u8          fail_byte;  // The offending byte
    const char* reason;     // Human-readable rejection
} SeaShieldResult;
```

### Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `sea_shield_validate` | `SeaShieldResult (SeaSlice input, SeaGrammarType grammar)` | Full validation with details. |
| `sea_shield_check` | `bool (SeaSlice input, SeaGrammarType grammar)` | Quick pass/fail check. |
| `sea_shield_enforce` | `SeaError (SeaSlice input, SeaGrammarType grammar, const char* ctx)` | Validate + log rejection. |
| `sea_shield_detect_injection` | `bool (SeaSlice input)` | Detect shell/SQL injection patterns. |
| `sea_shield_validate_url` | `bool (SeaSlice url)` | Check URL is HTTPS + allowed domain. |
| `sea_shield_check_magic` | `bool (SeaSlice data, const char* type)` | Check file magic bytes. |
| `sea_grammar_name` | `const char* (SeaGrammarType grammar)` | Grammar type to string. |

---

## 6. `sea_http.h` — HTTP Client

**File:** `include/seaclaw/sea_http.h` (34 lines)  
**Dependencies:** `sea_types.h`, `sea_arena.h`  
**Implementation:** `src/senses/sea_http.c`  
**External:** libcurl

### Types

```c
typedef struct {
    i32      status_code;   // HTTP status (200, 404, etc.)
    SeaSlice body;          // Response body (in arena)
    SeaSlice headers;       // Response headers (in arena)
} SeaHttpResponse;
```

### Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `sea_http_get` | `SeaError (const char* url, SeaArena* arena, SeaHttpResponse* resp)` | HTTP GET. Body in arena. |
| `sea_http_post_json` | `SeaError (const char* url, SeaSlice body, SeaArena* arena, SeaHttpResponse* resp)` | POST with JSON body. |
| `sea_http_post_json_auth` | `SeaError (const char* url, SeaSlice body, const char* auth, SeaArena* arena, SeaHttpResponse* resp)` | POST with auth header. |

---

## 7. `sea_db.h` — SQLite Database

**File:** `include/seaclaw/sea_db.h` (93 lines)  
**Dependencies:** `sea_types.h`, `sea_arena.h`  
**Implementation:** `src/core/sea_db.c`  
**Tests:** `tests/test_db.c` (10 tests)  
**External:** libsqlite3

### Types

```c
typedef struct SeaDb SeaDb;  // Opaque handle

typedef struct {
    i32 id; const char* entry_type; const char* title;
    const char* content; const char* created_at;
} SeaDbEvent;

typedef struct {
    i32 id; const char* title; const char* status;
    const char* priority; const char* content;
} SeaDbTask;

typedef struct {
    const char* role;     // "user", "assistant", "system", "tool"
    const char* content;
} SeaDbChatMsg;
```

### Functions — Lifecycle

| Function | Signature | Description |
|----------|-----------|-------------|
| `sea_db_open` | `SeaError (SeaDb** db, const char* path)` | Open/create database. Auto-creates tables. |
| `sea_db_close` | `void (SeaDb* db)` | Close and flush. |

### Functions — Audit Trail

| Function | Signature | Description |
|----------|-----------|-------------|
| `sea_db_log_event` | `SeaError (SeaDb* db, const char* type, const char* title, const char* content)` | Log an audit event. |
| `sea_db_recent_events` | `i32 (SeaDb* db, SeaDbEvent* out, i32 max, SeaArena* arena)` | Load last N events. Returns count. |

### Functions — Key-Value Config

| Function | Signature | Description |
|----------|-----------|-------------|
| `sea_db_config_set` | `SeaError (SeaDb* db, const char* key, const char* value)` | Set config value. |
| `sea_db_config_get` | `const char* (SeaDb* db, const char* key, SeaArena* arena)` | Get config value. NULL if missing. |

### Functions — Tasks

| Function | Signature | Description |
|----------|-----------|-------------|
| `sea_db_task_create` | `SeaError (SeaDb* db, const char* title, const char* priority, const char* content)` | Create task. |
| `sea_db_task_update_status` | `SeaError (SeaDb* db, i32 id, const char* status)` | Update task status. |
| `sea_db_task_list` | `i32 (SeaDb* db, const char* filter, SeaDbTask* out, i32 max, SeaArena* arena)` | List tasks. Returns count. |

### Functions — Chat History

| Function | Signature | Description |
|----------|-----------|-------------|
| `sea_db_chat_log` | `SeaError (SeaDb* db, i64 chat_id, const char* role, const char* content)` | Log a chat message. |
| `sea_db_chat_history` | `i32 (SeaDb* db, i64 chat_id, SeaDbChatMsg* out, i32 max, SeaArena* arena)` | Load last N messages. Returns count. |
| `sea_db_chat_clear` | `SeaError (SeaDb* db, i64 chat_id)` | Clear chat history. |

### Functions — Raw SQL

| Function | Signature | Description |
|----------|-----------|-------------|
| `sea_db_exec` | `SeaError (SeaDb* db, const char* sql)` | Execute raw SQL (escape hatch). |

---

## 8. `sea_config.h` — Configuration Loader

**File:** `include/seaclaw/sea_config.h` (73 lines)  
**Dependencies:** `sea_types.h`, `sea_arena.h`  
**Implementation:** `src/core/sea_config.c`  
**Tests:** `tests/test_config.c` (6 tests)

### Types

```c
typedef struct {
    // Telegram
    const char* telegram_token;
    i64         telegram_chat_id;

    // Database
    const char* db_path;

    // System
    const char* log_level;
    u32         arena_size_mb;

    // LLM Agent
    const char* llm_provider;   // "openai", "anthropic", "gemini", "openrouter", "local"
    const char* llm_api_key;
    const char* llm_model;
    const char* llm_api_url;

    // Fallbacks (up to 4)
    struct {
        const char* provider;
        const char* api_key;
        const char* model;
        const char* api_url;
    } llm_fallbacks[4];
    u32 llm_fallback_count;

    bool loaded;
} SeaConfig;
```

### Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `sea_config_load` | `SeaError (SeaConfig* cfg, const char* path, SeaArena* arena)` | Load from JSON file. Strings stored in arena. |
| `sea_config_defaults` | `void (SeaConfig* cfg)` | Apply defaults for unset fields. |
| `sea_config_print` | `void (const SeaConfig* cfg)` | Print config summary to stdout. |

---

## 9. `sea_tools.h` — Tool Registry

**File:** `include/seaclaw/sea_tools.h` (45 lines)  
**Dependencies:** `sea_types.h`, `sea_arena.h`  
**Implementation:** `src/hands/sea_tools.c` + `src/hands/impl/tool_*.c` (50 files)

### Types

```c
// Every tool implements this signature
typedef SeaError (*SeaToolFunc)(SeaSlice args, SeaArena* arena, SeaSlice* output);

typedef struct {
    u32           id;           // Tool ID (1-50)
    const char*   name;         // Tool name for lookup
    const char*   description;  // Human-readable description
    SeaToolFunc   func;         // Function pointer
} SeaTool;
```

### Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `sea_tools_init` | `void (void)` | Initialize registry. Logs tool count. |
| `sea_tools_count` | `u32 (void)` | Get number of registered tools. |
| `sea_tool_by_name` | `const SeaTool* (const char* name)` | Lookup tool by name. NULL if not found. |
| `sea_tool_by_id` | `const SeaTool* (u32 id)` | Lookup tool by ID. NULL if not found. |
| `sea_tool_exec` | `SeaError (const char* name, SeaSlice args, SeaArena* arena, SeaSlice* output)` | Execute tool by name. |
| `sea_tools_list` | `void (void)` | Print all tools to stdout. |

---

## 10. `sea_agent.h` — LLM Agent

**File:** `include/seaclaw/sea_agent.h` (100 lines)  
**Dependencies:** `sea_types.h`, `sea_arena.h`  
**Implementation:** `src/brain/sea_agent.c`

### Types

```c
typedef enum {
    SEA_LLM_OPENAI = 0,
    SEA_LLM_ANTHROPIC,
    SEA_LLM_GEMINI,
    SEA_LLM_OPENROUTER,
    SEA_LLM_LOCAL,
} SeaLlmProvider;

typedef struct {
    SeaLlmProvider provider;
    const char* api_key;
    const char* api_url;
    const char* model;
} SeaLlmFallback;

typedef struct {
    SeaLlmProvider provider;
    const char*    api_key;
    const char*    api_url;
    const char*    model;
    const char*    system_prompt;
    u32            max_tokens;
    f64            temperature;
    u32            max_tool_rounds;
    SeaLlmFallback fallbacks[4];
    u32            fallback_count;
} SeaAgentConfig;

typedef enum {
    SEA_ROLE_SYSTEM = 0, SEA_ROLE_USER, SEA_ROLE_ASSISTANT, SEA_ROLE_TOOL,
} SeaRole;

typedef struct {
    SeaRole     role;
    const char* content;
    const char* tool_call_id;
    const char* tool_name;
} SeaChatMsg;

typedef struct {
    const char* text;          // Final text response
    u32         tool_calls;    // Number of tool calls made
    u32         tokens_used;   // Approximate token usage
    SeaError    error;
} SeaAgentResult;
```

### Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `sea_agent_init` | `void (SeaAgentConfig* cfg)` | Initialize agent. Logs provider + model. |
| `sea_agent_defaults` | `void (SeaAgentConfig* cfg)` | Set default config values. |
| `sea_agent_chat` | `SeaAgentResult (SeaAgentConfig* cfg, SeaChatMsg* history, u32 count, const char* input, SeaArena* arena)` | Process user message through agent loop. May make multiple LLM calls. |
| `sea_agent_build_system_prompt` | `const char* (SeaArena* arena)` | Build system prompt with tool descriptions. |

---

## 11. `sea_telegram.h` — Telegram Bot

**File:** `include/seaclaw/sea_telegram.h` (49 lines)  
**Dependencies:** `sea_types.h`, `sea_arena.h`  
**Implementation:** `src/telegram/sea_telegram.c`

### Types

```c
typedef SeaError (*SeaTelegramHandler)(i64 chat_id, SeaSlice text,
                                       SeaArena* arena, SeaSlice* response);

typedef struct {
    const char*        bot_token;
    i64                allowed_chat_id;  // 0 = allow all
    SeaTelegramHandler handler;
    SeaArena*          arena;
    i64                last_update_id;
    bool               running;
} SeaTelegram;
```

### Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `sea_telegram_init` | `SeaError (SeaTelegram* tg, const char* token, i64 chat_id, SeaTelegramHandler handler, SeaArena* arena)` | Initialize bot. Does NOT start polling. |
| `sea_telegram_poll` | `SeaError (SeaTelegram* tg)` | Poll for updates once. Call in event loop. |
| `sea_telegram_send` | `SeaError (SeaTelegram* tg, i64 chat_id, const char* text)` | Send text message. |
| `sea_telegram_send_slice` | `SeaError (SeaTelegram* tg, i64 chat_id, SeaSlice text)` | Send SeaSlice message. |
| `sea_telegram_get_me` | `SeaError (SeaTelegram* tg, SeaArena* arena)` | Test connection (getMe API). |

---

## 12. `sea_a2a.h` — Agent-to-Agent Protocol

**File:** `include/seaclaw/sea_a2a.h` (76 lines)  
**Dependencies:** `sea_types.h`, `sea_arena.h`  
**Implementation:** `src/a2a/sea_a2a.c`

### Types

```c
typedef enum {
    SEA_A2A_DELEGATE  = 0,
    SEA_A2A_RESULT    = 1,
    SEA_A2A_HEARTBEAT = 2,
    SEA_A2A_DISCOVER  = 3,
    SEA_A2A_CANCEL    = 4,
} SeaA2aType;

typedef struct {
    const char* name;       // "openclaw-vps1"
    const char* endpoint;   // "https://vps.example.com:8080/a2a"
    const char* api_key;
    bool        healthy;
    u64         last_seen;
} SeaA2aPeer;

typedef struct {
    const char* task_id;
    const char* task_desc;
    const char* context;
    u32         timeout_ms;
} SeaA2aRequest;

typedef struct {
    const char* task_id;
    bool        success;
    const char* output;
    u32         latency_ms;
    const char* agent_name;
    bool        verified;    // Shield-verified
    const char* error;
} SeaA2aResult;
```

### Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `sea_a2a_delegate` | `SeaA2aResult (const SeaA2aPeer* peer, const SeaA2aRequest* req, SeaArena* arena)` | Delegate task to remote agent. Shield-validates response. |
| `sea_a2a_heartbeat` | `bool (const SeaA2aPeer* peer, SeaArena* arena)` | Check if peer is alive. |
| `sea_a2a_discover` | `i32 (const char* url, SeaA2aPeer* out, i32 max, SeaArena* arena)` | Discover agents on network. Returns count. |

---

## 13. `sea_bus.h` — Message Bus

**File:** `include/seaclaw/sea_bus.h`
**Dependencies:** `sea_types.h`, `sea_arena.h`, `<pthread.h>`
**Implementation:** `src/bus/sea_bus.c`
**Tests:** `tests/test_bus.c` (12 tests)

### Types

```c
typedef enum {
    SEA_MSG_USER = 0,       /* User message from a channel          */
    SEA_MSG_SYSTEM,         /* System message (cron, heartbeat)     */
    SEA_MSG_TOOL_RESULT,    /* Tool execution result                */
    SEA_MSG_OUTBOUND,       /* Response to send back to channel     */
} SeaMsgType;

typedef struct {
    SeaMsgType  type;
    const char* channel;      /* Channel name: "telegram", "discord", etc. */
    const char* sender_id;    /* Sender identifier (user ID as string)     */
    i64         chat_id;      /* Chat/conversation ID                      */
    const char* content;      /* Message text (arena-allocated)            */
    u32         content_len;
    const char* session_key;  /* Session key: "channel:chat_id"            */
    u64         timestamp_ms;
} SeaBusMsg;

typedef struct {
    SeaBusMsg inbound[256];
    u32 in_head, in_tail, in_count;
    pthread_mutex_t in_mutex;
    pthread_cond_t  in_cond;
    SeaBusMsg outbound[256];
    u32 out_head, out_tail, out_count;
    pthread_mutex_t out_mutex;
    pthread_cond_t  out_cond;
    SeaArena arena;
    bool running;
} SeaBus;
```

### Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `sea_bus_init` | `SeaError (SeaBus* bus, u64 arena_size)` | Initialize the message bus. |
| `sea_bus_destroy` | `void (SeaBus* bus)` | Destroy the bus and free all resources. |
| `sea_bus_publish_inbound` | `SeaError (SeaBus* bus, SeaMsgType type, const char* channel, const char* sender_id, i64 chat_id, const char* content, u32 content_len)` | Publish an inbound message (channel → agent). Thread-safe. |
| `sea_bus_consume_inbound` | `SeaError (SeaBus* bus, SeaBusMsg* out, u32 timeout_ms)` | Consume an inbound message. Blocks up to timeout_ms. |
| `sea_bus_publish_outbound` | `SeaError (SeaBus* bus, const char* channel, i64 chat_id, const char* content, u32 content_len)` | Publish an outbound message (agent → channel). Thread-safe. |
| `sea_bus_consume_outbound` | `SeaError (SeaBus* bus, SeaBusMsg* out)` | Consume any outbound message. Non-blocking. |
| `sea_bus_consume_outbound_for` | `SeaError (SeaBus* bus, const char* channel, SeaBusMsg* out)` | Consume outbound for a specific channel. Non-blocking. |
| `sea_bus_reset_arena` | `void (SeaBus* bus)` | Reset the bus arena (call periodically). |
| `sea_bus_inbound_count` | `u32 (SeaBus* bus)` | Get inbound queue depth. |
| `sea_bus_outbound_count` | `u32 (SeaBus* bus)` | Get outbound queue depth. |

### Example

```c
SeaBus bus;
sea_bus_init(&bus, 4 * 1024 * 1024);

// Channel thread:
sea_bus_publish_inbound(&bus, SEA_MSG_USER, "telegram", "user123",
                         890034905, "hello", 5);

// Agent thread (blocking, 100ms timeout):
SeaBusMsg msg;
if (sea_bus_consume_inbound(&bus, &msg, 100) == SEA_OK) {
    sea_bus_publish_outbound(&bus, msg.channel, msg.chat_id, "reply", 5);
}
sea_bus_destroy(&bus);
```

---

## 14. `sea_channel.h` — Channel Interface

**File:** `include/seaclaw/sea_channel.h`
**Dependencies:** `sea_types.h`, `sea_arena.h`, `sea_bus.h`
**Implementation:** `src/channel/sea_channel.c`
**Tests:** No dedicated test file; channel is exercised via integration through `tests/test_bus.c` and `tests/test_session.c`

### Types

```c
typedef enum {
    SEA_CHAN_STOPPED = 0,
    SEA_CHAN_STARTING,
    SEA_CHAN_RUNNING,
    SEA_CHAN_ERROR,
} SeaChanState;

typedef struct {
    SeaError (*init)(SeaChannel* ch, SeaBus* bus, SeaArena* arena);
    SeaError (*start)(SeaChannel* ch);
    SeaError (*poll)(SeaChannel* ch);
    SeaError (*send)(SeaChannel* ch, i64 chat_id, const char* text, u32 text_len);
    void     (*stop)(SeaChannel* ch);
    void     (*destroy)(SeaChannel* ch);
} SeaChannelVTable;

struct SeaChannel {
    char                    name[32];
    SeaChanState            state;
    SeaBus*                 bus;
    SeaArena*               arena;
    const SeaChannelVTable* vtable;
    void*                   impl;
    bool                    enabled;
};

typedef struct {
    SeaChannel* channels[16];
    u32         count;
    SeaBus*     bus;
    bool        running;
} SeaChannelManager;
```

### Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `sea_channel_manager_init` | `SeaError (SeaChannelManager* mgr, SeaBus* bus)` | Initialize the channel manager. |
| `sea_channel_manager_register` | `SeaError (SeaChannelManager* mgr, SeaChannel* ch)` | Register a channel. Manager does NOT own the channel. |
| `sea_channel_manager_start_all` | `SeaError (SeaChannelManager* mgr)` | Start all enabled channels in their own threads. |
| `sea_channel_manager_stop_all` | `void (SeaChannelManager* mgr)` | Stop all channels gracefully. |
| `sea_channel_manager_get` | `SeaChannel* (SeaChannelManager* mgr, const char* name)` | Get a channel by name. NULL if not found. |
| `sea_channel_manager_enabled_names` | `u32 (SeaChannelManager* mgr, const char** names, u32 max_names)` | List enabled channel names. Returns count. |
| `sea_channel_dispatch_outbound` | `u32 (SeaChannelManager* mgr)` | Route outbound bus messages to correct channel send(). Non-blocking. |
| `sea_channel_base_init` | `void (SeaChannel* ch, const char* name, const SeaChannelVTable* vtable, void* impl)` | Initialize base channel fields. |

---

## 15. `sea_session.h` — Session Management

**File:** `include/seaclaw/sea_session.h`
**Dependencies:** `sea_types.h`, `sea_arena.h`, `sea_db.h`, `sea_agent.h`
**Implementation:** `src/session/sea_session.c`
**Tests:** `tests/test_session.c` (10 tests)

### Types

```c
typedef struct {
    SeaRole     role;
    const char* content;
    u64         timestamp_ms;
} SeaSessionMsg;

typedef struct {
    char          key[128];             /* "channel:chat_id"          */
    const char*   channel;
    i64           chat_id;
    SeaSessionMsg history[50];
    u32           history_count;
    const char*   summary;
    u32           total_messages;
    u64           created_at;
    u64           last_active;
} SeaSession;

typedef struct {
    SeaSession      sessions[64];
    u32             count;
    SeaDb*          db;
    SeaArena        arena;
    u32             max_history;
    u32             keep_recent;
    SeaAgentConfig* agent_cfg;
} SeaSessionManager;
```

### Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `sea_session_init` | `SeaError (SeaSessionManager* mgr, SeaDb* db, SeaAgentConfig* agent_cfg, u64 arena_size)` | Initialize session manager. |
| `sea_session_destroy` | `void (SeaSessionManager* mgr)` | Destroy session manager and free resources. |
| `sea_session_get` | `SeaSession* (SeaSessionManager* mgr, const char* key)` | Get or create session by key. |
| `sea_session_get_by_chat` | `SeaSession* (SeaSessionManager* mgr, const char* channel, i64 chat_id)` | Get or create session by channel + chat_id. |
| `sea_session_add_message` | `SeaError (SeaSessionManager* mgr, const char* key, SeaRole role, const char* content)` | Add message. Triggers summarization if threshold reached. |
| `sea_session_get_history` | `u32 (SeaSessionManager* mgr, const char* key, SeaChatMsg* out, u32 max_count, SeaArena* arena)` | Get history as SeaChatMsg array. Returns count. |
| `sea_session_get_summary` | `const char* (SeaSessionManager* mgr, const char* key)` | Get session summary. NULL if none. |
| `sea_session_summarize` | `SeaError (SeaSessionManager* mgr, const char* key)` | Force LLM summarization of history. |
| `sea_session_clear` | `SeaError (SeaSessionManager* mgr, const char* key)` | Clear history and summary. |
| `sea_session_count` | `u32 (SeaSessionManager* mgr)` | Get active session count. |
| `sea_session_list_keys` | `u32 (SeaSessionManager* mgr, const char** keys, u32 max_keys)` | List active session keys. Returns count. |
| `sea_session_save_all` | `SeaError (SeaSessionManager* mgr)` | Persist all sessions to DB. |
| `sea_session_load_all` | `SeaError (SeaSessionManager* mgr)` | Load sessions from DB. |
| `sea_session_build_key` | `void (char* buf, u32 buf_size, const char* channel, i64 chat_id)` | Build `"channel:chat_id"` key into buf. |

### Example

```c
SeaSessionManager sessions;
sea_session_init(&sessions, db, &agent_cfg, 2 * 1024 * 1024);

char key[128];
sea_session_build_key(key, sizeof(key), "telegram", 890034905);
sea_session_add_message(&sessions, key, SEA_ROLE_USER, "Hello");

SeaChatMsg history[50];
u32 count = sea_session_get_history(&sessions, key, history, 50, &arena);
SeaAgentResult result = sea_agent_chat(&cfg, history, count, "Hello", &arena);
```

---

## 16. `sea_memory.h` — Long-Term Memory

**File:** `include/seaclaw/sea_memory.h`
**Dependencies:** `sea_types.h`, `sea_arena.h`
**Implementation:** `src/memory/sea_memory.c`
**Tests:** `tests/test_memory.c` (7 tests)

### Types

```c
typedef struct {
    char     workspace[4096];   /* Full path to workspace dir  */
    SeaArena arena;             /* Arena for loaded content     */
    bool     initialized;
} SeaMemory;
```

**Workspace Layout:**
```
~/.seaclaw/
  MEMORY.md        — Long-term facts and preferences
  IDENTITY.md      — Agent identity and personality
  USER.md          — User profile and preferences
  SOUL.md          — Agent behavioral guidelines
  AGENTS.md        — Known agents and capabilities
  notes/
    202602/
      20260211.md  — Daily notes for Feb 11, 2026
```

### Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `sea_memory_init` | `SeaError (SeaMemory* mem, const char* workspace_path, u64 arena_size)` | Initialize. Creates workspace dir if needed. |
| `sea_memory_destroy` | `void (SeaMemory* mem)` | Destroy memory system. |
| `sea_memory_read` | `const char* (SeaMemory* mem)` | Read MEMORY.md. Returns content or NULL. |
| `sea_memory_write` | `SeaError (SeaMemory* mem, const char* content)` | Overwrite MEMORY.md. |
| `sea_memory_append` | `SeaError (SeaMemory* mem, const char* content)` | Append to MEMORY.md. |
| `sea_memory_read_bootstrap` | `const char* (SeaMemory* mem, const char* filename)` | Read a bootstrap file (IDENTITY.md, etc.). |
| `sea_memory_write_bootstrap` | `SeaError (SeaMemory* mem, const char* filename, const char* content)` | Write a bootstrap file. |
| `sea_memory_read_daily` | `const char* (SeaMemory* mem)` | Read today's daily note. NULL if none. |
| `sea_memory_append_daily` | `SeaError (SeaMemory* mem, const char* content)` | Append to today's daily note. Creates dirs if needed. |
| `sea_memory_read_daily_for` | `const char* (SeaMemory* mem, const char* date_str)` | Read a specific day's note (YYYYMMDD format). |
| `sea_memory_read_recent_notes` | `const char* (SeaMemory* mem, u32 days)` | Read and concatenate the last N days of notes. |
| `sea_memory_build_context` | `const char* (SeaMemory* mem, SeaArena* arena)` | Build full memory context (IDENTITY+SOUL+USER+MEMORY+notes) for prompt injection. |
| `sea_memory_create_defaults` | `SeaError (SeaMemory* mem)` | Create default bootstrap files if missing. |
| `sea_memory_workspace` | `const char* (SeaMemory* mem)` | Get workspace path. |

---

## 17. `sea_cron.h` — Cron Scheduler

**File:** `include/seaclaw/sea_cron.h`
**Dependencies:** `sea_types.h`, `sea_arena.h`, `sea_db.h`, `sea_bus.h`
**Implementation:** `src/cron/sea_cron.c`
**Tests:** `tests/test_cron.c` (9 tests)

### Types

```c
typedef enum {
    SEA_CRON_SHELL = 0,     /* Execute a shell command          */
    SEA_CRON_TOOL,          /* Call a registered tool            */
    SEA_CRON_BUS_MSG,       /* Publish a message to the bus     */
} SeaCronJobType;

typedef enum {
    SEA_CRON_ACTIVE = 0,
    SEA_CRON_PAUSED,
    SEA_CRON_COMPLETED,
    SEA_CRON_FAILED,
} SeaCronJobState;

typedef enum {
    SEA_SCHED_CRON = 0,     /* Standard cron expression         */
    SEA_SCHED_INTERVAL,     /* @every Ns/Nm/Nh                  */
    SEA_SCHED_ONCE,         /* @once Ns (fire once then done)   */
} SeaSchedType;

typedef struct {
    i32             id;
    char            name[64];
    SeaCronJobType  type;
    SeaCronJobState state;
    SeaSchedType    sched_type;
    char            schedule[64];
    char            command[512];
    char            args[512];
    u64             interval_sec;
    u64             next_run;
    u64             last_run;
    u32             run_count;
    u32             fail_count;
    u64             created_at;
} SeaCronJob;

typedef struct {
    SeaCronJob jobs[64];
    u32        count;
    SeaDb*     db;
    SeaBus*    bus;
    SeaArena   arena;
    bool       running;
    u64        tick_count;
} SeaCronScheduler;
```

### Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `sea_cron_init` | `SeaError (SeaCronScheduler* sched, SeaDb* db, SeaBus* bus)` | Initialize scheduler. Creates DB tables if needed. |
| `sea_cron_destroy` | `void (SeaCronScheduler* sched)` | Destroy the scheduler. |
| `sea_cron_add` | `i32 (SeaCronScheduler* sched, const char* name, SeaCronJobType type, const char* schedule, const char* command, const char* args)` | Add a new job. Returns job ID or -1 on error. |
| `sea_cron_remove` | `SeaError (SeaCronScheduler* sched, i32 job_id)` | Remove a job by ID. |
| `sea_cron_pause` | `SeaError (SeaCronScheduler* sched, i32 job_id)` | Pause a job. |
| `sea_cron_resume` | `SeaError (SeaCronScheduler* sched, i32 job_id)` | Resume a paused job. |
| `sea_cron_tick` | `u32 (SeaCronScheduler* sched)` | Check and execute due jobs. Call once per second. Returns jobs fired. |
| `sea_cron_get` | `SeaCronJob* (SeaCronScheduler* sched, i32 job_id)` | Get job by ID. NULL if not found. |
| `sea_cron_list` | `u32 (SeaCronScheduler* sched, SeaCronJob** out, u32 max_count)` | List all jobs. Returns count. |
| `sea_cron_count` | `u32 (SeaCronScheduler* sched)` | Get job count. |
| `sea_cron_save` | `SeaError (SeaCronScheduler* sched)` | Persist all jobs to DB. |
| `sea_cron_load` | `SeaError (SeaCronScheduler* sched)` | Load jobs from DB. |
| `sea_cron_parse_schedule` | `SeaError (const char* schedule, SeaSchedType* out_type, u64* out_interval, u64* out_next_run)` | Parse a schedule string. Exported for testing. |

---

## 18. `sea_skill.h` — Skills & Plugins

**File:** `include/seaclaw/sea_skill.h`
**Dependencies:** `sea_types.h`, `sea_arena.h`
**Implementation:** `src/skill/sea_skill.c`

### Types

```c
typedef struct {
    char name[64];
    char description[256];
    char trigger[64];    /* Command trigger e.g. "/summarize" */
    char body[4096];     /* Prompt/instruction body           */
    char path[512];      /* Source file path                  */
    bool enabled;
} SeaSkill;

typedef struct {
    SeaSkill skills[64];
    u32      count;
    char     skills_dir[512];
    SeaArena arena;
} SeaSkillRegistry;
```

### Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `sea_skill_init` | `SeaError (SeaSkillRegistry* reg, const char* skills_dir)` | Initialize registry. |
| `sea_skill_destroy` | `void (SeaSkillRegistry* reg)` | Destroy the registry. |
| `sea_skill_load_all` | `SeaError (SeaSkillRegistry* reg)` | Scan skills_dir and load all .md skills. |
| `sea_skill_load_file` | `SeaError (SeaSkillRegistry* reg, const char* path)` | Load a single skill from a .md file. |
| `sea_skill_parse` | `SeaError (const char* content, u32 content_len, SeaSkill* out)` | Parse YAML frontmatter + body into SeaSkill. |
| `sea_skill_register` | `SeaError (SeaSkillRegistry* reg, const SeaSkill* skill)` | Register a skill manually. |
| `sea_skill_find` | `const SeaSkill* (const SeaSkillRegistry* reg, const char* name)` | Find by name. NULL if not found. |
| `sea_skill_find_by_trigger` | `const SeaSkill* (const SeaSkillRegistry* reg, const char* trigger)` | Find by trigger command. NULL if not found. |
| `sea_skill_count` | `u32 (const SeaSkillRegistry* reg)` | Get skill count. |
| `sea_skill_list` | `u32 (const SeaSkillRegistry* reg, const char** names, u32 max)` | List all skill names. Returns count. |
| `sea_skill_enable` | `SeaError (SeaSkillRegistry* reg, const char* name, bool enabled)` | Enable or disable a skill. |
| `sea_skill_build_prompt` | `const char* (const SeaSkill* skill, const char* user_input, SeaArena* arena)` | Combine skill body with user input. Result in arena. |

---

## 19. `sea_usage.h` — Usage Tracking

**File:** `include/seaclaw/sea_usage.h`
**Dependencies:** `sea_types.h`, `sea_db.h`
**Implementation:** `src/usage/sea_usage.c`

### Types

```c
typedef struct {
    char name[32];
    u64  tokens_in;
    u64  tokens_out;
    u64  requests;
    u64  errors;
} SeaUsageProvider;

typedef struct {
    u32 date;       /* YYYYMMDD as integer */
    u64 tokens_in;
    u64 tokens_out;
    u64 requests;
    u64 errors;
} SeaUsageDay;

typedef struct {
    SeaUsageProvider providers[8];
    u32              provider_count;
    SeaUsageDay      days[30];
    u32              day_count;
    u64              total_tokens_in;
    u64              total_tokens_out;
    u64              total_requests;
    u64              total_errors;
    SeaDb*           db;
} SeaUsageTracker;
```

### Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `sea_usage_init` | `SeaError (SeaUsageTracker* tracker, SeaDb* db)` | Initialize tracker. Creates DB table if needed. |
| `sea_usage_record` | `void (SeaUsageTracker* tracker, const char* provider, u32 tokens_in, u32 tokens_out, bool error)` | Record a completed request. |
| `sea_usage_provider` | `const SeaUsageProvider* (const SeaUsageTracker* tracker, const char* provider)` | Get stats for a provider. NULL if not found. |
| `sea_usage_today` | `const SeaUsageDay* (const SeaUsageTracker* tracker)` | Get today's stats. NULL if no activity. |
| `sea_usage_total_tokens` | `u64 (const SeaUsageTracker* tracker)` | Get total tokens used (in + out). |
| `sea_usage_save` | `SeaError (SeaUsageTracker* tracker)` | Save stats to DB. |
| `sea_usage_load` | `SeaError (SeaUsageTracker* tracker)` | Load stats from DB. |
| `sea_usage_summary` | `u32 (const SeaUsageTracker* tracker, char* buf, u32 buf_size)` | Format human-readable summary. Returns length. |

---

## 20. `sea_pii.h` — PII Firewall

**File:** `include/seaclaw/sea_pii.h`
**Dependencies:** `sea_types.h`, `sea_arena.h`
**Implementation:** `src/pii/sea_pii.c`
**Tests:** `tests/test_pii.c` (6 tests)

### Types

```c
typedef enum {
    SEA_PII_EMAIL       = (1 << 0),
    SEA_PII_PHONE       = (1 << 1),
    SEA_PII_SSN         = (1 << 2),
    SEA_PII_CREDIT_CARD = (1 << 3),
    SEA_PII_IP_ADDR     = (1 << 4),
    SEA_PII_ALL         = 0x1F,
} SeaPiiCategory;

typedef struct {
    SeaPiiCategory category;
    u32            offset;      /* Byte offset in input */
    u32            length;      /* Length of match */
} SeaPiiMatch;

typedef struct {
    SeaPiiMatch matches[32];
    u32         count;
    bool        has_pii;
} SeaPiiResult;
```

### Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `sea_pii_scan` | `SeaPiiResult (SeaSlice text, u32 categories)` | Scan for PII matches. Returns result with match list. |
| `sea_pii_redact` | `const char* (SeaSlice text, u32 categories, SeaArena* arena)` | Redact PII, replacing matches with `[REDACTED]`. Result in arena. |
| `sea_pii_contains` | `bool (SeaSlice text, u32 categories)` | Quick boolean check for PII presence. |
| `sea_pii_category_name` | `const char* (SeaPiiCategory cat)` | Get human-readable name for a PII category. |

### Example

```c
SeaSlice text = SEA_SLICE_LIT("Email me at secret@corp.com, SSN 123-45-6789");
SeaPiiResult r = sea_pii_scan(text, SEA_PII_ALL);
if (r.has_pii) {
    const char* clean = sea_pii_redact(text, SEA_PII_ALL, &arena);
    // clean = "Email me at [REDACTED], SSN [REDACTED]"
}
```

---

## 21. `sea_recall.h` — Memory Retrieval

**File:** `include/seaclaw/sea_recall.h`
**Dependencies:** `sea_types.h`, `sea_arena.h`, `sea_db.h` (opaque `SeaDb*`)
**Implementation:** `src/recall/sea_recall.c`
**Tests:** `tests/test_recall.c` (3 tests)

### Types

```c
typedef struct {
    i32         id;
    const char* category;    /* user, preference, fact, rule, context, identity */
    const char* content;
    const char* keywords;    /* Space-separated keyword tokens */
    i32         importance;  /* 1-10, higher = more likely recalled */
    const char* created_at;
    const char* accessed_at;
    i32         access_count;
    f64         score;       /* Computed relevance score (transient) */
} SeaRecallFact;

typedef struct {
    SeaDb* db;
    bool   initialized;
    u32    max_context_tokens;
} SeaRecall;
```

### Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `sea_recall_init` | `SeaError (SeaRecall* rc, SeaDb* db, u32 max_context_tokens)` | Initialize recall engine. Creates facts table if needed. |
| `sea_recall_destroy` | `void (SeaRecall* rc)` | Destroy recall engine. |
| `sea_recall_store` | `SeaError (SeaRecall* rc, const char* category, const char* content, const char* keywords, i32 importance)` | Store a fact. Keywords auto-extracted if NULL. |
| `sea_recall_query` | `i32 (SeaRecall* rc, const char* query, SeaRecallFact* out, i32 max_results, SeaArena* arena)` | Find top-N relevant facts. Scored by keyword_overlap × importance × recency_decay. |
| `sea_recall_build_context` | `const char* (SeaRecall* rc, const char* query, SeaArena* arena)` | Build context string for top facts. Within max_context_tokens budget. |
| `sea_recall_forget` | `SeaError (SeaRecall* rc, i32 fact_id)` | Forget a fact by ID. |
| `sea_recall_forget_category` | `SeaError (SeaRecall* rc, const char* category)` | Forget all facts in a category. |
| `sea_recall_count` | `u32 (SeaRecall* rc)` | Get total fact count. |
| `sea_recall_count_category` | `u32 (SeaRecall* rc, const char* category)` | Get fact count by category. |

### Example

```c
SeaRecall recall;
sea_recall_init(&recall, db, 2000);  // ~2000 token budget

sea_recall_store(&recall, "preference", "User prefers concise answers",
                  "prefer concise brief short", 8);
sea_recall_store(&recall, "fact", "User lives in Berlin",
                  "berlin city location lives", 7);

const char* ctx = sea_recall_build_context(&recall, "where do you live", &arena);
// ctx = "## Relevant Memory\n- User lives in Berlin\n"
```

---

*Generated from Sea-Claw v2.0.0 — 21 headers (18+ public), 13,400+ lines of C11*
