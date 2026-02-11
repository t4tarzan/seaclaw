# Sea-Claw API Reference

> Complete C API documentation for all public headers in Sea-Claw v1.0.0

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

*Generated from Sea-Claw v1.0.0 — 12 headers, 9,159 lines of C11*
