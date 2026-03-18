# Sea-Claw Architecture & Dependency Guide

> *"We stop building software that breaks. We start building logic that survives."*

**Version:** 1.0.0  
**Stats:** 9,159 lines of C11 | 74 source files | 50 tools | 61 tests | 2 dependencies  
**Build:** `make all` → single binary `sea_claw` (~82KB stripped)

---

## Table of Contents

1. [System Overview](#1-system-overview)
2. [Directory Structure](#2-directory-structure)
3. [Dependency Order](#3-dependency-order)
4. [Module Reference](#4-module-reference)
5. [Function Flow](#5-function-flow)
6. [Build System](#6-build-system)
7. [Memory Model](#7-memory-model)
8. [Security Model](#8-security-model)
9. [Configuration](#9-configuration)
10. [External Dependencies](#10-external-dependencies)

---

## 1. System Overview

Sea-Claw is a sovereign AI agent platform — a single C11 binary that:

- Receives input via **Telegram bot** or **interactive TUI**
- Validates every byte through a **Grammar Shield**
- Routes natural language to **LLM providers** (OpenRouter → OpenAI → Gemini fallback chain)
- Parses tool calls from LLM responses and executes them via a **static tool registry**
- Persists state in **SQLite** (chat history, tasks, audit trail)
- Delegates tasks to remote agents via **A2A protocol**

```
┌───────────────────────────────────────────────────────┐
│                  Sea-Claw Binary                      │
├──────────┬──────────┬───────────┬────────┬────────────┤
│ Substrate│  Senses  │  Shield   │ Brain  │   Hands    │
│(Arena,DB)│(JSON,HTTP)│(Grammar) │(Agent) │ (50 Tools) │
├──────────┴──────────┴───────────┴────────┴────────────┤
│                   Telegram / TUI                      │
└───────────────────────────────────────────────────────┘
```

### The Five Pillars

| Pillar | Directory | Purpose |
|--------|-----------|---------|
| **Substrate** | `src/core/` | Arena allocator, logging, SQLite DB, JSON config |
| **Senses** | `src/senses/` | Zero-copy JSON parser, HTTP client (libcurl) |
| **Shield** | `src/shield/` | Byte-level grammar validation, injection detection |
| **Brain** | `src/brain/` | LLM agent loop with multi-provider fallback + tool calling |
| **Hands** | `src/hands/` | 50 static tools: file, shell, web, text, data, network, security |

---

## 2. Directory Structure

```
seaclaw/
├── include/seaclaw/          # Public headers (12 files)
│   ├── sea_types.h           # Fixed-width types, error codes, SeaSlice
│   ├── sea_arena.h           # Arena allocator API
│   ├── sea_log.h             # Structured logging
│   ├── sea_json.h            # Zero-copy JSON parser
│   ├── sea_shield.h          # Grammar filter API
│   ├── sea_http.h            # HTTP client API
│   ├── sea_db.h              # SQLite database API
│   ├── sea_config.h          # JSON config loader
│   ├── sea_tools.h           # Tool registry API
│   ├── sea_agent.h           # LLM agent API
│   ├── sea_telegram.h        # Telegram bot API
│   └── sea_a2a.h             # Agent-to-Agent protocol
│
├── src/
│   ├── core/                 # Substrate layer
│   │   ├── sea_arena.c       # mmap-based arena allocator (9 tests)
│   │   ├── sea_log.c         # Timestamped structured logging
│   │   ├── sea_db.c          # SQLite wrapper (10 tests)
│   │   └── sea_config.c      # JSON config loader (6 tests)
│   │
│   ├── senses/               # I/O layer
│   │   ├── sea_json.c        # Zero-copy JSON parser (17 tests)
│   │   └── sea_http.c        # libcurl HTTP client
│   │
│   ├── shield/               # Security layer
│   │   └── sea_shield.c      # Grammar filter (19 tests)
│   │
│   ├── brain/                # Intelligence layer
│   │   └── sea_agent.c       # LLM agent loop + tool dispatch
│   │
│   ├── hands/                # Tool layer
│   │   ├── sea_tools.c       # Static registry (50 entries)
│   │   └── impl/             # Tool implementations (50 files)
│   │       ├── tool_echo.c
│   │       ├── tool_system_status.c
│   │       ├── ... (48 more)
│   │       └── tool_count_lines.c
│   │
│   ├── telegram/             # Interface layer
│   │   └── sea_telegram.c    # Long-polling Telegram bot
│   │
│   ├── a2a/                  # Federation layer
│   │   └── sea_a2a.c         # Agent-to-Agent delegation
│   │
│   └── main.c               # Entry point, event loop, command dispatch
│
├── tests/                    # Test suites (5 files, 61 tests)
│   ├── test_arena.c          # 9 tests
│   ├── test_json.c           # 17 tests
│   ├── test_shield.c         # 19 tests
│   ├── test_db.c             # 10 tests
│   └── test_config.c         # 6 tests
│
├── config/
│   └── config.example.json   # Example configuration
│
├── Makefile                  # Primary build system
├── .env                      # API keys (not committed)
└── README.md                 # Project overview
```

---

## 3. Dependency Order

### 3.1 Header Include Graph

Every module depends on `sea_types.h`. The full dependency tree:

```
Level 0 (Foundation):
  sea_types.h          ← No dependencies. Defines u8/u32/u64, SeaSlice, SeaError.

Level 1 (Core):
  sea_arena.h          ← sea_types.h
  sea_log.h            ← sea_types.h

Level 2 (I/O + Security):
  sea_json.h           ← sea_types.h, sea_arena.h
  sea_http.h           ← sea_types.h, sea_arena.h
  sea_shield.h         ← sea_types.h

Level 3 (Storage + Config):
  sea_db.h             ← sea_types.h, sea_arena.h
  sea_config.h         ← sea_types.h, sea_arena.h

Level 4 (Intelligence):
  sea_tools.h          ← sea_types.h, sea_arena.h
  sea_agent.h          ← sea_types.h, sea_arena.h

Level 5 (Interface):
  sea_telegram.h       ← sea_types.h, sea_arena.h
  sea_a2a.h            ← sea_types.h, sea_arena.h

Level 6 (Application):
  main.c               ← ALL headers
```

### 3.2 Compilation Order

The Makefile compiles in this order (each group depends on the previous):

```
1. src/core/sea_arena.c        → sea_arena.o
2. src/core/sea_log.c          → sea_log.o
3. src/core/sea_db.c           → sea_db.o        (links: libsqlite3)
4. src/core/sea_config.c       → sea_config.o
5. src/senses/sea_json.c       → sea_json.o
6. src/senses/sea_http.c       → sea_http.o      (links: libcurl)
7. src/shield/sea_shield.c     → sea_shield.o
8. src/telegram/sea_telegram.c → sea_telegram.o
9. src/brain/sea_agent.c       → sea_agent.o
10. src/a2a/sea_a2a.c          → sea_a2a.o
11. src/hands/sea_tools.c      → sea_tools.o
12. src/hands/impl/tool_*.c    → tool_*.o  (50 files)
13. src/main.c                 → main.o
14. LINK ALL → sea_claw        (links: -lm -lcurl -lsqlite3)
```

### 3.3 Runtime Initialization Order

```c
// In main():
1. sea_log_init(SEA_LOG_INFO)           // Start logging clock
2. load_dotenv(".env")                   // Load API keys from .env
3. sea_config_load(&cfg, path, &arena)   // Parse config.json
4. sea_arena_create(&session, 16MB)      // Session arena
5. sea_arena_create(&request, 1MB)       // Per-request arena
6. sea_tools_init()                      // Log tool count
7. sea_db_open(&db, path)               // Open SQLite, create tables
8. sea_agent_init(&agent_cfg)            // Configure LLM provider chain
9. sea_telegram_init(...)  OR  TUI loop  // Start interface
```

---

## 4. Module Reference

### 4.1 `sea_types.h` — Foundation Types

**Purpose:** Zero-dependency type definitions shared across the entire system.

**Key Types:**
```c
// Fixed-width aliases
typedef uint8_t  u8;    typedef int8_t   i8;
typedef uint16_t u16;   typedef int16_t  i16;
typedef uint32_t u32;   typedef int32_t  i32;
typedef uint64_t u64;   typedef int64_t  i64;
typedef float    f32;   typedef double   f64;

// Zero-copy string view — never owns memory
typedef struct {
    const u8* data;
    u32       len;
} SeaSlice;

// Convenience macros
#define SEA_SLICE_EMPTY ((SeaSlice){NULL, 0})
#define SEA_SLICE_LIT(s) ((SeaSlice){(const u8*)(s), sizeof(s) - 1})
```

**Error Codes (22 total):**
```c
typedef enum {
    SEA_OK = 0,
    SEA_ERR_OOM, SEA_ERR_ARENA_FULL,           // Memory
    SEA_ERR_IO, SEA_ERR_EOF, SEA_ERR_TIMEOUT,   // I/O
    SEA_ERR_PARSE, SEA_ERR_INVALID_JSON,         // Parsing
    SEA_ERR_INVALID_INPUT, SEA_ERR_GRAMMAR_REJECT, // Security
    SEA_ERR_TOOL_NOT_FOUND, SEA_ERR_TOOL_FAILED,   // Tools
    // ... and more
} SeaError;
```

**Inline Functions:**
- `sea_slice_eq(a, b)` — Compare two slices byte-by-byte
- `sea_slice_eq_cstr(s, cstr)` — Compare slice to C string
- `sea_error_str(err)` — Error code to human-readable string

---

### 4.2 `sea_arena.h/.c` — The Memory Notebook

**Purpose:** Bump-pointer arena allocator. All allocations are sequential writes into a single mmap'd block. Reset is instant (one pointer move).

**Performance:** 11ns per allocation. Zero fragmentation.

**API:**
```c
SeaError sea_arena_create(SeaArena* arena, u64 size);   // mmap a block
void     sea_arena_destroy(SeaArena* arena);             // munmap
void*    sea_arena_alloc(SeaArena* arena, u64 size, u64 align);  // bump alloc
void*    sea_arena_push(SeaArena* arena, u64 size);      // alloc with align=8
SeaSlice sea_arena_push_cstr(SeaArena* arena, const char* s);    // copy string
void*    sea_arena_push_bytes(SeaArena* arena, const void* data, u64 len);
void     sea_arena_reset(SeaArena* arena);               // instant reset
u64      sea_arena_used(const SeaArena* arena);
u64      sea_arena_remaining(const SeaArena* arena);
f64      sea_arena_usage_pct(const SeaArena* arena);
```

**Memory Layout:**
```
┌──────────────────────────────────────────┐
│ base                    offset     size  │
│  ↓                        ↓         ↓   │
│  [used data...............][free........]│
│  ← sea_arena_used() →                   │
│                    ← sea_arena_remaining() →
└──────────────────────────────────────────┘
```

**Code Example:**
```c
SeaArena arena;
sea_arena_create(&arena, 1024 * 1024);  // 1 MB

char* buf = sea_arena_alloc(&arena, 256, 1);
snprintf(buf, 256, "Hello from arena!");

SeaSlice s = sea_arena_push_cstr(&arena, "zero-copy");
// s.data points into arena memory

sea_arena_reset(&arena);  // instant — all memory reusable
sea_arena_destroy(&arena);
```

---

### 4.3 `sea_log.h/.c` — Structured Logging

**Purpose:** Tagged logging with millisecond timestamps from program start.

**Output Format:** `T+<ms> [<TAG>] <LEVEL>: <message>`

**API:**
```c
void sea_log_init(SeaLogLevel min_level);
u64  sea_log_elapsed_ms(void);
void sea_log(SeaLogLevel level, const char* tag, const char* fmt, ...);

// Convenience macros
SEA_LOG_DEBUG("TAG", "format %s", arg);
SEA_LOG_INFO("TAG", "format %s", arg);
SEA_LOG_WARN("TAG", "format %s", arg);
SEA_LOG_ERROR("TAG", "format %s", arg);
```

**Code Example:**
```c
sea_log_init(SEA_LOG_INFO);
SEA_LOG_INFO("HANDS", "Tool registry loaded: %u tools", 50);
// Output: T+0ms [HANDS] INF: Tool registry loaded: 50 tools
```

---

### 4.4 `sea_json.h/.c` — The Shape Sorter

**Purpose:** Zero-copy JSON parser. Values are `SeaSlice` pointers into the original input buffer. No malloc, no string copies.

**Performance:** 5.4 μs per parse. 17 tests.

**API:**
```c
SeaError sea_json_parse(SeaSlice input, SeaArena* arena, SeaJsonValue* out);

// Accessors
const SeaJsonValue* sea_json_get(const SeaJsonValue* obj, const char* key);
SeaSlice sea_json_get_string(const SeaJsonValue* obj, const char* key);
f64      sea_json_get_number(const SeaJsonValue* obj, const char* key, f64 fallback);
bool     sea_json_get_bool(const SeaJsonValue* obj, const char* key, bool fallback);
const SeaJsonValue* sea_json_array_get(const SeaJsonValue* arr, u32 index);
```

**JSON Value Types:**
```c
typedef enum {
    SEA_JSON_NULL, SEA_JSON_BOOL, SEA_JSON_NUMBER,
    SEA_JSON_STRING, SEA_JSON_ARRAY, SEA_JSON_OBJECT,
} SeaJsonType;
```

**Code Example:**
```c
const char* json = "{\"name\":\"Sea-Claw\",\"tools\":50,\"active\":true}";
SeaSlice input = { .data = (const u8*)json, .len = strlen(json) };

SeaJsonValue root;
sea_json_parse(input, &arena, &root);

SeaSlice name = sea_json_get_string(&root, "name");   // "Sea-Claw"
f64 tools     = sea_json_get_number(&root, "tools", 0); // 50.0
bool active   = sea_json_get_bool(&root, "active", false); // true
```

---

### 4.5 `sea_shield.h/.c` — The Grammar Filter

**Purpose:** Byte-level input/output validation. Every piece of data is checked against a grammar bitmap before it touches the engine. Rejects shell injection, SQL injection, XSS at the byte level.

**Performance:** 0.97 μs per check. 19 tests.

**Grammar Types (10):**
```c
SEA_GRAMMAR_SAFE_TEXT   // Printable ASCII, no control chars
SEA_GRAMMAR_NUMERIC     // Digits, dot, minus, plus, e/E
SEA_GRAMMAR_ALPHA       // Letters only
SEA_GRAMMAR_ALPHANUM    // Letters + digits
SEA_GRAMMAR_FILENAME    // Alphanumeric + . - _ /
SEA_GRAMMAR_URL         // URL-safe characters
SEA_GRAMMAR_JSON        // Valid JSON characters
SEA_GRAMMAR_COMMAND     // / prefix + alphanumeric + space
SEA_GRAMMAR_HEX         // 0-9, a-f, A-F
SEA_GRAMMAR_BASE64      // A-Z, a-z, 0-9, +, /, =
```

**API:**
```c
SeaShieldResult sea_shield_validate(SeaSlice input, SeaGrammarType grammar);
bool            sea_shield_check(SeaSlice input, SeaGrammarType grammar);
SeaError        sea_shield_enforce(SeaSlice input, SeaGrammarType grammar, const char* ctx);
bool            sea_shield_detect_injection(SeaSlice input);
bool            sea_shield_validate_url(SeaSlice url);
```

**Code Example:**
```c
SeaSlice user_input = SEA_SLICE_LIT("hello world");
if (sea_shield_detect_injection(user_input)) {
    // REJECTED — contains dangerous patterns like ; rm -rf, $(cmd), etc.
    return SEA_ERR_INVALID_INPUT;
}
if (!sea_shield_check(user_input, SEA_GRAMMAR_SAFE_TEXT)) {
    // REJECTED — contains control characters or invalid bytes
    return SEA_ERR_GRAMMAR_REJECT;
}
// Safe to process
```

---

### 4.6 `sea_http.h/.c` — HTTP Client

**Purpose:** Minimal HTTP client wrapping libcurl. Response bodies are allocated in the arena.

**API:**
```c
SeaError sea_http_get(const char* url, SeaArena* arena, SeaHttpResponse* resp);
SeaError sea_http_post_json(const char* url, SeaSlice body, SeaArena* arena, SeaHttpResponse* resp);
SeaError sea_http_post_json_auth(const char* url, SeaSlice body, const char* auth,
                                  SeaArena* arena, SeaHttpResponse* resp);
```

**Code Example:**
```c
SeaHttpResponse resp;
SeaError err = sea_http_get("https://api.example.com/data", &arena, &resp);
if (err == SEA_OK && resp.status_code == 200) {
    printf("Body: %.*s\n", (int)resp.body.len, (const char*)resp.body.data);
}
```

---

### 4.7 `sea_db.h/.c` — The Ledger

**Purpose:** SQLite-backed persistent storage. Single file, WAL mode, zero-config. Stores chat history, tasks, audit trail, and key-value config.

**Performance:** 10 tests passing.

**Tables Created Automatically:**
- `trajectory` — Audit log (entry_type, title, content, timestamp)
- `config` — Key-value store
- `tasks` — Task manager (title, status, priority, content)
- `chat_history` — Per-chat conversation memory (chat_id, role, content)

**API:**
```c
// Lifecycle
SeaError sea_db_open(SeaDb** db, const char* path);
void     sea_db_close(SeaDb* db);

// Audit trail
SeaError sea_db_log_event(SeaDb* db, const char* type, const char* title, const char* content);
i32      sea_db_recent_events(SeaDb* db, SeaDbEvent* out, i32 max, SeaArena* arena);

// Key-value config
SeaError    sea_db_config_set(SeaDb* db, const char* key, const char* value);
const char* sea_db_config_get(SeaDb* db, const char* key, SeaArena* arena);

// Tasks
SeaError sea_db_task_create(SeaDb* db, const char* title, const char* priority, const char* content);
SeaError sea_db_task_update_status(SeaDb* db, i32 id, const char* status);
i32      sea_db_task_list(SeaDb* db, const char* filter, SeaDbTask* out, i32 max, SeaArena* arena);

// Chat history
SeaError sea_db_chat_log(SeaDb* db, i64 chat_id, const char* role, const char* content);
i32      sea_db_chat_history(SeaDb* db, i64 chat_id, SeaDbChatMsg* out, i32 max, SeaArena* arena);
SeaError sea_db_chat_clear(SeaDb* db, i64 chat_id);
```

**Code Example:**
```c
SeaDb* db;
sea_db_open(&db, "seaclaw.db");
sea_db_log_event(db, "startup", "Sea-Claw started", "1.0.0");
sea_db_task_create(db, "Implement weather tool", "medium", "Use wttr.in API");
sea_db_chat_log(db, 890034905, "user", "What's the weather?");
sea_db_close(db);
```

---

### 4.8 `sea_config.h/.c` — Configuration Loader

**Purpose:** Parses a JSON config file and provides typed fields. Environment variables override config values for secrets.

**Config Fields:**
```c
typedef struct {
    const char* telegram_token;
    i64         telegram_chat_id;
    const char* db_path;
    const char* log_level;
    u32         arena_size_mb;
    const char* llm_provider;    // "openai", "anthropic", "gemini", "openrouter", "local"
    const char* llm_api_key;
    const char* llm_model;
    const char* llm_api_url;
    struct { const char* provider, *api_key, *model, *api_url; } llm_fallbacks[4];
    u32 llm_fallback_count;
} SeaConfig;
```

**Environment Variable Overrides:**
```
TELEGRAM_BOT_TOKEN  → telegram_token
TELEGRAM_CHAT_ID    → telegram_chat_id
OPENAI_API_KEY      → llm_api_key (when provider=openai)
OPENROUTER_API_KEY  → llm_api_key (when provider=openrouter)
GEMINI_API_KEY      → llm_api_key (when provider=gemini)
ANTHROPIC_API_KEY   → llm_api_key (when provider=anthropic)
```

---

### 4.9 `sea_tools.h/.c` — The Static Tool Registry

**Purpose:** Compile-time tool registry. AI can only call tools that are wired at build time. No dynamic loading, no eval, no exec.

**Tool Function Signature:**
```c
typedef SeaError (*SeaToolFunc)(SeaSlice args, SeaArena* arena, SeaSlice* output);
```

Every tool receives:
- `args` — Input arguments as a byte slice
- `arena` — Arena for output allocation
- `output` — Pointer to write the result slice

**API:**
```c
void           sea_tools_init(void);
u32            sea_tools_count(void);
const SeaTool* sea_tool_by_name(const char* name);
const SeaTool* sea_tool_by_id(u32 id);
SeaError       sea_tool_exec(const char* name, SeaSlice args, SeaArena* arena, SeaSlice* output);
void           sea_tools_list(void);
```

**Code Example — Calling a Tool:**
```c
SeaSlice args = SEA_SLICE_LIT("/root/seaclaw/README.md");
SeaSlice output;
SeaError err = sea_tool_exec("file_read", args, &arena, &output);
if (err == SEA_OK) {
    printf("%.*s\n", (int)output.len, (const char*)output.data);
}
```

**Code Example — Adding a New Tool:**
```c
// 1. Create src/hands/impl/tool_example.c
SeaError tool_example(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    char buf[256];
    int len = snprintf(buf, sizeof(buf), "Hello: %.*s", (int)args.len, (const char*)args.data);
    u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)len);
    if (!dst) return SEA_ERR_ARENA_FULL;
    output->data = dst;
    output->len  = (u32)len;
    return SEA_OK;
}

// 2. Add to sea_tools.c:
//    extern SeaError tool_example(SeaSlice args, SeaArena* arena, SeaSlice* output);
//    {51, "example", "Example tool. Args: text", tool_example},

// 3. Add to Makefile HANDS_SRC
```

---

### 4.10 `sea_agent.h/.c` — The Brain

**Purpose:** LLM agent loop. Takes user input, builds a prompt with tool descriptions, calls an LLM API, parses tool calls from the response, executes them, and loops until the LLM returns a final answer.

**Provider Chain:**
```
Primary: OpenRouter (moonshotai/kimi-k2.5)
    ↓ (on failure)
Fallback 1: OpenAI (gpt-4o-mini)
    ↓ (on failure)
Fallback 2: Gemini (gemini-2.0-flash)
```

**API:**
```c
void           sea_agent_init(SeaAgentConfig* cfg);
void           sea_agent_defaults(SeaAgentConfig* cfg);
SeaAgentResult sea_agent_chat(SeaAgentConfig* cfg, SeaChatMsg* history, u32 count,
                               const char* input, SeaArena* arena);
const char*    sea_agent_build_system_prompt(SeaArena* arena);
```

**Agent Loop Flow:**
```
User Input
    │
    ▼
┌─────────────────┐
│ Shield Validate  │ ← Reject injection / invalid bytes
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Build Messages   │ ← system prompt + history + user input
└────────┬────────┘
         │
         ▼
┌─────────────────┐     ┌──────────────┐
│ Call LLM API     │────→│ Parse Response│
│ (with fallback)  │     └──────┬───────┘
└─────────────────┘            │
                               ▼
                    ┌──────────────────┐
                    │ Tool call found? │
                    └────┬────────┬───┘
                    Yes  │        │ No
                         ▼        ▼
              ┌──────────────┐  ┌──────────┐
              │ Execute Tool  │  │ Return   │
              │ via Registry  │  │ Response │
              └──────┬───────┘  └──────────┘
                     │
                     ▼
              ┌──────────────┐
              │ Append Result │
              │ Loop Again    │──→ (back to Call LLM)
              └──────────────┘
```

---

### 4.11 `sea_telegram.h/.c` — Telegram Bot Interface

**Purpose:** Long-polling Telegram bot. Receives messages, dispatches them through a handler callback, sends responses back.

**API:**
```c
SeaError sea_telegram_init(SeaTelegram* tg, const char* token, i64 chat_id,
                            SeaTelegramHandler handler, SeaArena* arena);
SeaError sea_telegram_poll(SeaTelegram* tg);
SeaError sea_telegram_send(SeaTelegram* tg, i64 chat_id, const char* text);
SeaError sea_telegram_send_slice(SeaTelegram* tg, i64 chat_id, SeaSlice text);
SeaError sea_telegram_get_me(SeaTelegram* tg, SeaArena* arena);
```

**Handler Signature:**
```c
typedef SeaError (*SeaTelegramHandler)(i64 chat_id, SeaSlice text,
                                       SeaArena* arena, SeaSlice* response);
```

---

### 4.12 `sea_a2a.h/.c` — Agent-to-Agent Protocol

**Purpose:** Delegate tasks to remote agents (OpenClaw, Agent-0, other Sea-Claw instances) via HTTP JSON-RPC. Shield-verifies results before returning.

**API:**
```c
SeaA2aResult sea_a2a_delegate(const SeaA2aPeer* peer, const SeaA2aRequest* req, SeaArena* arena);
bool         sea_a2a_heartbeat(const SeaA2aPeer* peer, SeaArena* arena);
i32          sea_a2a_discover(const char* url, SeaA2aPeer* out, i32 max, SeaArena* arena);
```

---

### 4.13 `main.c` — The Nervous System

**Purpose:** Entry point. Parses CLI args, initializes all subsystems, runs either TUI or Telegram mode.

**Dual-Mode Operation:**
```
./sea_claw                          # Interactive TUI mode
./sea_claw --telegram <token>       # Telegram bot mode
./sea_claw --config config.json     # Custom config
./sea_claw --db mydata.db           # Custom database path
./sea_claw --chat 890034905         # Restrict to one chat
```

**15 Telegram Slash Commands:**
```
/help           — Full command reference
/status         — System status & memory
/tools          — List all 50 tools
/task list      — List tasks
/task create    — Create a task
/task done <id> — Complete a task
/file read      — Read a file
/file write     — Write a file
/shell          — Run shell command
/web            — Fetch URL content
/session clear  — Clear chat history
/model          — Show current LLM model
/audit          — View audit trail
/delegate       — Delegate to remote agent
/exec           — Raw tool execution
```

---

## 5. Function Flow

### 5.1 Startup Flow

```
main()
  ├── signal(SIGINT, handle_signal)
  ├── sea_log_init(SEA_LOG_INFO)
  ├── load_dotenv(".env")
  ├── sea_config_load(&config, "config.json", &cfg_arena)
  ├── sea_arena_create(&session_arena, 16MB)
  ├── sea_arena_create(&request_arena, 1MB)
  ├── sea_tools_init()                    // logs "50 tools"
  ├── sea_db_open(&db, "seaclaw.db")
  │     └── CREATE TABLE IF NOT EXISTS (trajectory, config, tasks, chat_history)
  ├── sea_agent_init(&agent_cfg)          // logs provider + model
  │     └── Resolves API keys from env vars per provider
  └── run_telegram() OR tui_loop()
```

### 5.2 Telegram Message Flow

```
sea_telegram_poll()
  ├── HTTP GET /getUpdates (long poll, 30s timeout)
  ├── Parse JSON response
  └── For each update:
        ├── Extract chat_id, text
        ├── Call telegram_handler(chat_id, text, arena, &response)
        │     ├── sea_shield_detect_injection(text)     // Security gate
        │     ├── If /command:
        │     │     ├── /status → sea_tool_exec("system_status", ...)
        │     │     ├── /tools  → iterate sea_tool_by_id(1..50)
        │     │     ├── /exec   → sea_tool_exec(name, args, ...)
        │     │     ├── /task   → sea_tool_exec("task_manage", ...)
        │     │     ├── /file   → sea_tool_exec("file_read"|"file_write", ...)
        │     │     ├── /shell  → sea_tool_exec("shell_exec", ...)
        │     │     ├── /web    → sea_tool_exec("web_fetch", ...)
        │     │     ├── /audit  → sea_db_recent_events(...)
        │     │     ├── /delegate → sea_a2a_delegate(...)
        │     │     └── /session clear → sea_db_chat_clear(...)
        │     └── If natural language:
        │           ├── sea_db_chat_history(db, chat_id, ...) // Load context
        │           ├── sea_agent_chat(&cfg, history, input, arena)
        │           │     ├── Build system prompt with tool descriptions
        │           │     ├── POST to LLM API (with fallback chain)
        │           │     ├── Parse response for tool_call JSON
        │           │     ├── If tool_call: sea_tool_exec(name, args, ...)
        │           │     └── Loop until final text response
        │           └── sea_db_chat_log(db, chat_id, "user"|"assistant", ...)
        ├── sea_telegram_send(tg, chat_id, response)
        └── sea_arena_reset(arena)        // Instant cleanup
```

### 5.3 Tool Execution Flow

```
sea_tool_exec("hash_compute", args, arena, &output)
  ├── sea_tool_by_name("hash_compute")    // O(n) scan of static array
  ├── SEA_LOG_INFO("HANDS", "Executing tool: hash_compute")
  ├── tool->func(args, arena, output)     // Direct function pointer call
  │     └── tool_hash_compute():
  │           ├── Parse args: "<algorithm> <text>"
  │           ├── Compute hash (CRC32/DJB2/FNV-1a)
  │           ├── Format result into arena buffer
  │           └── Set output->data, output->len
  └── Return SEA_OK or SEA_ERR_TOOL_FAILED
```

---

## 6. Build System

### Makefile Targets

```bash
make all        # Build sea_claw binary (debug, with ASan)
make release    # Build optimized (-O3, stripped)
make test       # Run all 5 test suites (61 tests)
make clean      # Remove all .o files and binaries
```

### Compiler Flags

```
Debug:   -std=c11 -Wall -Wextra -Werror -Wpedantic -O0 -g -fsanitize=address,undefined
Release: -std=c11 -Wall -Wextra -Werror -Wpedantic -O3 -DNDEBUG -flto
x86:     -mavx2 -mfma -DSEA_ARCH_X86
ARM:     -DSEA_ARCH_ARM
```

### Link Libraries

```
-lm        # Math library (floor, ceil, fmod)
-lcurl     # HTTP client (Telegram API, LLM APIs, web fetch)
-lsqlite3  # Embedded database
```

---

## 7. Memory Model

Sea-Claw uses **two arenas** and **zero malloc**:

| Arena | Size | Purpose | Lifetime |
|-------|------|---------|----------|
| `s_session_arena` | 16 MB | Long-lived data (config strings) | Program lifetime |
| `s_request_arena` | 1 MB | Per-request allocations | Reset after each message |

**Per-Request Cycle:**
```
1. Message arrives (Telegram or TUI)
2. All processing uses s_request_arena
3. Tool outputs, JSON parsing, HTTP responses → arena
4. Response sent back to user
5. sea_arena_reset(&s_request_arena)  // ONE instruction
6. All memory from step 2-4 is instantly reclaimed
```

**Why No malloc:**
- No memory leaks possible
- No use-after-free possible
- No fragmentation
- Deterministic performance (no GC pauses)
- Arena reset is O(1) — just move a pointer

---

## 8. Security Model

### Defense in Depth

```
Layer 1: Shield Grammar Filter
  └── Every input byte checked against charset bitmap (<1μs)
  └── Rejects: shell injection, SQL injection, XSS, control chars

Layer 2: Shield Injection Detector
  └── Pattern matching for: $(cmd), `cmd`, ; rm, | cat, etc.
  └── Applied to ALL user input before any processing

Layer 3: Tool-Level Validation
  └── Each tool validates its own arguments
  └── File paths checked by Shield before fopen()
  └── SQL queries restricted to SELECT/PRAGMA only

Layer 4: Static Tool Registry
  └── AI cannot invent new tools at runtime
  └── No eval(), no exec(), no dynamic loading
  └── Tools are function pointers — one CPU JUMP instruction

Layer 5: Arena Memory Safety
  └── No malloc/free → no use-after-free, no double-free
  └── Buffer sizes enforced at allocation time
  └── Arena bounds checked on every allocation

Layer 6: A2A Output Verification
  └── Remote agent responses Shield-validated before use
```

---

## 9. Configuration

### config.json Example

```json
{
  "telegram_token": "",
  "telegram_chat_id": 0,
  "db_path": "seaclaw.db",
  "log_level": "info",
  "arena_size_mb": 16,
  "llm_provider": "openrouter",
  "llm_api_key": "",
  "llm_model": "moonshotai/kimi-k2.5",
  "llm_api_url": "",
  "llm_fallbacks": [
    { "provider": "openai", "model": "gpt-4o-mini" },
    { "provider": "gemini", "model": "gemini-2.0-flash" }
  ]
}
```

### .env File (API Keys)

```bash
OPENROUTER_API_KEY=sk-or-...
OPENAI_API_KEY=sk-...
GEMINI_API_KEY=AI...
TELEGRAM_BOT_TOKEN=123456:ABC...
TELEGRAM_CHAT_ID=890034905
EXA_API_KEY=exa-...
```

---

## 10. External Dependencies

| Dependency | Version | Purpose | Install |
|------------|---------|---------|---------|
| **gcc** | 11+ | C11 compiler | `apt install build-essential` |
| **libcurl** | 7.x | HTTP client | `apt install libcurl4-openssl-dev` |
| **libsqlite3** | 3.x | Embedded database | `apt install libsqlite3-dev` |
| **make** | 4.x | Build system | `apt install make` |

That's it. Two runtime libraries. No npm. No pip. No cargo. No go modules.

---

*Generated from Sea-Claw v1.0.0 source code — 74 files, 9,159 lines of C11.*
