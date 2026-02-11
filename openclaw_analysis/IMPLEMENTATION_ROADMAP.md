# OpenClaw-C: Master Implementation Roadmap

**Version:** 1.0.0  
**Project:** OpenClaw-C - C-Language Reimplementation of OpenClaw  
**Timeline:** 20 Weeks (5 Months)  
**Target:** Production-ready AI assistant gateway with native inference

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Project Architecture Overview](#project-architecture-overview)
3. [Directory Structure](#directory-structure)
4. [Phase 1: Foundation (Weeks 1-4)](#phase-1-foundation-weeks-1-4)
5. [Phase 2: Inference Engine (Weeks 5-8)](#phase-2-inference-engine-weeks-5-8)
6. [Phase 3: Tool System (Weeks 9-11)](#phase-3-tool-system-weeks-9-11)
7. [Phase 4: Agent Loop (Weeks 12-14)](#phase-4-agent-loop-weeks-12-14)
8. [Phase 5: TUI & Channels (Weeks 15-17)](#phase-5-tui--channels-weeks-15-17)
9. [Phase 6: Security & Hardening (Weeks 18-20)](#phase-6-security--hardening-weeks-18-20)
10. [Dependencies & Libraries](#dependencies--libraries)
11. [Testing Strategy](#testing-strategy)
12. [Risk Mitigation](#risk-mitigation)

---

## Executive Summary

This roadmap outlines a 20-week implementation plan for OpenClaw-C, a high-performance C reimplementation of the OpenClaw AI assistant gateway. The project synthesizes insights from:

- **Code Analysis:** Understanding of TypeScript architecture and performance bottlenecks
- **Security Audit:** CVE mitigation and secure coding practices
- **C Architecture:** mmap-based model loading, indexed file systems, event-driven design
- **Agent Framework:** Grammar-constrained tool calling, multi-model routing, secure credential storage

### Key Design Principles

1. **Memory Efficiency:** mmap-based model loading - 4GB+ models on 8GB systems
2. **Performance:** O(1) tensor lookups, SIMD-accelerated operations, lock-free data structures
3. **Security:** Sandboxed tool execution, encrypted credential storage, input validation
4. **Extensibility:** Plugin system for external models, modular channel adapters
5. **Reliability:** State machine-based agent loop, automatic failover, persistence

---

## Project Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                         OPENCLAW-C SYSTEM ARCHITECTURE                          │
├─────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│  ┌─────────────────────────────────────────────────────────────────────────┐   │
│  │                         PRESENTATION LAYER                               │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌─────────────┐  │   │
│  │  │ TUI (ncurses)│  │ CLI Parser   │  │ Dashboard    │  │ Web API     │  │   │
│  │  └──────────────┘  └──────────────┘  └──────────────┘  └─────────────┘  │   │
│  └─────────────────────────────────────────────────────────────────────────┘   │
│                                    │                                            │
│  ┌─────────────────────────────────▼─────────────────────────────────────────┐  │
│  │                         CORE ENGINE LAYER                                  │  │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌─────────────┐    │  │
│  │  │ Event Loop   │  │ Agent State  │  │ Context Mgr  │  │ Memory Mgr  │    │  │
│  │  │ (epoll)      │  │ Machine      │  │              │  │ (arena/mmap)│    │  │
│  │  └──────────────┘  └──────────────┘  └──────────────┘  └─────────────┘    │  │
│  └───────────────────────────────────────────────────────────────────────────┘  │
│                                    │                                            │
│  ┌─────────────────────────────────▼─────────────────────────────────────────┐  │
│  │                      INFERENCE ENGINE LAYER                                │  │
│  │  ┌─────────────────────────────────────────────────────────────────────┐   │  │
│  │  │                    MODEL INDEX (mmap-based)                          │   │  │
│  │  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐  │   │  │
│  │  │  │Attention │ │   FFN    │ │Embedding│ │  Norm    │ │ Output │  │   │  │
│  │  │  │  Blocks  │ │  Blocks  │ │  Table  │ │ Layers   │ │  Head  │  │   │  │
│  │  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘ └──────────┘  │   │  │
│  │  │                                                                      │   │  │
│  │  │  ┌────────────────────────────────────────────────────────────────┐ │   │  │
│  │  │  │              GRAMMAR CONSTRAINT ENGINE                          │ │   │  │
│  │  │  │  (BNF/JSON Schema → Grammar Automaton → Constrained Decode)    │ │   │  │
│  │  │  └────────────────────────────────────────────────────────────────┘ │   │  │
│  │  └─────────────────────────────────────────────────────────────────────┘   │  │
│  └───────────────────────────────────────────────────────────────────────────┘  │
│                                    │                                            │
│  ┌─────────────────────────────────▼─────────────────────────────────────────┐  │
│  │                        SERVICE LAYER                                       │  │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌─────────────┐    │  │
│  │  │ Tool Registry│  │ Skill System │  │ Model Router │  │ Credential  │    │  │
│  │  │ (mmap hash)  │  │ (SQLite)     │  │ (Multi-LLM)  │  │ Keyring     │    │  │
│  │  └──────────────┘  └──────────────┘  └──────────────┘  └─────────────┘    │  │
│  └───────────────────────────────────────────────────────────────────────────┘  │
│                                    │                                            │
│  ┌─────────────────────────────────▼─────────────────────────────────────────┐  │
│  │                      ADAPTER LAYER                                         │  │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌────────────────┐   │  │
│  │  │ Telegram │ │ WhatsApp │ │  Slack   │ │ Discord  │ │ External APIs  │   │  │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘ └────────────────┘   │  │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌────────────────┐   │  │
│  │  │  Ollama  │ │  RunPod  │ │  Kimi    │ │ OpenAI   │ │  Anthropic     │   │  │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘ └────────────────┘   │  │
│  └───────────────────────────────────────────────────────────────────────────┘  │
│                                                                                 │
└─────────────────────────────────────────────────────────────────────────────────┘
```

---

## Directory Structure

```
openclaw-c/
├── CMakeLists.txt              # Main build configuration
├── README.md                   # Project documentation
├── LICENSE                     # MIT License
├── VERSION                     # Version file
│
├── cmake/                      # CMake modules
│   ├── FindLlamaCpp.cmake
│   ├── FindGGML.cmake
│   └── FindNcursesw.cmake
│
├── docs/                       # Documentation
│   ├── API.md
│   ├── ARCHITECTURE.md
│   ├── SECURITY.md
│   └── DEPLOYMENT.md
│
├── include/                    # Public headers
│   └── openclaw/
│       ├── claw.h              # Main API header
│       ├── model.h             # Model loading API
│       ├── inference.h         # Inference engine API
│       ├── grammar.h           # Grammar constraint API
│       ├── agent.h             # Agent runtime API
│       ├── tool.h              # Tool registry API
│       ├── channel.h           # Channel adapter API
│       ├── tui.h               # Terminal UI API
│       ├── memory.h            # Memory management API
│       ├── event.h             # Event loop API
│       ├── ipc.h               # IPC mechanism API
│       ├── security.h          # Security/credential API
│       └── utils.h             # Utility functions
│
├── src/                        # Source code
│   ├── core/                   # Core infrastructure
│   │   ├── main.c              # Entry point
│   │   ├── config.c            # Configuration management
│   │   ├── config.h
│   │   ├── globals.c           # Global state
│   │   ├── globals.h
│   │   ├── version.c           # Version info
│   │   └── version.h
│   │
│   ├── memory/                 # Memory management
│   │   ├── arena.c             # Arena allocator
│   │   ├── arena.h
│   │   ├── pool.c              # Object pool allocator
│   │   ├── pool.h
│   │   ├── mmap_alloc.c        # mmap-based allocator
│   │   ├── mmap_alloc.h
│   │   ├── cache.c             # LRU cache
│   │   ├── cache.h
│   │   ├── memstats.c          # Memory statistics
│   │   └── memstats.h
│   │
│   ├── logging/                # Logging system
│   │   ├── log.c               # Core logging
│   │   ├── log.h
│   │   ├── log_levels.c        # Log level management
│   │   ├── log_levels.h
│   │   ├── log_formatters.c    # Output formatters
│   │   └── log_formatters.h
│   │
│   ├── event/                  # Event loop
│   │   ├── event_loop.c        # Main event loop
│   │   ├── event_loop.h
│   │   ├── event_epoll.c       # Linux epoll backend
│   │   ├── event_kqueue.c      # BSD/macOS kqueue backend
│   │   ├── event_iocp.c        # Windows IOCP backend
│   │   ├── timer.c             # Timer wheel
│   │   ├── timer.h
│   │   ├── channel.c           # Cross-thread channels
│   │   └── channel.h
│   │
│   ├── data_structures/        # Core data structures
│   │   ├── hash_table.c        # Hash table implementation
│   │   ├── hash_table.h
│   │   ├── vector.c            # Dynamic array
│   │   ├── vector.h
│   │   ├── string.c            # String utilities
│   │   ├── string.h
│   │   ├── buffer.c            # Byte buffer
│   │   ├── buffer.h
│   │   ├── ring_buffer.c       # Lock-free ring buffer
│   │   └── ring_buffer.h
│   │
│   ├── model/                  # Model loading
│   │   ├── model_index.c       # Model index (mmap-based)
│   │   ├── model_index.h
│   │   ├── model_loader.c      # GGUF/ClawModel loader
│   │   ├── model_loader.h
│   │   ├── tensor.c            # Tensor operations
│   │   ├── tensor.h
│   │   ├── tensor_cache.c      # Tensor LRU cache
│   │   └── tensor_cache.h
│   │
│   ├── inference/              # Inference engine
│   │   ├── inference_ctx.c     # Inference context
│   │   ├── inference_ctx.h
│   │   ├── llama_backend.c     # llama.cpp integration
│   │   ├── llama_backend.h
│   │   ├── sampler.c           # Token sampling
│   │   ├── sampler.h
│   │   ├── kv_cache.c          # KV cache management
│   │   ├── kv_cache.h
│   │   ├── tokenizer.c         # Tokenization
│   │   └── tokenizer.h
│   │
│   ├── grammar/                # Grammar constraints
│   │   ├── grammar.c           # Grammar definition
│   │   ├── grammar.h
│   │   ├── grammar_parser.c    # BNF parser
│   │   ├── grammar_parser.h
│   │   ├── grammar_compile.c   # Grammar compiler
│   │   ├── grammar_compile.h
│   │   ├── constraint_engine.c # Constrained decoding
│   │   ├── constraint_engine.h
│   │   ├── json_schema.c       # JSON Schema → Grammar
│   │   └── json_schema.h
│   │
│   ├── tool/                   # Tool system
│   │   ├── tool_registry.c     # Tool registry (mmap)
│   │   ├── tool_registry.h
│   │   ├── tool_parser.c       # Tool call parser (GLL)
│   │   ├── tool_parser.h
│   │   ├── tool_executor.c     # Tool execution
│   │   ├── tool_executor.h
│   │   ├── tool_sandbox.c      # Sandbox implementation
│   │   ├── tool_sandbox.h
│   │   ├── builtin_tools.c     # Built-in tool implementations
│   │   └── builtin_tools.h
│   │
│   ├── skill/                  # Skill system
│   │   ├── skill_registry.c    # Skill registry (SQLite)
│   │   ├── skill_registry.h
│   │   ├── skill_loader.c      # Skill loader
│   │   ├── skill_loader.h
│   │   └── skill_builtin.c     # Built-in skills
│   │
│   ├── agent/                  # Agent runtime
│   │   ├── agent.c             # Agent instance
│   │   ├── agent.h
│   │   ├── agent_state.c       # State machine
│   │   ├── agent_state.h
│   │   ├── agent_loop.c        # Main agent loop
│   │   ├── agent_loop.h
│   │   ├── context_window.c    # Context management
│   │   ├── context_window.h
│   │   ├── planner.c           # Planning engine
│   │   ├── planner.h
│   │   ├── reflection.c        # Reflection engine
│   │   └── reflection.h
│   │
│   ├── router/                 # Model routing
│   │   ├── model_router.c      # Multi-model router
│   │   ├── model_router.h
│   │   ├── provider_local.c    # Local model provider
│   │   ├── provider_local.h
│   │   ├── provider_ollama.c   # Ollama provider
│   │   ├── provider_ollama.h
│   │   ├── provider_openai.c   # OpenAI provider
│   │   ├── provider_openai.h
│   │   ├── provider_anthropic.c # Anthropic provider
│   │   └── provider_anthropic.h
│   │
│   ├── security/               # Security
│   │   ├── keyring.c           # Secure credential storage
│   │   ├── keyring.h
│   │   ├── keyring_file.c      # File-based encrypted backend
│   │   ├── keyring_libsecret.c # libsecret backend
│   │   ├── sandbox.c           # Sandbox implementation
│   │   ├── sandbox.h
│   │   ├── audit.c             # Audit logging
│   │   └── audit.h
│   │
│   ├── channel/                # Channel adapters
│   │   ├── channel_registry.c  # Channel registry
│   │   ├── channel_registry.h
│   │   ├── channel_terminal.c  # Terminal channel
│   │   ├── channel_terminal.h
│   │   ├── channel_telegram.c  # Telegram adapter
│   │   ├── channel_telegram.h
│   │   ├── channel_slack.c     # Slack adapter
│   │   ├── channel_slack.h
│   │   ├── channel_discord.c   # Discord adapter
│   │   └── channel_discord.h
│   │
│   ├── tui/                    # Terminal UI
│   │   ├── tui_main.c          # Main TUI
│   │   ├── tui_main.h
│   │   ├── tui_chat.c          # Chat window
│   │   ├── tui_chat.h
│   │   ├── tui_dashboard.c     # Dashboard panel
│   │   ├── tui_dashboard.h
│   │   ├── tui_tools.c         # Tool panel
│   │   ├── tui_tools.h
│   │   ├── tui_input.c         # Input handling
│   │   └── tui_input.h
│   │
│   ├── ipc/                    # IPC mechanism
│   │   ├── ipc_server.c        # IPC server
│   │   ├── ipc_server.h
│   │   ├── ipc_client.c        # IPC client
│   │   ├── ipc_client.h
│   │   ├── ipc_protocol.c      # Protocol implementation
│   │   └── ipc_protocol.h
│   │
│   └── utils/                  # Utilities
│       ├── error.c             # Error handling
│       ├── error.h
│       ├── json.c              # JSON utilities
│       ├── json.h
│       ├── base64.c            # Base64 encoding
│       ├── base64.h
│       ├── uuid.c              # UUID generation
│       ├── uuid.h
│       ├── time.c              # Time utilities
│       └── time.h
│
├── tests/                      # Test suite
│   ├── CMakeLists.txt
│   ├── unit/                   # Unit tests
│   │   ├── test_arena.c
│   │   ├── test_hash_table.c
│   │   ├── test_grammar.c
│   │   ├── test_tool_parser.c
│   │   └── test_model_index.c
│   ├── integration/            # Integration tests
│   │   ├── test_inference.c
│   │   ├── test_agent_loop.c
│   │   └── test_tool_system.c
│   ├── fuzz/                   # Fuzz tests
│   │   ├── fuzz_tool_parser.c
│   │   └── fuzz_grammar.c
│   └── fixtures/               # Test fixtures
│       ├── models/
│       ├── grammars/
│       └── tools/
│
├── tools/                      # Development tools
│   ├── model_converter.c       # Convert GGUF to ClawModel
│   ├── grammar_validator.c     # Validate grammar files
│   └── benchmark.c             # Performance benchmarks
│
├── scripts/                    # Build/deployment scripts
│   ├── build.sh
│   ├── install.sh
│   └── docker/
│       ├── Dockerfile
│       └── docker-compose.yml
│
└── third_party/                # External dependencies
    ├── llama.cpp/              # llama.cpp submodule
    ├── ggml/                   # GGML submodule
    └── cmake/                  # CMake fetch content

```

---

## Phase 1: Foundation (Weeks 1-4)

**Goal:** Establish the core infrastructure, build system, memory management, and logging framework.

### Week 1: Build System & Project Structure

#### Files to Create
| File | Description | Lines (est.) |
|------|-------------|--------------|
| `CMakeLists.txt` | Main CMake configuration | 200 |
| `cmake/FindLlamaCpp.cmake` | llama.cpp finder module | 80 |
| `cmake/FindNcursesw.cmake` | ncurses finder module | 60 |
| `src/core/version.c/h` | Version management | 50 |
| `src/core/globals.c/h` | Global state | 100 |
| `src/core/config.c/h` | Configuration parser | 300 |

#### Key Functions to Implement
```c
// config.h
int claw_config_load(const char *path, claw_config_t **config);
int claw_config_save(const char *path, const claw_config_t *config);
const char *claw_config_get(claw_config_t *config, const char *key);
int claw_config_set(claw_config_t *config, const char *key, const char *value);
void claw_config_free(claw_config_t *config);

// version.h
const char *claw_version_string(void);
uint32_t claw_version_major(void);
uint32_t claw_version_minor(void);
uint32_t claw_version_patch(void);
```

#### Dependencies to Integrate
- CMake 3.20+
- pkg-config
- Git submodules for llama.cpp

#### Testing Strategy
- CMake configuration tests
- Version string validation
- Config file parsing tests

#### Milestone Criteria
- [ ] Project builds with `cmake -B build && cmake --build build`
- [ ] Version information accessible
- [ ] Configuration file loads/saves correctly
- [ ] CI pipeline established

---

### Week 2: Memory Management

#### Files to Create
| File | Description | Lines (est.) |
|------|-------------|--------------|
| `src/memory/arena.c/h` | Arena allocator | 250 |
| `src/memory/pool.c/h` | Object pool allocator | 200 |
| `src/memory/mmap_alloc.c/h` | mmap-based allocator | 300 |
| `src/memory/cache.c/h` | LRU cache | 250 |
| `src/memory/memstats.c/h` | Memory statistics | 150 |

#### Key Functions to Implement
```c
// arena.h - Fast, linear allocation with rollback
typedef struct claw_arena claw_arena_t;

claw_arena_t *claw_arena_create(size_t initial_size);
void *claw_arena_alloc(claw_arena_t *arena, size_t size);
void *claw_arena_alloc_aligned(claw_arena_t *arena, size_t size, size_t align);
void claw_arena_savepoint(claw_arena_t *arena);
void claw_arena_rollback(claw_arena_t *arena);
void claw_arena_reset(claw_arena_t *arena);
void claw_arena_destroy(claw_arena_t *arena);

// pool.h - O(1) fixed-size allocation
typedef struct claw_pool claw_pool_t;

claw_pool_t *claw_pool_create(size_t obj_size, size_t objs_per_chunk);
void *claw_pool_alloc(claw_pool_t *pool);
void claw_pool_free(claw_pool_t *pool, void *ptr);
void claw_pool_destroy(claw_pool_t *pool);

// mmap_alloc.h - Large, file-backed allocation
typedef struct claw_mmap_alloc claw_mmap_alloc_t;

claw_mmap_alloc_t *claw_mmap_alloc_file(int fd, size_t offset, size_t size, uint32_t flags);
claw_mmap_alloc_t *claw_mmap_alloc_anon(size_t size);
void *claw_mmap_get_ptr(claw_mmap_alloc_t *alloc);
void claw_mmap_prefetch(claw_mmap_alloc_t *alloc, size_t offset, size_t len);
void claw_mmap_evict(claw_mmap_alloc_t *alloc, size_t offset, size_t len);
void claw_mmap_free(claw_mmap_alloc_t *alloc);

// cache.h - LRU cache for tensors
typedef struct claw_cache claw_cache_t;

claw_cache_t *claw_cache_create(size_t max_entries, size_t max_bytes);
int claw_cache_insert(claw_cache_t *cache, uint64_t key, void *data, size_t size);
void *claw_cache_get(claw_cache_t *cache, uint64_t key, size_t *size);
void claw_cache_evict(claw_cache_t *cache, uint64_t key);
void claw_cache_destroy(claw_cache_t *cache);
```

#### Dependencies to Integrate
- POSIX mmap/munmap
- madvise for prefetch hints

#### Testing Strategy
- Unit tests for each allocator
- Memory leak detection with Valgrind
- Stress tests with concurrent access
- Benchmark vs malloc/free

#### Milestone Criteria
- [ ] Arena allocator passes all tests
- [ ] Pool allocator achieves O(1) alloc/free
- [ ] mmap allocator handles 4GB+ files
- [ ] LRU cache maintains size bounds
- [ ] Memory statistics accurate

---

### Week 3: Data Structures

#### Files to Create
| File | Description | Lines (est.) |
|------|-------------|--------------|
| `src/data_structures/hash_table.c/h` | Hash table | 300 |
| `src/data_structures/vector.c/h` | Dynamic array | 200 |
| `src/data_structures/string.c/h` | String utilities | 250 |
| `src/data_structures/buffer.c/h` | Byte buffer | 200 |
| `src/data_structures/ring_buffer.c/h` | Lock-free ring buffer | 250 |

#### Key Functions to Implement
```c
// hash_table.h - FNV-1a hash table
typedef struct claw_hash_table claw_hash_table_t;

claw_hash_table_t *claw_ht_create(size_t initial_buckets);
int claw_ht_insert(claw_hash_table_t *ht, const char *key, void *value);
void *claw_ht_get(claw_hash_table_t *ht, const char *key);
int claw_ht_remove(claw_hash_table_t *ht, const char *key);
void claw_ht_foreach(claw_hash_table_t *ht, void (*fn)(const char *, void *, void *), void *ctx);
void claw_ht_destroy(claw_hash_table_t *ht);

// vector.h - Type-safe dynamic array
#define CLAW_VECTOR_TYPE(T, NAME) \
    typedef struct { T *data; size_t size; size_t capacity; } NAME

CLAW_VECTOR_TYPE(void*, claw_vector_ptr);
CLAW_VECTOR_TYPE(int, claw_vector_int);
CLAW_VECTOR_TYPE(float, claw_vector_float);

#define claw_vector_init(v) do { (v).data = NULL; (v).size = 0; (v).capacity = 0; } while(0)
#define claw_vector_push(v, val) /* implementation */
#define claw_vector_pop(v) /* implementation */
#define claw_vector_free(v) /* implementation */

// ring_buffer.h - Lock-free SPSC ring buffer
typedef struct claw_ring_buffer claw_ring_buffer_t;

claw_ring_buffer_t *claw_rb_create(size_t capacity, size_t elem_size);
int claw_rb_push(claw_ring_buffer_t *rb, const void *elem);
int claw_rb_pop(claw_ring_buffer_t *rb, void *elem);
size_t claw_rb_available(claw_ring_buffer_t *rb);
void claw_rb_destroy(claw_ring_buffer_t *rb);
```

#### Testing Strategy
- Unit tests for all operations
- Concurrent access tests for ring buffer
- Fuzz testing for hash table

#### Milestone Criteria
- [ ] Hash table achieves O(1) average lookup
- [ ] Vector handles dynamic growth correctly
- [ ] Ring buffer is lock-free and thread-safe
- [ ] All data structures pass stress tests

---

### Week 4: Logging & Event Loop

#### Files to Create
| File | Description | Lines (est.) |
|------|-------------|--------------|
| `src/logging/log.c/h` | Core logging | 300 |
| `src/logging/log_levels.c/h` | Log level management | 100 |
| `src/logging/log_formatters.c/h` | Output formatters | 200 |
| `src/event/event_loop.c/h` | Event loop core | 400 |
| `src/event/event_epoll.c` | Linux epoll backend | 250 |
| `src/event/timer.c/h` | Timer wheel | 300 |
| `src/event/channel.c/h` | Cross-thread channels | 200 |

#### Key Functions to Implement
```c
// log.h - Structured logging
typedef enum {
    CLAW_LOG_TRACE = 0,
    CLAW_LOG_DEBUG = 1,
    CLAW_LOG_INFO = 2,
    CLAW_LOG_WARN = 3,
    CLAW_LOG_ERROR = 4,
    CLAW_LOG_FATAL = 5
} claw_log_level_t;

int claw_log_init(claw_log_level_t level, const char *output_path);
void claw_log_set_level(claw_log_level_t level);
void claw_log_trace(const char *fmt, ...);
void claw_log_debug(const char *fmt, ...);
void claw_log_info(const char *fmt, ...);
void claw_log_warn(const char *fmt, ...);
void claw_log_error(const char *fmt, ...);
void claw_log_fatal(const char *fmt, ...);
void claw_log_shutdown(void);

// event_loop.h - Cross-platform event loop
typedef struct claw_event_loop claw_event_loop_t;
typedef struct claw_event claw_event_t;
typedef void (*claw_event_handler_t)(claw_event_loop_t *loop, claw_event_t *event, void *userdata);

claw_event_loop_t *claw_event_loop_create(void);
int claw_event_loop_run(claw_event_loop_t *loop);
void claw_event_loop_stop(claw_event_loop_t *loop);
void claw_event_loop_destroy(claw_event_loop_t *loop);

// Event registration
claw_event_t *claw_event_add_io(claw_event_loop_t *loop, int fd, uint32_t flags,
                                 claw_event_handler_t handler, void *userdata);
claw_event_t *claw_event_add_timer(claw_event_loop_t *loop, uint64_t timeout_ms,
                                    uint64_t interval_ms, claw_event_handler_t handler,
                                    void *userdata);
claw_event_t *claw_event_add_signal(claw_event_loop_t *loop, int signum,
                                     claw_event_handler_t handler, void *userdata);
void claw_event_remove(claw_event_loop_t *loop, claw_event_t *event);

// timer.h - Hierarchical timing wheel
claw_event_t *claw_timer_add_oneshot(claw_event_loop_t *loop, uint64_t delay_ms,
                                      claw_event_handler_t handler, void *userdata);
claw_event_t *claw_timer_add_interval(claw_event_loop_t *loop, uint64_t interval_ms,
                                       claw_event_handler_t handler, void *userdata);
void claw_timer_cancel(claw_event_t *timer);
```

#### Dependencies to Integrate
- epoll (Linux)
- kqueue (BSD/macOS)
- IOCP (Windows)
- eventfd (Linux) / pipe (others)

#### Testing Strategy
- Log output validation
- Event loop stress tests
- Timer accuracy tests
- Signal handling tests

#### Milestone Criteria
- [ ] Logging outputs to file and stderr
- [ ] Event loop handles 10,000+ concurrent events
- [ ] Timer accuracy within 1ms
- [ ] Signal handling works correctly
- [ ] Cross-platform build succeeds

---

## Phase 2: Inference Engine (Weeks 5-8)

**Goal:** Implement the core inference engine with llama.cpp integration, mmap-based model loading, and grammar-constrained generation.

### Week 5: Model Index & mmap Loading

#### Files to Create
| File | Description | Lines (est.) |
|------|-------------|--------------|
| `src/model/model_index.c/h` | Model index structures | 400 |
| `src/model/model_loader.c/h` | GGUF/ClawModel loader | 500 |
| `src/model/tensor.c/h` | Tensor operations | 400 |
| `include/openclaw/model.h` | Public model API | 200 |

#### Key Functions to Implement
```c
// model.h - Public model API
typedef struct claw_model claw_model_t;
typedef struct claw_tensor claw_tensor_t;

// Model loading
claw_model_t *claw_model_load(const char *path, uint32_t flags);
claw_model_t *claw_model_load_gguf(const char *path, uint32_t flags);
int claw_model_unload(claw_model_t *model);

// Tensor access (O(1) lookup)
claw_tensor_t *claw_model_get_tensor(claw_model_t *model, const char *name);
claw_tensor_t *claw_model_get_tensor_by_type(claw_model_t *model, claw_tensor_type_t type, uint32_t layer);
size_t claw_model_get_tensor_count(claw_model_t *model);

// Tensor operations
void *claw_tensor_get_data(claw_tensor_t *tensor);
claw_dtype_t claw_tensor_get_dtype(claw_tensor_t *tensor);
const uint32_t *claw_tensor_get_dims(claw_tensor_t *tensor, uint32_t *ndim);
size_t claw_tensor_get_size_bytes(claw_tensor_t *tensor);

// Model info
const char *claw_model_get_name(claw_model_t *model);
const claw_model_arch_t *claw_model_get_arch(claw_model_t *model);
uint32_t claw_model_get_vocab_size(claw_model_t *model);
uint32_t claw_model_get_context_size(claw_model_t *model);

// Memory management
void claw_model_prefetch_layer(claw_model_t *model, uint32_t layer_idx);
void claw_model_evict_layer(claw_model_t *model, uint32_t layer_idx);
size_t claw_model_get_memory_usage(claw_model_t *model);

// model_index.h - Internal structures
#define CLAW_MODEL_MAGIC        0x434C4157  /* "CLAW" */
#define CLAW_MODEL_VERSION      1
#define CLAW_MAX_LAYERS         256
#define CLAW_MAX_TENSORS        65536
#define CLAW_TENSOR_NAME_LEN    128

typedef enum {
    CLAW_DTYPE_F32 = 0, CLAW_DTYPE_F16, CLAW_DTYPE_Q4_0, CLAW_DTYPE_Q4_1,
    CLAW_DTYPE_Q5_0, CLAW_DTYPE_Q5_1, CLAW_DTYPE_Q8_0, CLAW_DTYPE_Q8_1,
    CLAW_DTYPE_Q2_K, CLAW_DTYPE_Q3_K, CLAW_DTYPE_Q4_K, CLAW_DTYPE_Q5_K,
    CLAW_DTYPE_Q6_K, CLAW_DTYPE_Q8_K, CLAW_DTYPE_IQ4_NL
} claw_dtype_t;

typedef enum {
    CLAW_TENSOR_ATTN_Q = 0x01, CLAW_TENSOR_ATTN_K = 0x02,
    CLAW_TENSOR_ATTN_V = 0x04, CLAW_TENSOR_ATTN_O = 0x08,
    CLAW_TENSOR_FFN_UP = 0x10, CLAW_TENSOR_FFN_GATE = 0x20,
    CLAW_TENSOR_FFN_DOWN = 0x40, CLAW_TENSOR_EMBED = 0x80,
    CLAW_TENSOR_NORM = 0x100, CLAW_TENSOR_OUTPUT = 0x200
} claw_tensor_type_t;

// Tensor descriptor - fixed size for O(1) lookup
typedef struct __attribute__((packed)) {
    uint32_t name_hash;
    uint64_t file_offset;
    uint64_t size_bytes;
    uint32_t dims[4];
    uint16_t ndim;
    uint16_t dtype;
    uint16_t tensor_type;
    uint16_t layer_idx;
    char name[CLAW_TENSOR_NAME_LEN];
} claw_tensor_desc_t;

// Model architecture
typedef struct __attribute__((packed)) {
    uint32_t vocab_size, hidden_size, intermediate_size;
    uint32_t num_layers, num_heads, num_kv_heads;
    uint32_t max_position_embeddings, head_dim, sliding_window;
    float rms_norm_eps, rope_theta;
    uint8_t use_gqa, use_sliding_window;
} claw_model_arch_t;
```

#### Dependencies to Integrate
- llama.cpp (as submodule)
- GGML (bundled with llama.cpp)

#### Testing Strategy
- Load various GGUF models (Q4_K_M, Q5_K_M, Q8_0)
- Verify O(1) tensor lookup
- Test mmap prefetch/evict
- Memory usage validation

#### Milestone Criteria
- [ ] Load Qwen 2.5 7B Q4_K_M model via mmap
- [ ] Tensor lookup < 1 microsecond
- [ ] Memory usage < 2GB for 7B model
- [ ] Layer prefetch reduces inference latency

---

### Week 6: llama.cpp Integration

#### Files to Create
| File | Description | Lines (est.) |
|------|-------------|--------------|
| `src/inference/llama_backend.c/h` | llama.cpp wrapper | 500 |
| `src/inference/inference_ctx.c/h` | Inference context | 400 |
| `src/inference/tokenizer.c/h` | Tokenization | 350 |

#### Key Functions to Implement
```c
// llama_backend.h - llama.cpp integration
typedef struct claw_llama_ctx claw_llama_ctx_t;

claw_llama_ctx_t *claw_llama_init(claw_model_t *model, const claw_inference_params_t *params);
void claw_llama_free(claw_llama_ctx_t *ctx);

// Inference
int claw_llama_encode(claw_llama_ctx_t *ctx, const uint32_t *tokens, uint32_t n_tokens);
int claw_llama_decode(claw_llama_ctx_t *ctx, uint32_t token);
float *claw_llama_get_logits(claw_llama_ctx_t *ctx);

// KV cache
void claw_llama_kv_cache_clear(claw_llama_ctx_t *ctx);
void claw_llama_kv_cache_seq_rm(claw_llama_ctx_t *ctx, int32_t seq_id, int32_t p0, int32_t p1);
size_t claw_llama_kv_cache_size(claw_llama_ctx_t *ctx);

// Performance
void claw_llama_set_n_threads(claw_llama_ctx_t *ctx, uint32_t n_threads);
void claw_llama_set_batch_size(claw_llama_ctx_t *ctx, uint32_t batch_size);

// inference_ctx.h - High-level inference API
typedef struct claw_inference_ctx claw_inference_ctx_t;
typedef struct claw_inference_params claw_inference_params_t;

struct claw_inference_params {
    uint32_t n_threads;
    uint32_t batch_size;
    uint32_t context_size;
    float temperature;
    float top_p;
    uint32_t top_k;
    float repetition_penalty;
    uint32_t max_tokens;
    uint32_t seed;
    uint8_t stream_output;
};

claw_inference_ctx_t *claw_inference_create(claw_model_t *model, const claw_inference_params_t *params);
void claw_inference_destroy(claw_inference_ctx_t *ctx);

// Generate text
int claw_inference_generate(claw_inference_ctx_t *ctx, const char *prompt,
                            char *output, size_t output_size);
int claw_inference_generate_stream(claw_inference_ctx_t *ctx, const char *prompt,
                                   void (*callback)(const char *token, void *userdata),
                                   void *userdata);

// Tokenize
uint32_t *claw_inference_tokenize(claw_inference_ctx_t *ctx, const char *text, size_t *n_tokens);
char *claw_inference_detokenize(claw_inference_ctx_t *ctx, const uint32_t *tokens, size_t n_tokens);

// tokenizer.h - Tokenization utilities
claw_tokenizer_t *claw_tokenizer_load(const char *path);
void claw_tokenizer_free(claw_tokenizer_t *tokenizer);
uint32_t *claw_tokenizer_encode(claw_tokenizer_t *tokenizer, const char *text, size_t *n_tokens);
char *claw_tokenizer_decode(claw_tokenizer_t *tokenizer, const uint32_t *tokens, size_t n_tokens);
const char *claw_tokenizer_get_token_text(claw_tokenizer_t *tokenizer, uint32_t token_id);
```

#### Dependencies to Integrate
- llama.cpp library
- sentencepiece (for tokenization)

#### Testing Strategy
- Inference accuracy tests vs reference
- Performance benchmarks (tokens/sec)
- Tokenization round-trip tests
- Memory leak detection

#### Milestone Criteria
- [ ] Generate coherent text from Qwen 7B
- [ ] Achieve > 20 tokens/sec on CPU (8 threads)
- [ ] Tokenization matches reference
- [ ] KV cache management works correctly

---

### Week 7: Grammar Constraint Engine

#### Files to Create
| File | Description | Lines (est.) |
|------|-------------|--------------|
| `src/grammar/grammar.c/h` | Grammar structures | 300 |
| `src/grammar/grammar_parser.c/h` | BNF parser | 400 |
| `src/grammar/grammar_compile.c/h` | Grammar compiler | 350 |
| `src/grammar/constraint_engine.c/h` | Constrained decoding | 500 |

#### Key Functions to Implement
```c
// grammar.h - Grammar definition
typedef struct claw_grammar claw_grammar_t;
typedef struct claw_symbol claw_symbol_t;

typedef enum {
    CLAW_SYM_TERMINAL, CLAW_SYM_NONTERM, CLAW_SYM_REGEX,
    CLAW_SYM_CHAR_RANGE, CLAW_SYM_SEQUENCE, CLAW_SYM_CHOICE,
    CLAW_SYM_OPTIONAL, CLAW_SYM_STAR, CLAW_SYM_PLUS
} claw_symbol_type_t;

claw_grammar_t *claw_grammar_create(const char *name);
void claw_grammar_free(claw_grammar_t *grammar);

// Rule construction
claw_symbol_t *claw_grammar_add_terminal(claw_grammar_t *grammar, const char *value);
claw_symbol_t *claw_grammar_add_nonterm(claw_grammar_t *grammar, const char *name);
claw_symbol_t *claw_grammar_add_sequence(claw_grammar_t *grammar, claw_symbol_t **symbols, size_t count);
claw_symbol_t *claw_grammar_add_choice(claw_grammar_t *grammar, claw_symbol_t **symbols, size_t count);
claw_symbol_t *claw_grammar_add_optional(claw_grammar_t *grammar, claw_symbol_t *symbol);
claw_symbol_t *claw_grammar_add_star(claw_grammar_t *grammar, claw_symbol_t *symbol);
claw_symbol_t *claw_grammar_add_plus(claw_grammar_t *grammar, claw_symbol_t *symbol);
claw_symbol_t *claw_grammar_add_char_range(claw_grammar_t *grammar, uint32_t start, uint32_t end);

int claw_grammar_add_rule(claw_grammar_t *grammar, const char *name, claw_symbol_t *rhs, int is_start);
int claw_grammar_compile(claw_grammar_t *grammar);

// Predefined grammars
claw_grammar_t *claw_grammar_json(void);
claw_grammar_t *claw_grammar_json_object(void);
claw_grammar_t *claw_grammar_json_array(void);
claw_grammar_t *claw_grammar_tool_call(void);

// grammar_parser.h - BNF parser
claw_grammar_t *claw_grammar_parse_bnf(const char *bnf_text);
claw_grammar_t *claw_grammar_parse_ebnf(const char *ebnf_text);

// constraint_engine.h - Constrained decoding
typedef struct claw_constraint_ctx claw_constraint_ctx_t;

claw_constraint_ctx_t *claw_constraint_create(claw_grammar_t *grammar, claw_inference_ctx_t *inference);
void claw_constraint_free(claw_constraint_ctx_t *ctx);

// Apply constraints during sampling
void claw_constraint_apply(claw_constraint_ctx_t *ctx, float *logits, uint32_t vocab_size);
int claw_constraint_advance(claw_constraint_ctx_t *ctx, uint32_t token);
int claw_constraint_is_complete(claw_constraint_ctx_t *ctx);
const char *claw_constraint_get_partial(claw_constraint_ctx_t *ctx);

// Validation
int claw_constraint_validate(claw_constraint_ctx_t *ctx, const char *text);
```

#### Dependencies to Integrate
- PCRE2 (for regex support)

#### Testing Strategy
- Grammar parsing tests
- Constraint validation tests
- End-to-end constrained generation tests
- JSON output validation

#### Milestone Criteria
- [ ] Parse BNF grammar for JSON
- [ ] Generate valid JSON from model
- [ ] Tool call grammar produces valid calls
- [ ] Constraint overhead < 10%

---

### Week 8: KV Cache Management & Sampler

#### Files to Create
| File | Description | Lines (est.) |
|------|-------------|--------------|
| `src/inference/kv_cache.c/h` | KV cache management | 400 |
| `src/inference/sampler.c/h` | Token sampling | 350 |
| `src/model/tensor_cache.c/h` | Tensor LRU cache | 300 |

#### Key Functions to Implement
```c
// kv_cache.h - KV cache management
typedef struct claw_kv_cache claw_kv_cache_t;

claw_kv_cache_t *claw_kv_cache_create(const claw_model_arch_t *arch, size_t max_seq_len);
void claw_kv_cache_free(claw_kv_cache_t *cache);

// Cache operations
void claw_kv_cache_clear(claw_kv_cache_t *cache);
int claw_kv_cache_resize(claw_kv_cache_t *cache, size_t new_max_seq_len);
size_t claw_kv_cache_get_used_tokens(claw_kv_cache_t *cache);
size_t claw_kv_cache_get_max_tokens(claw_kv_cache_t *cache);

// Memory optimization
void claw_kv_cache_defrag(claw_kv_cache_t *cache);
int claw_kv_cache_save(claw_kv_cache_t *cache, const char *path);
int claw_kv_cache_load(claw_kv_cache_t *cache, const char *path);

// sampler.h - Token sampling
typedef struct claw_sampler claw_sampler_t;

typedef enum {
    CLAW_SAMPLE_GREEDY = 0,
    CLAW_SAMPLE_TEMPERATURE = 1,
    CLAW_SAMPLE_TOP_K = 2,
    CLAW_SAMPLE_TOP_P = 3,
    CLAW_SAMPLE_MIN_P = 4,
    CLAW_SAMPLE_TYPICAL = 5
} claw_sample_method_t;

claw_sampler_t *claw_sampler_create(claw_sample_method_t method);
void claw_sampler_free(claw_sampler_t *sampler);

void claw_sampler_set_temperature(claw_sampler_t *sampler, float temp);
void claw_sampler_set_top_k(claw_sampler_t *sampler, uint32_t k);
void claw_sampler_set_top_p(claw_sampler_t *sampler, float p);
void claw_sampler_set_repetition_penalty(claw_sampler_t *sampler, float penalty);
void claw_sampler_set_penalty_tokens(claw_sampler_t *sampler, const uint32_t *tokens, size_t count);

uint32_t claw_sampler_sample(claw_sampler_t *sampler, const float *logits, uint32_t vocab_size);
```

#### Testing Strategy
- KV cache correctness tests
- Sampler distribution tests
- Memory optimization tests
- Cache eviction tests

#### Milestone Criteria
- [ ] KV cache supports 32K context
- [ ] Sampler produces expected distributions
- [ ] Repetition penalty works
- [ ] Cache defrag reduces fragmentation

---

## Phase 3: Tool System (Weeks 9-11)

**Goal:** Implement the tool registry, grammar-based parser, and sandboxed execution environment.

### Week 9: Tool Registry

#### Files to Create
| File | Description | Lines (est.) |
|------|-------------|--------------|
| `src/tool/tool_registry.c/h` | Tool registry (mmap) | 500 |
| `src/tool/builtin_tools.c/h` | Built-in tools | 400 |
| `include/openclaw/tool.h` | Public tool API | 250 |

#### Key Functions to Implement
```c
// tool.h - Public tool API
typedef struct claw_tool_registry claw_tool_registry_t;
typedef struct claw_tool_desc claw_tool_desc_t;
typedef struct claw_tool_call claw_tool_call_t;
typedef struct claw_tool_result claw_tool_result_t;

typedef enum {
    CLAW_TOOL_BUILTIN = 0, CLAW_TOOL_PLUGIN, CLAW_TOOL_SCRIPT,
    CLAW_TOOL_HTTP, CLAW_TOOL_WEBSOCKET
} claw_tool_type_t;

typedef enum {
    CLAW_TOOL_SAFE = 0, CLAW_TOOL_CAUTIOUS, CLAW_TOOL_DANGEROUS, CLAW_TOOL_SANDBOXED
} claw_tool_safety_t;

// Registry management
claw_tool_registry_t *claw_tool_registry_create(const char *path);
void claw_tool_registry_destroy(claw_tool_registry_t *registry);
int claw_tool_registry_load(claw_tool_registry_t *registry);
int claw_tool_registry_save(claw_tool_registry_t *registry);

// Tool registration
int claw_tool_register(claw_tool_registry_t *registry, const claw_tool_desc_t *desc);
int claw_tool_unregister(claw_tool_registry_t *registry, const char *name);

// Tool discovery
claw_tool_desc_t *claw_tool_get(claw_tool_registry_t *registry, const char *name);
claw_tool_desc_t **claw_tool_list(claw_tool_registry_t *registry, size_t *count);
claw_tool_desc_t **claw_tool_search(claw_tool_registry_t *registry, const char *query, size_t *count);

// Tool execution
typedef int (*claw_tool_func_t)(const claw_tool_call_t *call, claw_tool_result_t *result, void *context);

int claw_tool_execute(claw_tool_registry_t *registry, const claw_tool_call_t *call,
                      claw_tool_result_t *result, uint32_t timeout_ms);
int claw_tool_execute_async(claw_tool_registry_t *registry, const claw_tool_call_t *call,
                            void (*callback)(claw_tool_result_t *result, void *userdata),
                            void *userdata);

// Tool descriptor
struct claw_tool_desc {
    char name[64];
    char namespace[64];
    char description[512];
    claw_tool_type_t type;
    claw_tool_safety_t safety_level;
    uint32_t timeout_ms;
    
    // Parameter schema
    struct {
        char name[64];
        char type[32];
        char description[256];
        uint8_t required;
    } params[32];
    uint32_t param_count;
    
    // Execution info
    union {
        struct { claw_tool_func_t func; } builtin;
        struct { char lib_path[256]; char symbol_name[128]; } plugin;
        struct { char interpreter[32]; char script_path[256]; } script;
        struct { char endpoint[256]; char method[16]; } http;
    } exec;
    
    // Statistics
    atomic_uint64_t call_count;
    atomic_uint64_t total_latency_us;
    atomic_uint64_t error_count;
};

struct claw_tool_call {
    char tool_name[64];
    char namespace[64];
    char call_id[64];
    char *params_json;
    size_t params_len;
    uint32_t timeout_ms;
    void *user_data;
};

struct claw_tool_result {
    char call_id[64];
    int status;
    char *result_json;
    size_t result_len;
    uint64_t latency_us;
    char error_msg[256];
};
```

#### Dependencies to Integrate
- SQLite (for tool metadata)
- inotify/kqueue (for registry change detection)

#### Testing Strategy
- Tool registration tests
- Registry persistence tests
- Tool discovery tests
- Concurrent access tests

#### Milestone Criteria
- [ ] Register 100+ tools in < 100ms
- [ ] Tool lookup O(1) average
- [ ] Registry persists across restarts
- [ ] Change detection works

---

### Week 10: Grammar-Based Tool Parser

#### Files to Create
| File | Description | Lines (est.) |
|------|-------------|--------------|
| `src/tool/tool_parser.c/h` | GLL parser | 500 |
| `src/grammar/json_schema.c/h` | JSON Schema → Grammar | 300 |

#### Key Functions to Implement
```c
// tool_parser.h - Grammar-based tool call parser

typedef enum {
    TOK_AT, TOK_LBRACE, TOK_RBRACE, TOK_LPAREN, TOK_RPAREN,
    TOK_LBRACKET, TOK_RBRACKET, TOK_ARROW, TOK_SEMICOLON, TOK_COMMA,
    TOK_EQUALS, TOK_DOT, TOK_SLASH, TOK_DOLLAR, TOK_STRING,
    TOK_NUMBER, TOK_BOOL, TOK_IDENTIFIER, TOK_EOF, TOK_ERROR
} claw_token_type_t;

typedef struct {
    claw_token_type_t type;
    const char *start;
    size_t length;
    uint32_t line;
    uint32_t column;
} claw_token_t;

typedef enum {
    PARAM_STRING, PARAM_NUMBER_INT, PARAM_NUMBER_FLOAT,
    PARAM_BOOL, PARAM_ARRAY, PARAM_REFERENCE
} claw_param_type_t;

typedef struct claw_param_value {
    claw_param_type_t type;
    union {
        struct { char *data; size_t len; } string;
        int64_t int_val;
        double float_val;
        bool bool_val;
        struct { struct claw_param_value *items; size_t count; } array;
        struct { char *ref_type; char *ref_name; } reference;
    };
} claw_param_value_t;

typedef struct {
    char *key;
    claw_param_value_t value;
} claw_parameter_t;

typedef struct claw_tool_invoke {
    char *namespace;
    char *name;
    claw_parameter_t *params;
    size_t param_count;
    struct claw_tool_invoke *next_chain;
} claw_tool_invoke_t;

typedef struct {
    bool success;
    claw_tool_invoke_t *invocations;
    size_t invocation_count;
    char *error_message;
    uint32_t error_line;
    uint32_t error_column;
} claw_parse_result_t;

// Parser API
claw_parse_result_t *claw_tool_parse(const char *input, size_t len);
void claw_tool_parse_free(claw_parse_result_t *result);
bool claw_tool_validate(const char *input, size_t len);

// Grammar-based validation
bool claw_tool_validate_against_schema(const char *input, const claw_tool_desc_t *desc);

// Serialization
char *claw_tool_invoke_serialize(const claw_tool_invoke_t *invoke);
claw_tool_invoke_t *claw_tool_invoke_deserialize(const char *json);

// json_schema.h - JSON Schema to Grammar conversion
claw_grammar_t *claw_json_schema_to_grammar(const char *schema_json);
char *claw_grammar_to_json_schema(const claw_grammar_t *grammar);
```

#### Dependencies to Integrate
- PCRE2 (for pattern matching)
- jansson (for JSON parsing)

#### Testing Strategy
- Parser unit tests
- Grammar validation tests
- Fuzz testing with random inputs
- Injection attack tests

#### Milestone Criteria
- [ ] Parse complex tool calls correctly
- [ ] Reject malformed/invalid calls
- [ ] Handle nested structures
- [ ] No parser crashes on fuzzing

---

### Week 11: Tool Execution Sandbox

#### Files to Create
| File | Description | Lines (est.) |
|------|-------------|--------------|
| `src/tool/tool_executor.c/h` | Tool execution | 400 |
| `src/tool/tool_sandbox.c/h` | Sandbox implementation | 500 |
| `src/skill/skill_registry.c/h` | Skill registry | 350 |

#### Key Functions to Implement
```c
// tool_executor.h - Tool execution engine
typedef struct claw_executor claw_executor_t;

claw_executor_t *claw_executor_create(claw_tool_registry_t *registry);
void claw_executor_destroy(claw_executor_t *executor);

// Execution modes
int claw_executor_execute_sync(claw_executor_t *executor, const claw_tool_call_t *call,
                               claw_tool_result_t *result);
int claw_executor_execute_async(claw_executor_t *executor, const claw_tool_call_t *call,
                                void (*callback)(claw_tool_result_t *, void *), void *userdata);

// Batch execution
int claw_executor_execute_parallel(claw_executor_t *executor, claw_tool_call_t *calls,
                                   size_t count, claw_tool_result_t *results);
int claw_executor_execute_chain(claw_executor_t *executor, claw_tool_invoke_t *chain,
                                claw_tool_result_t *final_result);

// tool_sandbox.h - Sandboxed execution
typedef struct claw_sandbox claw_sandbox_t;

typedef enum {
    CLAW_SANDBOX_NONE = 0,      // No sandbox
    CLAW_SANDBOX_CHROOT = 1,    // chroot jail
    CLAW_SANDBOX_NAMESPACE = 2, // Linux namespaces
    CLAW_SANDBOX_SECCOMP = 4,   // seccomp-bpf
    CLAW_SANDBOX_LANDLOCK = 8,  // Landlock LSM
    CLAW_SANDBOX_FULL = 0xFF    // All protections
} claw_sandbox_flags_t;

claw_sandbox_t *claw_sandbox_create(claw_sandbox_flags_t flags);
void claw_sandbox_destroy(claw_sandbox_t *sandbox);

// Sandbox configuration
int claw_sandbox_allow_read(claw_sandbox_t *sandbox, const char *path);
int claw_sandbox_allow_write(claw_sandbox_t *sandbox, const char *path);
int claw_sandbox_allow_exec(claw_sandbox_t *sandbox, const char *path);
int claw_sandbox_allow_network(claw_sandbox_t *sandbox, const char *host, uint16_t port);

// Execute in sandbox
int claw_sandbox_execute(claw_sandbox_t *sandbox, const char *command,
                         char **output, size_t *output_len, uint32_t timeout_ms);

// skill_registry.h - Skill system
typedef struct claw_skill_registry claw_skill_registry_t;
typedef struct claw_skill claw_skill_t;

claw_skill_registry_t *claw_skill_registry_create(const char *db_path);
void claw_skill_registry_destroy(claw_skill_registry_t *registry);

int claw_skill_register(claw_skill_registry_t *registry, const claw_skill_t *skill);
int claw_skill_unregister(claw_skill_registry_t *registry, const char *name);
claw_skill_t *claw_skill_get(claw_skill_registry_t *registry, const char *name);
claw_skill_t **claw_skill_search(claw_skill_registry_t *registry, const char *query, size_t *count);

int claw_skill_execute(claw_skill_registry_t *registry, const char *name,
                       const char *input, char **output);
```

#### Dependencies to Integrate
- Linux namespaces (clone, unshare)
- seccomp-bpf
- Landlock (if available)

#### Testing Strategy
- Sandbox escape tests
- Resource limit tests
- Timeout handling tests
- Skill execution tests

#### Milestone Criteria
- [ ] Sandbox prevents file system escape
- [ ] Network access controlled
- [ ] Resource limits enforced
- [ ] Timeout kills runaway processes

---

## Phase 4: Agent Loop (Weeks 12-14)

**Goal:** Implement the agent runtime with state machine, context window management, and multi-model routing.

### Week 12: Agent State Machine & Context Window

#### Files to Create
| File | Description | Lines (est.) |
|------|-------------|--------------|
| `src/agent/agent.c/h` | Agent instance | 400 |
| `src/agent/agent_state.c/h` | State machine | 350 |
| `src/agent/context_window.c/h` | Context management | 400 |
| `include/openclaw/agent.h` | Public agent API | 250 |

#### Key Functions to Implement
```c
// agent.h - Public agent API
typedef struct claw_agent claw_agent_t;
typedef struct claw_agent_config claw_agent_config_t;

typedef enum {
    CLAW_AGENT_IDLE = 0,
    CLAW_AGENT_THINKING,
    CLAW_AGENT_CALLING_TOOL,
    CLAW_AGENT_STREAMING,
    CLAW_AGENT_COMPACTING,
    CLAW_AGENT_PAUSED,
    CLAW_AGENT_ERROR,
    CLAW_AGENT_SHUTDOWN
} claw_agent_state_t;

typedef enum {
    CLAW_ROLE_SYSTEM = 0,
    CLAW_ROLE_USER,
    CLAW_ROLE_ASSISTANT,
    CLAW_ROLE_TOOL
} claw_message_role_t;

struct claw_agent_config {
    char name[64];
    char session_id[64];
    char model_id[64];
    
    // Behavior
    uint32_t max_tokens;
    float temperature;
    float top_p;
    uint32_t top_k;
    float repetition_penalty;
    
    // Context
    uint32_t context_window;
    uint32_t compact_threshold;
    
    // Tools
    char available_tools[32][64];
    uint32_t tool_count;
    uint8_t auto_tool_confirm;
    
    // Safety
    uint8_t sandbox_mode;
    uint8_t require_confirmation;
};

// Agent lifecycle
claw_agent_t *claw_agent_create(const claw_agent_config_t *config,
                                claw_tool_registry_t *tools,
                                claw_model_t *model);
void claw_agent_destroy(claw_agent_t *agent);

// State machine
claw_agent_state_t claw_agent_get_state(claw_agent_t *agent);
const char *claw_agent_state_string(claw_agent_state_t state);
int claw_agent_transition(claw_agent_t *agent, claw_agent_state_t new_state);

// Message processing
int claw_agent_process_message(claw_agent_t *agent, const char *message,
                               char **response, size_t *response_len);
int claw_agent_process_message_stream(claw_agent_t *agent, const char *message,
                                      void (*callback)(const char *chunk, void *userdata),
                                      void *userdata);

// Context management
int claw_agent_add_message(claw_agent_t *agent, claw_message_role_t role, const char *content);
int claw_agent_clear_context(claw_agent_t *agent);
int claw_agent_compact_context(claw_agent_t *agent);
size_t claw_agent_get_context_tokens(claw_agent_t *agent);

// Persistence
int claw_agent_save_state(claw_agent_t *agent, const char *path);
int claw_agent_load_state(claw_agent_t *agent, const char *path);

// agent_state.h - State machine implementation
typedef struct claw_state_machine claw_state_machine_t;

typedef struct {
    claw_agent_state_t from_state;
    claw_agent_state_t to_state;
    int (*validate)(claw_agent_t *agent, void *data);
    void (*on_enter)(claw_agent_t *agent, void *data);
    void (*on_exit)(claw_agent_t *agent, void *data);
} claw_state_transition_t;

claw_state_machine_t *claw_state_machine_create(const claw_state_transition_t *transitions,
                                                size_t count);
void claw_state_machine_destroy(claw_state_machine_t *sm);
int claw_state_machine_transition(claw_state_machine_t *sm, claw_agent_t *agent,
                                  claw_agent_state_t to_state, void *data);

// context_window.h - Context window management
typedef struct claw_context_window claw_context_window_t;
typedef struct claw_message claw_message_t;

struct claw_message {
    uint64_t id;
    claw_message_role_t role;
    uint64_t timestamp_ms;
    char *content;
    size_t content_len;
    uint32_t token_count;
    
    // Tool calls
    struct {
        char tool_name[64];
        char call_id[64];
        char *arguments;
    } tool_call;
    
    // Tool results
    struct {
        char call_id[64];
        char *output;
        int status;
    } tool_result;
    
    claw_message_t *prev;
    claw_message_t *next;
};

claw_context_window_t *claw_context_create(uint32_t max_tokens);
void claw_context_destroy(claw_context_window_t *ctx);

int claw_context_add_message(claw_context_window_t *ctx, const claw_message_t *msg);
int claw_context_remove_message(claw_context_window_t *ctx, uint64_t msg_id);
claw_message_t *claw_context_get_message(claw_context_window_t *ctx, uint64_t msg_id);

// Compaction strategies
int claw_context_compact_sliding(claw_context_window_t *ctx, uint32_t keep_last_n);
int claw_context_compact_summarize(claw_context_window_t *ctx, claw_inference_ctx_t *inference);
int claw_context_compact_selective(claw_context_window_t *ctx, float importance_threshold);

uint32_t claw_context_get_token_count(claw_context_window_t *ctx);
uint32_t claw_context_get_message_count(claw_context_window_t *ctx);
```

#### Dependencies to Integrate
- SQLite (for session persistence)

#### Testing Strategy
- State transition tests
- Context compaction tests
- Message handling tests
- Persistence tests

#### Milestone Criteria
- [ ] State machine transitions correctly
- [ ] Context window respects token limits
- [ ] Compaction preserves important messages
- [ ] Session persists and restores

---

### Week 13: Agent Loop & Planning

#### Files to Create
| File | Description | Lines (est.) |
|------|-------------|--------------|
| `src/agent/agent_loop.c/h` | Main agent loop | 450 |
| `src/agent/planner.c/h` | Planning engine | 400 |
| `src/agent/reflection.c/h` | Reflection engine | 300 |

#### Key Functions to Implement
```c
// agent_loop.h - Agent-0 style autonomous loop
typedef struct claw_agent_runtime claw_agent_runtime_t;

typedef enum {
    AGENT_STATE_IDLE = 0,
    AGENT_STATE_PLANNING,
    AGENT_STATE_EXECUTING,
    AGENT_STATE_WAITING_INPUT,
    AGENT_STATE_WAITING_TOOL,
    AGENT_STATE_REFLECTING,
    AGENT_STATE_HALTED,
    AGENT_STATE_ERROR
} claw_agent_runtime_state_t;

claw_agent_runtime_t *claw_agent_runtime_create(claw_agent_t *agent);
void claw_agent_runtime_destroy(claw_agent_runtime_t *runtime);

// Main loop
int claw_agent_runtime_start(claw_agent_runtime_t *runtime);
int claw_agent_runtime_stop(claw_agent_runtime_t *runtime);
int claw_agent_runtime_run_iteration(claw_agent_runtime_t *runtime);

// Observations
typedef enum {
    OBS_USER_INPUT, OBS_TOOL_OUTPUT, OBS_SYSTEM_EVENT,
    OBS_AGENT_ACTION, OBS_REFLECTION, OBS_PLAN_UPDATE
} claw_observation_type_t;

typedef struct {
    uint64_t id;
    claw_observation_type_t type;
    uint64_t timestamp;
    char *content;
    size_t content_len;
    float importance;
} claw_observation_t;

int claw_agent_add_observation(claw_agent_runtime_t *runtime, const claw_observation_t *obs);
claw_observation_t **claw_agent_get_observations(claw_agent_runtime_t *runtime, size_t *count);

// planner.h - Planning engine
typedef struct claw_planner claw_planner_t;
typedef struct claw_plan claw_plan_t;
typedef struct claw_plan_step claw_plan_step_t;

struct claw_plan_step {
    uint32_t step_number;
    char *description;
    claw_tool_invoke_t *tool_call;
    bool completed;
    bool failed;
    char *result;
    uint64_t started_at;
    uint64_t completed_at;
};

struct claw_plan {
    char *goal;
    claw_plan_step_t steps[64];
    size_t step_count;
    size_t current_step;
    bool is_revised;
    uint64_t created_at;
};

claw_planner_t *claw_planner_create(claw_inference_ctx_t *inference);
void claw_planner_destroy(claw_planner_t *planner);

// Planning
claw_plan_t *claw_planner_create_plan(claw_planner_t *planner, const char *goal,
                                      const claw_observation_t **observations, size_t obs_count);
claw_plan_t *claw_planner_revise_plan(claw_planner_t *planner, claw_plan_t *plan,
                                      const char *feedback);

// Plan execution
int claw_planner_execute_step(claw_planner_t *planner, claw_plan_t *plan,
                              claw_executor_t *executor);
int claw_planner_execute_plan(claw_planner_t *planner, claw_plan_t *plan,
                              claw_executor_t *executor);

// reflection.h - Reflection engine
typedef struct claw_reflector claw_reflector_t;

claw_reflector_t *claw_reflector_create(claw_inference_ctx_t *inference);
void claw_reflector_destroy(claw_reflector_t *reflector);

// Reflection
char *claw_reflector_reflect_on_plan(claw_reflector_t *reflector, const claw_plan_t *plan);
char *claw_reflector_reflect_on_action(claw_reflector_t *reflector, const claw_plan_step_t *step);
float claw_reflector_assess_progress(claw_reflector_t *reflector, const claw_plan_t *plan);
bool claw_reflector_should_continue(claw_reflector_t *reflector, const claw_plan_t *plan);
```

#### Testing Strategy
- Agent loop integration tests
- Planning accuracy tests
- Reflection quality tests
- End-to-end task completion tests

#### Milestone Criteria
- [ ] Agent completes simple tasks autonomously
- [ ] Planning produces valid step sequences
- [ ] Reflection improves task completion
- [ ] Loop handles errors gracefully

---

### Week 14: Multi-Model Routing

#### Files to Create
| File | Description | Lines (est.) |
|------|-------------|--------------|
| `src/router/model_router.c/h` | Multi-model router | 450 |
| `src/router/provider_local.c/h` | Local provider | 300 |
| `src/router/provider_ollama.c/h` | Ollama provider | 250 |
| `src/router/provider_openai.c/h` | OpenAI provider | 300 |
| `include/openclaw/router.h` | Public router API | 200 |

#### Key Functions to Implement
```c
// router.h - Multi-model routing API
typedef struct claw_model_router claw_model_router_t;
typedef struct claw_model_config claw_model_config_t;

typedef enum {
    MODEL_CAP_CHAT = 1 << 0,
    MODEL_CAP_TOOLS = 1 << 4,
    MODEL_CAP_JSON = 1 << 5,
    MODEL_CAP_STREAM = 1 << 6,
    MODEL_CAP_REASONING = 1 << 7,
    MODEL_CAP_CODE = 1 << 8,
    MODEL_CAP_LONG_CTX = 1 << 9,
    MODEL_CAP_VISION = 1 << 10
} claw_model_capability_t;

typedef enum {
    PROVIDER_LOCAL, PROVIDER_OLLAMA, PROVIDER_OPENAI,
    PROVIDER_ANTHROPIC, PROVIDER_RUNPOD, PROVIDER_KIMI
} claw_provider_type_t;

typedef enum {
    TASK_CHAT, TASK_REASONING, TASK_CODE, TASK_SUMMARIZE,
    TASK_EXTRACT, TASK_CLASSIFY, TASK_EMBED, TASK_VISION,
    TASK_TOOL_USE, TASK_AGENT_LOOP
} claw_task_type_t;

typedef enum {
    ROUTE_QUALITY, ROUTE_SPEED, ROUTE_COST, ROUTE_BALANCED,
    ROUTE_FALLBACK, ROUTE_PARALLEL, ROUTE_CASCADE
} claw_routing_strategy_t;

struct claw_model_config {
    char name[64];
    char display_name[64];
    claw_provider_type_t provider;
    char endpoint_url[512];
    char api_key_ref[64];
    uint32_t capabilities;
    uint32_t context_window;
    uint32_t max_output_tokens;
    float cost_per_1k_input;
    float cost_per_1k_output;
    uint32_t typical_latency_ms;
    float quality_score;
    float speed_score;
    float cost_score;
    float reliability_score;
    bool is_enabled;
    bool is_local;
    uint32_t priority;
};

typedef struct {
    claw_task_type_t task;
    uint32_t estimated_tokens;
    bool requires_tools;
    bool requires_vision;
    bool requires_json;
    float budget_usd;
    uint32_t timeout_ms;
} claw_request_context_t;

typedef struct {
    bool success;
    char *content;
    size_t content_len;
    uint32_t tokens_input;
    uint32_t tokens_output;
    float cost_usd;
    uint32_t latency_ms;
    char model_used[64];
    char *error_message;
} claw_model_response_t;

// Router lifecycle
claw_model_router_t *claw_model_router_create(void);
void claw_model_router_destroy(claw_model_router_t *router);

// Model registration
int claw_model_router_register(claw_model_router_t *router, const claw_model_config_t *config);
int claw_model_router_unregister(claw_model_router_t *router, const char *name);
claw_model_config_t *claw_model_router_get(claw_model_router_t *router, const char *name);

// Routing
claw_model_config_t *claw_model_router_select(claw_model_router_t *router,
                                               const claw_request_context_t *ctx,
                                               claw_routing_strategy_t strategy);
int claw_model_router_route(claw_model_router_t *router,
                            const claw_request_context_t *ctx,
                            const char *prompt, size_t len,
                            claw_model_response_t *response);

// Fallback
int claw_model_router_set_fallback_chain(claw_model_router_t *router,
                                          const char **model_names, size_t count);
int claw_model_router_execute_with_fallback(claw_model_router_t *router,
                                             const claw_request_context_t *ctx,
                                             const char *prompt, size_t len,
                                             claw_model_response_t *response);

// Health checks
int claw_model_router_health_check(claw_model_router_t *router, const char *model_name);
int claw_model_router_health_check_all(claw_model_router_t *router);
```

#### Dependencies to Integrate
- libcurl (for HTTP providers)
- jansson (for JSON parsing)

#### Testing Strategy
- Routing algorithm tests
- Provider integration tests
- Fallback chain tests
- Health check tests

#### Milestone Criteria
- [ ] Router selects appropriate model for task
- [ ] Fallback works on provider failure
- [ ] Cost tracking accurate
- [ ] Health checks detect failures

---

## Phase 5: TUI & Channels (Weeks 15-17)

**Goal:** Implement the ncurses-based terminal UI and channel adapters for messaging platforms.

### Week 15: ncurses TUI

#### Files to Create
| File | Description | Lines (est.) |
|------|-------------|--------------|
| `src/tui/tui_main.c/h` | Main TUI | 400 |
| `src/tui/tui_chat.c/h` | Chat window | 350 |
| `src/tui/tui_dashboard.c/h` | Dashboard panel | 300 |
| `src/tui/tui_input.c/h` | Input handling | 250 |
| `include/openclaw/tui.h` | Public TUI API | 150 |

#### Key Functions to Implement
```c
// tui.h - Terminal UI API
typedef struct claw_tui claw_tui_t;

typedef enum {
    TUI_MODE_CHAT,
    TUI_MODE_DASHBOARD,
    TUI_MODE_TOOLS,
    TUI_MODE_MEMORY,
    TUI_MODE_LOGS,
    TUI_MODE_CONFIG
} claw_tui_mode_t;

// TUI lifecycle
claw_tui_t *claw_tui_create(claw_agent_t *agent);
void claw_tui_destroy(claw_tui_t *tui);
int claw_tui_run(claw_tui_t *tui);
void claw_tui_stop(claw_tui_t *tui);

// Mode switching
void claw_tui_set_mode(claw_tui_t *tui, claw_tui_mode_t mode);
claw_tui_mode_t claw_tui_get_mode(claw_tui_t *tui);

// Chat interface
int claw_tui_chat_add_message(claw_tui_t *tui, claw_message_role_t role, const char *content);
int claw_tui_chat_add_tool_call(claw_tui_t *tui, const char *tool_name, const char *args);
int claw_tui_chat_add_tool_result(claw_tui_t *tui, const char *tool_name, const char *result);
void claw_tui_chat_clear(claw_tui_t *tui);

// Dashboard
void claw_tui_dashboard_update_agent_status(claw_tui_t *tui, const char *agent_name,
                                            claw_agent_state_t state);
void claw_tui_dashboard_update_model_info(claw_tui_t *tui, const char *model_name,
                                          uint32_t tokens_in, uint32_t tokens_out);
void claw_tui_dashboard_update_stats(claw_tui_t *tui, float tokens_per_sec,
                                     float cost_per_hour);

// Input
char *claw_tui_get_input(claw_tui_t *tui);
void claw_tui_set_input_prompt(claw_tui_t *tui, const char *prompt);

// tui_chat.h - Chat window implementation
typedef struct claw_tui_chat claw_tui_chat_t;

claw_tui_chat_t *claw_tui_chat_create(WINDOW *parent_win);
void claw_tui_chat_destroy(claw_tui_chat_t *chat);

void claw_tui_chat_render(claw_tui_chat_t *chat);
void claw_tui_chat_scroll(claw_tui_chat_t *chat, int lines);
void claw_tui_chat_page_up(claw_tui_chat_t *chat);
void claw_tui_chat_page_down(claw_tui_chat_t *chat);

// tui_dashboard.h - Dashboard panel
typedef struct claw_tui_dashboard claw_tui_dashboard_t;

claw_tui_dashboard_t *claw_tui_dashboard_create(WINDOW *parent_win);
void claw_tui_dashboard_destroy(claw_tui_dashboard_t *dashboard);

void claw_tui_dashboard_render(claw_tui_dashboard_t *dashboard);
void claw_tui_dashboard_set_agent_list(claw_tui_dashboard_t *dashboard,
                                        const char **names, claw_agent_state_t *states, size_t count);
void claw_tui_dashboard_set_active_tools(claw_tui_dashboard_t *dashboard,
                                          const char **tools, float *durations, size_t count);
```

#### Dependencies to Integrate
- ncursesw (wide character support)
- libpthread (for input handling)

#### Testing Strategy
- UI rendering tests
- Input handling tests
- Resize handling tests
- Color/theme tests

#### Milestone Criteria
- [ ] TUI renders without artifacts
- [ ] Chat history scrollable
- [ ] Dashboard shows real-time stats
- [ ] Input handling responsive

---

### Week 16: Channel Adapters

#### Files to Create
| File | Description | Lines (est.) |
|------|-------------|--------------|
| `src/channel/channel_registry.c/h` | Channel registry | 300 |
| `src/channel/channel_terminal.c/h` | Terminal channel | 200 |
| `src/channel/channel_telegram.c/h` | Telegram adapter | 400 |
| `src/channel/channel_slack.c/h` | Slack adapter | 350 |
| `include/openclaw/channel.h` | Public channel API | 200 |

#### Key Functions to Implement
```c
// channel.h - Channel adapter API
typedef struct claw_channel claw_channel_t;
typedef struct claw_channel_config claw_channel_config_t;

typedef enum {
    CHANNEL_TERMINAL,
    CHANNEL_TELEGRAM,
    CHANNEL_SLACK,
    CHANNEL_DISCORD,
    CHANNEL_WHATSAPP,
    CHANNEL_SIGNAL,
    CHANNEL_WEBSOCKET
} claw_channel_type_t;

typedef enum {
    MSG_TYPE_TEXT,
    MSG_TYPE_IMAGE,
    MSG_TYPE_FILE,
    MSG_TYPE_AUDIO,
    MSG_TYPE_VIDEO
} claw_message_type_t;

typedef struct {
    char id[128];
    char sender_id[128];
    char sender_name[128];
    char chat_id[128];
    char chat_name[128];
    claw_message_type_t type;
    char *content;
    size_t content_len;
    uint64_t timestamp;
} claw_channel_message_t;

struct claw_channel_config {
    claw_channel_type_t type;
    char name[64];
    char api_key[256];
    char webhook_url[512];
    char bot_token[256];
    uint8_t enabled;
    uint8_t auto_reply;
    uint8_t require_mention;
};

// Channel lifecycle
claw_channel_t *claw_channel_create(const claw_channel_config_t *config);
void claw_channel_destroy(claw_channel_t *channel);

// Connection
int claw_channel_connect(claw_channel_t *channel);
int claw_channel_disconnect(claw_channel_t *channel);
bool claw_channel_is_connected(claw_channel_t *channel);

// Messaging
int claw_channel_send_message(claw_channel_t *channel, const char *chat_id,
                              const char *content);
int claw_channel_send_reply(claw_channel_t *channel, const char *message_id,
                            const char *content);

// Event handling
void claw_channel_set_message_handler(claw_channel_t *channel,
                                       void (*handler)(claw_channel_message_t *msg, void *userdata),
                                       void *userdata);

// channel_registry.h - Channel registry
typedef struct claw_channel_registry claw_channel_registry_t;

claw_channel_registry_t *claw_channel_registry_create(void);
void claw_channel_registry_destroy(claw_channel_registry_t *registry);

int claw_channel_registry_register(claw_channel_registry_t *registry, claw_channel_t *channel);
int claw_channel_registry_unregister(claw_channel_registry_t *registry, const char *name);
claw_channel_t *claw_channel_registry_get(claw_channel_registry_t *registry, const char *name);

// Broadcast
int claw_channel_registry_broadcast(claw_channel_registry_t *registry, const char *content);
int claw_channel_registry_send_to(claw_channel_registry_t *registry, const char *channel_name,
                                   const char *chat_id, const char *content);
```

#### Dependencies to Integrate
- libcurl (for HTTP-based channels)
- libwebsockets (for WebSocket channels)
- jansson (for JSON APIs)

#### Testing Strategy
- Channel connection tests
- Message sending/receiving tests
- Error handling tests
- Rate limiting tests

#### Milestone Criteria
- [ ] Terminal channel works
- [ ] Telegram bot responds to messages
- [ ] Slack integration functional
- [ ] Message routing correct

---

### Week 17: Event Loop Integration & IPC

#### Files to Create
| File | Description | Lines (est.) |
|------|-------------|--------------|
| `src/ipc/ipc_server.c/h` | IPC server | 400 |
| `src/ipc/ipc_client.c/h` | IPC client | 350 |
| `src/ipc/ipc_protocol.c/h` | Protocol implementation | 300 |
| `include/openclaw/ipc.h` | Public IPC API | 200 |

#### Key Functions to Implement
```c
// ipc.h - Inter-process communication API
typedef struct claw_ipc_server claw_ipc_server_t;
typedef struct claw_ipc_client claw_ipc_client_t;
typedef struct claw_ipc_conn claw_ipc_conn_t;

typedef enum {
    IPC_MSG_HELLO,
    IPC_MSG_REQUEST,
    IPC_MSG_RESPONSE,
    IPC_MSG_STREAM_START,
    IPC_MSG_STREAM_DATA,
    IPC_MSG_STREAM_END,
    IPC_MSG_ERROR,
    IPC_MSG_HEARTBEAT,
    IPC_MSG_DISCONNECT
} claw_ipc_msg_type_t;

typedef struct {
    uint32_t magic;
    uint32_t version;
    claw_ipc_msg_type_t type;
    uint32_t seq_num;
    uint32_t payload_len;
    uint64_t shm_offset;
    char request_id[64];
} claw_ipc_header_t;

typedef struct {
    char method[32];
    char *payload;
    size_t payload_len;
    uint32_t timeout_ms;
    uint8_t streaming;
} claw_ipc_request_t;

typedef struct {
    int status;
    char *data;
    size_t data_len;
    char error_msg[256];
} claw_ipc_response_t;

// Server
claw_ipc_server_t *claw_ipc_server_create(const char *socket_path);
void claw_ipc_server_destroy(claw_ipc_server_t *server);
int claw_ipc_server_start(claw_ipc_server_t *server);
void claw_ipc_server_stop(claw_ipc_server_t *server);

void claw_ipc_server_set_connect_handler(claw_ipc_server_t *server,
                                          void (*handler)(claw_ipc_conn_t *conn));
void claw_ipc_server_set_message_handler(claw_ipc_server_t *server,
                                          void (*handler)(claw_ipc_conn_t *conn,
                                                         claw_ipc_header_t *msg, void *payload));
void claw_ipc_server_set_disconnect_handler(claw_ipc_server_t *server,
                                             void (*handler)(claw_ipc_conn_t *conn));

// Client
claw_ipc_client_t *claw_ipc_client_create(const char *socket_path);
void claw_ipc_client_destroy(claw_ipc_client_t *client);
int claw_ipc_client_connect(claw_ipc_client_t *client);
void claw_ipc_client_disconnect(claw_ipc_client_t *client);

int claw_ipc_client_send_request(claw_ipc_client_t *client, const claw_ipc_request_t *req,
                                  claw_ipc_response_t *resp);
int claw_ipc_client_send_request_async(claw_ipc_client_t *client, const claw_ipc_request_t *req,
                                        void (*callback)(claw_ipc_response_t *resp, void *userdata),
                                        void *userdata);

// Shared memory
claw_shm_region_t *claw_shm_create(const char *name, size_t size);
void claw_shm_destroy(claw_shm_region_t *shm);
void *claw_shm_get_ptr(claw_shm_region_t *shm);
```

#### Dependencies to Integrate
- Unix domain sockets
- POSIX shared memory
- eventfd / pipe

#### Testing Strategy
- IPC connection tests
- Message round-trip tests
- Shared memory tests
- Concurrent client tests

#### Milestone Criteria
- [ ] IPC server accepts connections
- [ ] Client can send/receive messages
- [ ] Shared memory transfer works
- [ ] 1000+ concurrent clients handled

---

## Phase 6: Security & Hardening (Weeks 18-20)

**Goal:** Implement secure credential storage, sandboxing, audit logging, and comprehensive testing.

### Week 18: Credential Storage & Encryption

#### Files to Create
| File | Description | Lines (est.) |
|------|-------------|--------------|
| `src/security/keyring.c/h` | Keyring core | 350 |
| `src/security/keyring_file.c` | File-based backend | 400 |
| `src/security/keyring_libsecret.c` | libsecret backend | 250 |
| `include/openclaw/security.h` | Public security API | 200 |

#### Key Functions to Implement
```c
// security.h - Security API
typedef struct claw_keyring claw_keyring_t;

typedef enum {
    KEY_TYPE_API, KEY_TYPE_TOKEN, KEY_TYPE_PASSWORD,
    KEY_TYPE_CERT, KEY_TYPE_SSH, KEY_TYPE_CUSTOM
} claw_key_type_t;

typedef enum {
    SEC_LEVEL_MEMORY, SEC_LEVEL_USER, SEC_LEVEL_SYSTEM, SEC_LEVEL_HARDWARE
} claw_security_level_t;

typedef enum {
    KEYRING_BACKEND_LIBSECRET, KEYRING_BACKEND_KEYCHAIN,
    KEYRING_BACKEND_CREDENTIAL, KEYRING_BACKEND_FILE,
    KEYRING_BACKEND_TPM, KEYRING_BACKEND_MEMORY
} claw_keyring_backend_t;

typedef struct {
    char name[128];
    char service[64];
    claw_key_type_t type;
    claw_security_level_t level;
    time_t created_at;
    time_t expires_at;
    bool auto_rotate;
    uint32_t rotation_days;
} claw_key_attributes_t;

// Keyring lifecycle
claw_keyring_t *claw_keyring_create(claw_keyring_backend_t backend, const char *app_name);
void claw_keyring_destroy(claw_keyring_t *keyring);

// Key operations
int claw_keyring_set(claw_keyring_t *keyring, const char *name, const char *service,
                     const uint8_t *value, size_t len, const claw_key_attributes_t *attrs);
int claw_keyring_get(claw_keyring_t *keyring, const char *name, const char *service,
                     uint8_t **value, size_t *len, claw_key_attributes_t *attrs);
int claw_keyring_delete(claw_keyring_t *keyring, const char *name, const char *service);
bool claw_keyring_has(claw_keyring_t *keyring, const char *name, const char *service);

// Security utilities
void claw_secure_zero(void *ptr, size_t len);
int claw_secure_compare(const void *a, const void *b, size_t len);
char *claw_secure_random_bytes(size_t len);

// File-based encryption (keyring_file.c)
#define KEYRING_FILE_MAGIC "OCKR"
#define KDF_ITERATIONS 100000
#define AES_KEY_SIZE 32
#define AES_IV_SIZE 12
#define AES_TAG_SIZE 16

typedef struct {
    char magic[4];
    uint32_t version;
    uint32_t entry_count;
    uint8_t salt[32];
} claw_keyring_file_header_t;

int claw_keyring_file_create(const char *path, const char *password);
int claw_keyring_file_unlock(claw_keyring_t *keyring, const char *password);
int claw_keyring_file_lock(claw_keyring_t *keyring);
```

#### Dependencies to Integrate
- OpenSSL (for encryption)
- libsecret (for system keyring)
- Keychain (macOS)
- Credential Manager (Windows)

#### Testing Strategy
- Encryption/decryption tests
- Key persistence tests
- Backend compatibility tests
- Security audit tests

#### Milestone Criteria
- [ ] Credentials encrypted at rest
- [ ] libsecret integration works
- [ ] File backend passes security audit
- [ ] No plaintext credential exposure

---

### Week 19: Sandboxing & Audit Logging

#### Files to Create
| File | Description | Lines (est.) |
|------|-------------|--------------|
| `src/security/sandbox.c/h` | Sandbox implementation | 500 |
| `src/security/audit.c/h` | Audit logging | 350 |
| `src/tool/tool_sandbox.c` | Tool sandbox (enhanced) | 300 |

#### Key Functions to Implement
```c
// sandbox.h - Sandboxing API (enhanced)
typedef struct claw_sandbox_config claw_sandbox_config_t;

struct claw_sandbox_config {
    // Filesystem
    char **allowed_read_paths;
    size_t allowed_read_count;
    char **allowed_write_paths;
    size_t allowed_write_count;
    
    // Network
    char **allowed_hosts;
    size_t allowed_host_count;
    uint16_t *allowed_ports;
    size_t allowed_port_count;
    
    // Resources
    uint64_t max_memory_bytes;
    uint32_t max_cpu_percent;
    uint64_t max_file_size_bytes;
    uint32_t max_open_files;
    
    // Time
    uint32_t timeout_ms;
    uint32_t wall_time_ms;
    
    // Security flags
    uint32_t flags;  // CLAW_SANDBOX_*
};

// Enhanced sandbox
claw_sandbox_t *claw_sandbox_create_enhanced(const claw_sandbox_config_t *config);
int claw_sandbox_execute_command(claw_sandbox_t *sandbox, const char *command,
                                  char **stdout, char **stderr, int *exit_code);
int claw_sandbox_execute_function(claw_sandbox_t *sandbox, claw_tool_func_t func,
                                   void *arg, void **result);

// Resource monitoring
uint64_t claw_sandbox_get_memory_usage(claw_sandbox_t *sandbox);
uint32_t claw_sandbox_get_cpu_usage(claw_sandbox_t *sandbox);
uint64_t claw_sandbox_get_elapsed_ms(claw_sandbox_t *sandbox);

// audit.h - Audit logging
typedef struct claw_audit_log claw_audit_log_t;

typedef enum {
    AUDIT_EVENT_AUTH, AUDIT_EVENT_TOOL_CALL, AUDIT_EVENT_MODEL_CALL,
    AUDIT_EVENT_CONFIG_CHANGE, AUDIT_EVENT_FILE_ACCESS,
    AUDIT_EVENT_NETWORK_ACCESS, AUDIT_EVENT_SECURITY_VIOLATION
} claw_audit_event_type_t;

claw_audit_log_t *claw_audit_create(const char *path);
void claw_audit_destroy(claw_audit_log_t *log);

int claw_audit_log(claw_audit_log_t *log, claw_audit_event_type_t type,
                   const char *actor, const char *action, const char *details);
int claw_audit_log_tool_call(claw_audit_log_t *log, const char *agent_id,
                              const char *tool_name, const char *params,
                              const char *result, uint64_t latency_us);
int claw_audit_log_model_call(claw_audit_log_t *log, const char *agent_id,
                               const char *model_name, uint32_t tokens_in,
                               uint32_t tokens_out, float cost_usd);

// Query
char **claw_audit_query(claw_audit_log_t *log, const char *filter, time_t start, time_t end,
                        size_t *count);

// Compliance
int claw_audit_export(claw_audit_log_t *log, const char *path, const char *format);
```

#### Dependencies to Integrate
- Linux namespaces (clone, unshare, setns)
- seccomp-bpf
- Landlock LSM
- cgroups (for resource limits)

#### Testing Strategy
- Sandbox escape tests
- Resource limit tests
- Audit log integrity tests
- Compliance validation tests

#### Milestone Criteria
- [ ] Sandbox prevents all escape attempts
- [ ] Resource limits enforced
- [ ] Audit log tamper-evident
- [ ] Compliance reports generated

---

### Week 20: Fuzz Testing & Hardening

#### Files to Create
| File | Description | Lines (est.) |
|------|-------------|--------------|
| `tests/fuzz/fuzz_tool_parser.c` | Tool parser fuzzer | 200 |
| `tests/fuzz/fuzz_grammar.c` | Grammar fuzzer | 200 |
| `tests/fuzz/fuzz_model_loader.c` | Model loader fuzzer | 200 |
| `tests/fuzz/fuzz_ipc.c` | IPC protocol fuzzer | 200 |
| `scripts/security_audit.sh` | Security audit script | 150 |

#### Key Functions to Implement
```c
// Fuzz targets
int LLVMFuzzerTestOneInput_tool_parser(const uint8_t *data, size_t size);
int LLVMFuzzerTestOneInput_grammar(const uint8_t *data, size_t size);
int LLVMFuzzerTestOneInput_model_loader(const uint8_t *data, size_t size);
int LLVMFuzzerTestOneInput_ipc(const uint8_t *data, size_t size);
```

#### Testing Strategy
- LibFuzzer integration
- AFL++ integration
- Corpus generation
- Crash analysis

#### Security Hardening Checklist
- [ ] AddressSanitizer enabled
- [ ] UndefinedBehaviorSanitizer enabled
- [ ] Stack canaries enabled
- [ ] ASLR compatible
- [ ] FORTIFY_SOURCE enabled
- [ ] RELRO and BIND_NOW
- [ ] No executable stack
- [ ] No RWX segments

#### Milestone Criteria
- [ ] Fuzz tests run continuously
- [ ] No crashes after 24-hour fuzzing
- [ ] Security audit passes
- [ ] Static analysis clean (Coverity, CodeQL)
- [ ] Penetration test completed

---

## Dependencies & Libraries

### Core Dependencies

| Library | Version | Purpose | Integration |
|---------|---------|---------|-------------|
| **llama.cpp** | latest | Local LLM inference | Git submodule |
| **ggml** | bundled | Tensor operations | With llama.cpp |
| **ncursesw** | 6.4+ | Terminal UI | System package |
| **libcurl** | 8.0+ | HTTP client | System package |
| **jansson** | 2.14+ | JSON parsing | System package |
| **pcre2** | 10.42+ | Regex/grammar | System package |
| **openssl** | 3.0+ | TLS/crypto | System package |
| **sqlite3** | 3.40+ | Local storage | System package |
| **libuv** | 1.45+ | Cross-platform async | System package |

### Optional Dependencies

| Library | Purpose | When to Use |
|---------|---------|-------------|
| **CUDA** | GPU acceleration | NVIDIA GPU available |
| **Vulkan** | Cross-platform GPU | AMD/Intel GPU |
| **Metal** | Apple GPU | macOS deployment |
| **libsecret** | System keyring | Linux desktop |
| **sentencepiece** | Tokenization | Custom tokenizers |

### Build Dependencies

| Tool | Version | Purpose |
|------|---------|---------|
| CMake | 3.20+ | Build system |
| GCC/Clang | 12+ / 14+ | Compiler |
| pkg-config | 0.29+ | Dependency detection |
| Python | 3.8+ | Build scripts |

---

## Testing Strategy

### Unit Tests
- Every module has corresponding test file
- Use Unity or custom test framework
- Aim for > 80% code coverage

### Integration Tests
- Test component interactions
- End-to-end workflows
- Provider integration tests

### Fuzz Tests
- LibFuzzer for continuous fuzzing
- AFL++ for deep fuzzing
- Custom corpus for each target

### Performance Tests
- Benchmark suite for key operations
- Memory usage profiling
- Latency measurements

### Security Tests
- Static analysis (Coverity, CodeQL)
- Dynamic analysis (Valgrind, ASan)
- Penetration testing

---

## Risk Mitigation

| Risk | Impact | Mitigation |
|------|--------|------------|
| llama.cpp API changes | High | Pin to stable version, abstraction layer |
| Memory leaks | High | Valgrind, ASan, custom allocators |
| Security vulnerabilities | Critical | Fuzzing, audits, sandboxing |
| Performance issues | Medium | Profiling, caching, optimization |
| Compatibility issues | Medium | CI testing on multiple platforms |
| Scope creep | Medium | Strict milestone criteria |

---

## Summary

This 20-week implementation roadmap provides a structured path to building OpenClaw-C:

1. **Foundation (Weeks 1-4):** Core infrastructure, memory management, event loop
2. **Inference Engine (Weeks 5-8):** Model loading, llama.cpp integration, grammar constraints
3. **Tool System (Weeks 9-11):** Tool registry, parser, sandboxed execution
4. **Agent Loop (Weeks 12-14):** State machine, planning, multi-model routing
5. **TUI & Channels (Weeks 15-17):** ncurses interface, channel adapters, IPC
6. **Security & Hardening (Weeks 18-20):** Credential storage, sandboxing, fuzz testing

### Success Criteria

- [ ] 7B Qwen model runs locally with < 2GB RAM
- [ ] Grammar-constrained tool calling works
- [ ] Agent completes multi-step tasks autonomously
- [ ] TUI provides intuitive interface
- [ ] Security audit passes with no critical issues
- [ ] Performance exceeds TypeScript baseline

---

*Document Version: 1.0.0*  
*Last Updated: February 2026*  
*Author: OpenClaw-C Architecture Team*
