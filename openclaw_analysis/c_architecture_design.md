# OpenClaw C Reimplementation Architecture Design

## Executive Summary

This document presents a comprehensive C-language architecture for reimplementing OpenClaw as a high-performance, memory-efficient personal AI assistant. The design prioritizes:

1. **mmap-based model loading** - No full RAM resident models
2. **Indexed file system** for O(1) weight lookups
3. **Memory-mapped tool registry** for fast discovery
4. **Grammar-constrained generation** via formal grammars
5. **Native 7B Qwen inference** in pure C
6. **Terminal UI** (ncurses-based)
7. **Event-driven architecture** with epoll/kqueue

---

## 1. High-Level System Architecture

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                           OPENCLAW C ARCHITECTURE                               │
├─────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│  ┌─────────────────────────────────────────────────────────────────────────┐   │
│  │                         TUI LAYER (ncurses)                              │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌─────────────┐  │   │
│  │  │ Chat Window  │  │ Tool Panel   │  │ Status Bar   │  │ Input Box   │  │   │
│  │  └──────────────┘  └──────────────┘  └──────────────┘  └─────────────┘  │   │
│  └───────────────────────────────┬─────────────────────────────────────────┘   │
│                                  │                                              │
│  ┌───────────────────────────────▼─────────────────────────────────────────┐   │
│  │                      EVENT LOOP (epoll/kqueue)                           │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌─────────────┐  │   │
│  │  │ Timer Events │  │ I/O Events   │  │ IPC Events   │  │ Signal Hnd  │  │   │
│  │  └──────────────┘  └──────────────┘  └──────────────┘  └─────────────┘  │   │
│  └───────────────────────────────┬─────────────────────────────────────────┘   │
│                                  │                                              │
│  ┌───────────────────────────────▼─────────────────────────────────────────┐   │
│  │                      AGENT ORCHESTRATOR                                  │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌─────────────┐  │   │
│  │  │ State Machine│  │ Context Mgr  │  │ Tool Router  │  │ Mem System  │  │   │
│  │  └──────────────┘  └──────────────┘  └──────────────┘  └─────────────┘  │   │
│  └───────────────────────────────┬─────────────────────────────────────────┘   │
│                                  │                                              │
│  ┌───────────────────────────────▼─────────────────────────────────────────┐   │
│  │                      INFERENCE ENGINE (llama.cpp/ggml)                   │   │
│  │  ┌─────────────────────────────────────────────────────────────────┐   │   │
│  │  │                    MODEL INDEX (mmap-based)                      │   │   │
│  │  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌────────┐ │   │   │
│  │  │  │Attention │ │   FFN    │ │Embedding│ │  Norm    │ │ Output │ │   │   │
│  │  │  │  Blocks  │ │  Blocks  │ │  Table  │ │ Layers   │ │  Head  │ │   │   │
│  │  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘ └────────┘ │   │   │
│  │  │                                                                  │   │   │
│  │  │  ┌────────────────────────────────────────────────────────────┐ │   │   │
│  │  │  │              GRAMMAR CONSTRAINT ENGINE                      │ │   │   │
│  │  │  │  (BNF/JSON Schema → Grammar Automaton → Constrained Decode) │ │   │   │
│  │  │  └────────────────────────────────────────────────────────────┘ │   │   │
│  │  └─────────────────────────────────────────────────────────────────┘   │   │
│  └───────────────────────────────┬─────────────────────────────────────────┘   │
│                                  │                                              │
│  ┌───────────────────────────────▼─────────────────────────────────────────┐   │
│  │                      TOOL REGISTRY (mmap hash table)                     │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌─────────────┐  │   │
│  │  │ Built-in     │  │ Plugin       │  │ External     │  │ Sandbox     │  │   │
│  │  │ Tools        │  │ Tools        │  │ APIs         │  │ Exec        │  │   │
│  │  └──────────────┘  └──────────────┘  └──────────────┘  └─────────────┘  │   │
│  └───────────────────────────────┬─────────────────────────────────────────┘   │
│                                  │                                              │
│  ┌───────────────────────────────▼─────────────────────────────────────────┐   │
│  │                      CHANNEL ADAPTERS                                    │   │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────────┐  │   │
│  │  │ Telegram │ │ WhatsApp │ │  Slack   │ │ Discord  │ │   iMessage   │  │   │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘ └──────────────┘  │   │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────────┐  │   │
│  │  │  Signal  │ │   Line   │ │  Matrix  │ │ WebChat  │ │   Terminal   │  │   │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘ └──────────────┘  │   │
│  └─────────────────────────────────────────────────────────────────────────┘   │
│                                                                                 │
│  ┌─────────────────────────────────────────────────────────────────────────┐   │
│  │                      PLUGIN SYSTEM (External Models)                     │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌─────────────┐  │   │
│  │  │   Ollama     │  │   RunPod     │  │    Kimi      │  │  OpenAI     │  │   │
│  │  │  (local)     │  │  (remote)    │  │  (remote)    │  │  (remote)   │  │   │
│  │  └──────────────┘  └──────────────┘  └──────────────┘  └─────────────┘  │   │
│  └─────────────────────────────────────────────────────────────────────────┘   │
│                                                                                 │
└─────────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Core Data Structures

### 2.1 Model Weight Index (mmap-based)

```c
/* ============================================================================
 * MODEL INDEX HEADER - Stored at beginning of .clawmodel file
 * ============================================================================ */

#define CLAW_MODEL_MAGIC        0x434C4157  /* "CLAW" */
#define CLAW_MODEL_VERSION      1
#define CLAW_MAX_LAYERS         256
#define CLAW_MAX_TENSORS        65536
#define CLAW_TENSOR_NAME_LEN    128

typedef enum {
    CLAW_DTYPE_F32  = 0,
    CLAW_DTYPE_F16  = 1,
    CLAW_DTYPE_Q4_0 = 2,
    CLAW_DTYPE_Q4_1 = 3,
    CLAW_DTYPE_Q5_0 = 4,
    CLAW_DTYPE_Q5_1 = 5,
    CLAW_DTYPE_Q8_0 = 6,
    CLAW_DTYPE_Q8_1 = 7,
    CLAW_DTYPE_Q2_K = 8,
    CLAW_DTYPE_Q3_K = 9,
    CLAW_DTYPE_Q4_K = 10,
    CLAW_DTYPE_Q5_K = 11,
    CLAW_DTYPE_Q6_K = 12,
    CLAW_DTYPE_Q8_K = 13,
    CLAW_DTYPE_IQ4_NL = 14,
} claw_dtype_t;

typedef enum {
    CLAW_TENSOR_ATTN_Q    = 0x01,   /* Attention query weights */
    CLAW_TENSOR_ATTN_K    = 0x02,   /* Attention key weights */
    CLAW_TENSOR_ATTN_V    = 0x04,   /* Attention value weights */
    CLAW_TENSOR_ATTN_O    = 0x08,   /* Attention output weights */
    CLAW_TENSOR_FFN_UP    = 0x10,   /* FFN up-projection */
    CLAW_TENSOR_FFN_GATE  = 0x20,   /* FFN gate (SwiGLU) */
    CLAW_TENSOR_FFN_DOWN  = 0x40,   /* FFN down-projection */
    CLAW_TENSOR_EMBED     = 0x80,   /* Token embeddings */
    CLAW_TENSOR_NORM      = 0x100,  /* Layer normalization */
    CLAW_TENSOR_OUTPUT    = 0x200,  /* Output head */
} claw_tensor_type_t;

/* Tensor descriptor - fixed size for O(1) lookup */
typedef struct __attribute__((packed)) {
    uint32_t    name_hash;          /* FNV-1a hash of tensor name */
    uint64_t    file_offset;        /* Offset in model file */
    uint64_t    size_bytes;         /* Size of tensor data */
    uint32_t    dims[4];            /* Dimensions (up to 4D) */
    uint16_t    ndim;               /* Number of dimensions */
    uint16_t    dtype;              /* claw_dtype_t */
    uint16_t    tensor_type;        /* claw_tensor_type_t */
    uint16_t    layer_idx;          /* Layer index (0 for shared) */
    char        name[CLAW_TENSOR_NAME_LEN];  /* Null-terminated name */
} claw_tensor_desc_t;

/* Model architecture configuration */
typedef struct __attribute__((packed)) {
    uint32_t    vocab_size;
    uint32_t    hidden_size;
    uint32_t    intermediate_size;
    uint32_t    num_layers;
    uint32_t    num_heads;
    uint32_t    num_kv_heads;
    uint32_t    max_position_embeddings;
    float       rms_norm_eps;
    float       rope_theta;
    uint32_t    head_dim;
    uint32_t    sliding_window;     /* For sliding window attention */
    uint8_t     use_gqa;            /* Grouped query attention */
    uint8_t     use_sliding_window;
    uint8_t     reserved[6];
} claw_model_arch_t;

/* Model file header */
typedef struct __attribute__((packed)) {
    uint32_t            magic;              /* CLAW_MODEL_MAGIC */
    uint32_t            version;            /* CLAW_MODEL_VERSION */
    uint64_t            header_size;        /* Size of full header */
    uint64_t            tensor_data_offset; /* Where tensor data begins */
    uint64_t            tensor_count;       /* Number of tensors */
    claw_model_arch_t   arch;               /* Architecture config */
    char                model_name[64];     /* Human-readable name */
    char                quantization[16];   /* "Q4_K_M", "Q5_K_S", etc. */
    uint64_t            checksum;           /* CRC64 of tensor data */
    uint8_t             reserved[64];       /* Future expansion */
    /* tensor_desc_t descriptors[tensor_count] follows */
} claw_model_header_t;

/* ============================================================================
 * RUNTIME MODEL HANDLE - Memory-mapped access
 * ============================================================================ */

typedef struct {
    int                 fd;                 /* File descriptor */
    size_t              file_size;          /* Total file size */
    void               *mmap_base;          /* mmap() base address */
    size_t              mmap_size;          /* mmap() size */
    
    /* Parsed header info */
    claw_model_header_t *header;
    claw_tensor_desc_t  *tensor_table;      /* Array of descriptors */
    
    /* Fast lookup index - hash table for O(1) tensor access */
    struct claw_tensor_index *index;        /* See below */
    
    /* Reference counting for shared access */
    atomic_uint         ref_count;
    
    /* Cache of recently accessed tensors */
    struct claw_tensor_cache *cache;
    
    pthread_rwlock_t    lock;               /* For thread-safe access */
} claw_model_t;

/* Tensor hash table entry for O(1) lookup */
struct claw_tensor_index {
    uint32_t            bucket_count;
    struct claw_index_entry {
        uint32_t        hash;               /* Name hash */
        uint32_t        idx;                /* Index in tensor_table */
        struct claw_index_entry *next;      /* Chain for collisions */
    } **buckets;
};

/* Tensor cache entry - LRU eviction */
struct claw_tensor_cache {
    size_t              max_entries;
    size_t              entry_size;
    struct cache_entry {
        uint32_t        tensor_idx;
        void           *data;               /* Dequantized/loaded data */
        size_t          data_size;
        uint64_t        last_access;
        uint8_t         resident;           /* Currently in RAM */
        struct cache_entry *lru_prev;
        struct cache_entry *lru_next;
    } *entries;
    struct cache_entry *lru_head;
    struct cache_entry *lru_tail;
};
```

### 2.2 Tool Registry (Memory-Mapped Hash Table)

```c
/* ============================================================================
 * TOOL REGISTRY - Memory-mapped for fast discovery
 * ============================================================================ */

#define CLAW_TOOL_MAGIC         0x544F4F4C  /* "TOOL" */
#define CLAW_TOOL_VERSION       1
#define CLAW_MAX_TOOLS          4096
#define CLAW_TOOL_NAME_LEN      64
#define CLAW_TOOL_DESC_LEN      512
#define CLAW_TOOL_SCHEMA_LEN    4096
#define CLAW_TOOL_PATH_LEN      256

typedef enum {
    CLAW_TOOL_BUILTIN   = 0,    /* Compiled-in C function */
    CLAW_TOOL_PLUGIN    = 1,    /* Shared library (.so/.dll) */
    CLAW_TOOL_SCRIPT    = 2,    /* External script (Python, etc) */
    CLAW_TOOL_HTTP      = 3,    /* HTTP API endpoint */
    CLAW_TOOL_WEBSOCKET = 4,    /* WebSocket endpoint */
} claw_tool_type_t;

typedef enum {
    CLAW_TOOL_SAFE      = 0,    /* Read-only, no side effects */
    CLAW_TOOL_CAUTIOUS  = 1,    /* May have side effects */
    CLAW_TOOL_DANGEROUS = 2,    /* Can modify system state */
    CLAW_TOOL_SANDBOXED = 3,    /* Requires sandbox execution */
} claw_tool_safety_t;

/* Parameter schema for a tool (JSON Schema subset) */
typedef struct {
    char        name[64];
    char        type[32];       /* "string", "number", "boolean", etc. */
    char        description[256];
    uint8_t     required;
    uint8_t     array;          /* Is array type */
    uint8_t     nullable;
    uint8_t     reserved;
    /* Default value as JSON string */
    char        default_value[256];
} claw_tool_param_t;

/* Tool descriptor in registry */
typedef struct __attribute__((packed)) {
    uint32_t            magic;              /* CLAW_TOOL_MAGIC */
    uint32_t            version;
    
    /* Identification */
    char                name[CLAW_TOOL_NAME_LEN];
    char                namespace[64];      /* e.g., "filesystem", "web" */
    uint32_t            name_hash;          /* FNV-1a hash for lookup */
    uint32_t            ns_hash;
    
    /* Metadata */
    char                description[CLAW_TOOL_DESC_LEN];
    claw_tool_type_t    type;
    claw_tool_safety_t  safety_level;
    uint32_t            timeout_ms;         /* Default timeout */
    
    /* Input/Output schema */
    uint32_t            param_count;
    claw_tool_param_t   params[32];         /* Fixed max for simplicity */
    char                returns_schema[CLAW_TOOL_SCHEMA_LEN];
    
    /* Execution info */
    union {
        struct {
            char        symbol_name[128];   /* Function symbol */
            char        lib_path[CLAW_TOOL_PATH_LEN];
        } plugin;
        struct {
            char        interpreter[32];    /* "python3", "node", etc. */
            char        script_path[CLAW_TOOL_PATH_LEN];
        } script;
        struct {
            char        endpoint[256];
            char        method[16];         /* GET, POST, etc. */
            char        headers[1024];      /* Default headers */
        } http;
    } exec;
    
    /* Statistics (atomically updated) */
    atomic_uint64_t     call_count;
    atomic_uint64_t     total_latency_us;
    atomic_uint64_t     error_count;
    
    /* Flags */
    uint32_t            enabled : 1;
    uint32_t            streaming : 1;
    uint32_t            async_only : 1;
    uint32_t            reserved : 29;
    
    uint8_t             padding[64];        /* Alignment */
} claw_tool_desc_t;

/* Registry header */
typedef struct __attribute__((packed)) {
    uint32_t            magic;
    uint32_t            version;
    uint64_t            header_size;
    uint64_t            tool_count;
    uint64_t            max_tools;
    uint64_t            desc_size;          /* sizeof(claw_tool_desc_t) */
    uint64_t            last_modified;
    uint8_t             reserved[64];
} claw_tool_registry_header_t;

/* Runtime registry handle */
typedef struct {
    int                         fd;
    void                       *mmap_base;
    size_t                      mmap_size;
    claw_tool_registry_header_t *header;
    claw_tool_desc_t           *tools;      /* Array of descriptors */
    
    /* Hash index for O(1) lookup */
    struct claw_tool_index {
        uint32_t bucket_count;
        struct tool_index_entry {
            uint32_t hash;
            uint32_t idx;
            struct tool_index_entry *next;
        } **buckets;
    } *index;
    
    /* Change notification */
    int                         inotify_fd; /* For file change detection */
    
    pthread_rwlock_t            lock;
} claw_tool_registry_t;

/* Tool call request/response */
typedef struct {
    char                tool_name[CLAW_TOOL_NAME_LEN];
    char                namespace[64];
    char                call_id[64];        /* UUID for tracking */
    char               *params_json;        /* JSON-encoded parameters */
    size_t              params_len;
    uint32_t            timeout_ms;
    void               *user_data;          /* For async callbacks */
} claw_tool_call_t;

typedef struct {
    char                call_id[64];
    int                 status;             /* 0 = success, else error */
    char               *result_json;
    size_t              result_len;
    uint64_t            latency_us;
    char                error_msg[256];
} claw_tool_result_t;

/* Tool function signature */
typedef int (*claw_tool_func_t)(
    const claw_tool_call_t *call,
    claw_tool_result_t *result,
    void *context
);
```

### 2.3 Agent State Machine

```c
/* ============================================================================
 * AGENT STATE MACHINE - Hierarchical state management
 * ============================================================================ */

#define CLAW_MAX_AGENTS         256
#define CLAW_SESSION_ID_LEN     64
#define CLAW_AGENT_NAME_LEN     64
#define CLAW_MAX_CONTEXT_MSGS   32768
#define CLAW_MAX_TOOLS_PER_CALL 32

typedef enum {
    CLAW_AGENT_IDLE         = 0,
    CLAW_AGENT_THINKING     = 1,    /* Processing user input */
    CLAW_AGENT_CALLING_TOOL = 2,    /* Waiting for tool execution */
    CLAW_AGENT_STREAMING    = 3,    /* Streaming response */
    CLAW_AGENT_COMPACTING   = 4,    /* Compressing context */
    CLAW_AGENT_PAUSED       = 5,    /* Paused by user */
    CLAW_AGENT_ERROR        = 6,    /* Error state */
    CLAW_AGENT_SHUTDOWN     = 7,    /* Shutting down */
} claw_agent_state_t;

typedef enum {
    CLAW_THINK_OFF      = 0,
    CLAW_THINK_MINIMAL  = 1,
    CLAW_THINK_LOW      = 2,
    CLAW_THINK_MEDIUM   = 3,
    CLAW_THINK_HIGH     = 4,
    CLAW_THINK_XHIGH    = 5,
} claw_thinking_level_t;

typedef enum {
    CLAW_ROLE_SYSTEM    = 0,
    CLAW_ROLE_USER      = 1,
    CLAW_ROLE_ASSISTANT = 2,
    CLAW_ROLE_TOOL      = 3,
} claw_message_role_t;

/* Message in conversation context */
typedef struct claw_message {
    uint64_t            id;
    claw_message_role_t role;
    uint64_t            timestamp_ms;
    char               *content;
    size_t              content_len;
    
    /* For tool calls */
    struct {
        char            tool_name[CLAW_TOOL_NAME_LEN];
        char            call_id[64];
        char           *arguments;
    } tool_call;
    
    /* For tool responses */
    struct {
        char            call_id[64];
        char           *output;
        int             status;
    } tool_result;
    
    /* Token counts (for context management) */
    uint32_t            token_count;
    uint32_t            reserved;
    
    /* Linked list for context window */
    struct claw_message *prev;
    struct claw_message *next;
} claw_message_t;

/* Context window management */
typedef struct {
    claw_message_t     *head;           /* Oldest message */
    claw_message_t     *tail;           /* Newest message */
    uint32_t            count;
    uint32_t            total_tokens;
    uint32_t            max_tokens;
    
    /* Summarization for long contexts */
    char               *summary;
    uint32_t            summary_tokens;
} claw_context_window_t;

/* Agent configuration */
typedef struct {
    char                name[CLAW_AGENT_NAME_LEN];
    char                session_id[CLAW_SESSION_ID_LEN];
    
    /* Model settings */
    char                model_id[64];       /* "qwen2.5-7b-q4_k_m" */
    claw_model_t       *model;
    
    /* Behavior settings */
    claw_thinking_level_t thinking_level;
    uint32_t            max_tokens;
    float               temperature;
    float               top_p;
    uint32_t            top_k;
    float               repetition_penalty;
    
    /* Tool settings */
    char                available_tools[CLAW_MAX_TOOLS_PER_CALL][CLAW_TOOL_NAME_LEN];
    uint32_t            tool_count;
    uint8_t             auto_tool_confirm;  /* Auto-approve safe tools */
    
    /* Context settings */
    uint32_t            context_window;     /* Max tokens in context */
    uint32_t            compact_threshold;  /* When to compact */
    
    /* Channel binding */
    char                channel_type[32];   /* "telegram", "terminal", etc. */
    char                channel_id[128];    /* Channel-specific ID */
    
    /* Safety */
    uint8_t             sandbox_mode;
    uint8_t             require_confirmation;
} claw_agent_config_t;

/* Agent instance */
typedef struct claw_agent {
    uint64_t            id;
    claw_agent_state_t  state;
    claw_agent_config_t config;
    
    /* Context */
    claw_context_window_t context;
    
    /* System prompts */
    char               *system_prompt;
    char               *soul_md;            /* Personality definition */
    char               *agents_md;          /* Agent instructions */
    char               *tools_md;           /* Tool descriptions */
    
    /* Grammar constraint (for structured output) */
    struct claw_grammar *output_grammar;
    
    /* Inference state */
    struct claw_inference_ctx *inference;
    
    /* Statistics */
    atomic_uint64_t     messages_received;
    atomic_uint64_t     messages_sent;
    atomic_uint64_t     tokens_generated;
    atomic_uint64_t     tool_calls;
    
    /* Synchronization */
    pthread_mutex_t     state_lock;
    pthread_cond_t      state_cond;
    
    /* Event loop integration */
    struct claw_event   *pending_event;
    
    /* Linked list for agent pool */
    struct claw_agent  *next;
    struct claw_agent  *prev;
} claw_agent_t;

/* Agent pool/manager */
typedef struct {
    claw_agent_t       *agents;
    uint32_t            count;
    uint32_t            max_agents;
    
    /* Fast lookup by session ID */
    struct agent_index {
        uint32_t bucket_count;
        struct agent_index_entry {
            char session_id[CLAW_SESSION_ID_LEN];
            claw_agent_t *agent;
            struct agent_index_entry *next;
        } **buckets;
    } *index;
    
    pthread_rwlock_t    lock;
} claw_agent_pool_t;

/* State transition table */
typedef struct {
    claw_agent_state_t  from_state;
    claw_agent_state_t  to_state;
    int (*validate)(claw_agent_t *agent, void *data);
    void (*on_enter)(claw_agent_t *agent, void *data);
    void (*on_exit)(claw_agent_t *agent, void *data);
} claw_state_transition_t;
```

### 2.4 Grammar Constraints

```c
/* ============================================================================
 * GRAMMAR CONSTRAINT ENGINE - Formal grammar for structured generation
 * ============================================================================ */

#define CLAW_GRAMMAR_MAX_RULES      1024
#define CLAW_GRAMMAR_MAX_SYMBOLS    4096
#define CLAW_GRAMMAR_MAX_ALTS       16
#define CLAW_SYMBOL_NAME_LEN        64

typedef enum {
    CLAW_SYM_TERMINAL,          /* Literal string */
    CLAW_SYM_NONTERM,           /* Non-terminal (rule reference) */
    CLAW_SYM_REGEX,             /* Regular expression */
    CLAW_SYM_CHAR_RANGE,        /* Character range [a-z] */
    CLAW_SYM_SEQUENCE,          /* Sequence of symbols */
    CLAW_SYM_CHOICE,            /* Alternative (A | B) */
    CLAW_SYM_OPTIONAL,          /* Optional (? ) */
    CLAW_SYM_STAR,              /* Zero or more (* ) */
    CLAW_SYM_PLUS,              /* One or more (+ ) */
} claw_symbol_type_t;

/* Grammar symbol (node in parse tree) */
typedef struct claw_symbol {
    claw_symbol_type_t  type;
    char                name[CLAW_SYMBOL_NAME_LEN];
    uint32_t            id;
    
    union {
        /* Terminal: literal string */
        struct {
            char       *value;
            size_t      len;
        } terminal;
        
        /* Character range */
        struct {
            uint32_t    start;      /* Unicode codepoint */
            uint32_t    end;
        } range;
        
        /* Regular expression */
        struct {
            char       *pattern;
            void       *regex;      /* Compiled regex (PCRE2) */
        } regex;
        
        /* Composite: sequence/choice */
        struct {
            struct claw_symbol **children;
            uint32_t            child_count;
        } composite;
        
        /* Quantified: optional/star/plus */
        struct {
            struct claw_symbol *child;
        } quantified;
        
        /* Reference to another rule */
        struct {
            uint32_t    rule_id;
        } ref;
    } data;
} claw_symbol_t;

/* Grammar rule: LHS -> RHS */
typedef struct {
    char                name[CLAW_SYMBOL_NAME_LEN];
    uint32_t            id;
    claw_symbol_t      *rhs;            /* Right-hand side */
    uint8_t             is_start;       /* Start symbol? */
    uint8_t             reserved[7];
} claw_grammar_rule_t;

/* Compiled grammar */
typedef struct {
    char                name[64];
    claw_grammar_rule_t *rules;
    uint32_t            rule_count;
    uint32_t            max_rules;
    
    /* Symbol table for fast lookup */
    struct grammar_symtab {
        uint32_t bucket_count;
        struct symtab_entry {
            char name[CLAW_SYMBOL_NAME_LEN];
            uint32_t rule_id;
            struct symtab_entry *next;
        } **buckets;
    } *symtab;
    
    /* LL(1) parsing table (optional optimization) */
    int                *ll1_table;
    
    /* First/Follow sets for validation */
    uint64_t           *first_sets;
    uint64_t           *follow_sets;
} claw_grammar_t;

/* Grammar constraint for inference */
typedef struct {
    claw_grammar_t     *grammar;
    
    /* Current parser state */
    struct {
        claw_symbol_t  *current_symbol;
        uint32_t        rule_stack[64];     /* Call stack */
        uint32_t        stack_depth;
    } parser_state;
    
    /* Valid next tokens mask (for vocabulary masking) */
    uint32_t           *valid_tokens;
    uint32_t            vocab_size;
    
    /* Partial parse tree */
    struct parse_node  *parse_tree;
} claw_grammar_constraint_t;

/* Predefined grammar templates */
typedef enum {
    CLAW_GRAMMAR_JSON,          /* Valid JSON */
    CLAW_GRAMMAR_JSON_ARRAY,    /* JSON array */
    CLAW_GRAMMAR_JSON_OBJECT,   /* JSON object */
    CLAW_GRAMMAR_TOOL_CALL,     /* Tool call format */
    CLAW_GRAMMAR_CHAT_RESPONSE, /* Chat message format */
    CLAW_GRAMMAR_CODE_BLOCK,    /* Markdown code block */
    CLAW_GRAMMAR_CUSTOM,        /* User-defined */
} claw_grammar_template_t;

/* JSON Schema to Grammar converter */
typedef struct {
    char               *schema_json;
    claw_grammar_t     *grammar;
    
    /* Schema parsing state */
    struct json_schema_parser {
        void           *root;
        char           *current_path;
        claw_symbol_t  *current_symbol;
    } parser;
} claw_schema_to_grammar_t;
```

---

## 3. Event Loop Architecture

```c
/* ============================================================================
 * EVENT LOOP - epoll (Linux) / kqueue (BSD/macOS) / IOCP (Windows)
 * ============================================================================ */

#define CLAW_MAX_EVENTS         1024
#define CLAW_MAX_TIMERS         256
#define CLAW_MAX_SIGNALS        32

typedef enum {
    CLAW_EV_READABLE    = 0x01,
    CLAW_EV_WRITABLE    = 0x02,
    CLAW_EV_ERROR       = 0x04,
    CLAW_EV_HUP         = 0x08,
    CLAW_EV_ET          = 0x10,     /* Edge-triggered */
} claw_event_flags_t;

typedef enum {
    CLAW_EV_IO,                 /* File descriptor event */
    CLAW_EV_TIMER,              /* Timer event */
    CLAW_EV_SIGNAL,             /* Unix signal */
    CLAW_EV_CUSTOM,             /* User-defined event */
    CLAW_EV_CHANNEL,            /* Cross-thread channel */
} claw_event_type_t;

/* Event callback signature */
typedef void (*claw_event_handler_t)(
    struct claw_event_loop *loop,
    struct claw_event *event,
    void *userdata
);

/* Event structure */
typedef struct claw_event {
    claw_event_type_t   type;
    uint32_t            flags;
    
    union {
        struct {
            int         fd;
        } io;
        struct {
            uint64_t    timeout_ms;
            uint64_t    interval_ms;    /* 0 = one-shot */
            uint64_t    fired_count;
        } timer;
        struct {
            int         signum;
        } signal;
        struct {
            void       *data;
            size_t      len;
        } custom;
    } data;
    
    claw_event_handler_t handler;
    void               *userdata;
    
    /* Linked list for timer queue */
    struct claw_event  *timer_next;
    struct claw_event  *timer_prev;
    
    /* State */
    uint8_t             active;
    uint8_t             pending;
    uint8_t             reserved[2];
} claw_event_t;

/* Cross-thread channel (lock-free ring buffer) */
typedef struct {
    uint8_t            *buffer;
    size_t              capacity;
    size_t              elem_size;
    
    /* Lock-free indices */
    atomic_size_t       write_idx;
    atomic_size_t       read_idx;
    
    /* Event to trigger when data available */
    int                 event_fd;     /* eventfd (Linux) or pipe */
} claw_channel_t;

/* Event loop structure */
typedef struct claw_event_loop {
    /* Backend-specific */
    union {
        int             epoll_fd;       /* Linux */
        int             kqueue_fd;      /* BSD/macOS */
        void           *iocp_handle;    /* Windows */
    } backend;
    
    /* Event storage */
    claw_event_t        events[CLAW_MAX_EVENTS];
    uint32_t            event_count;
    
    /* Timer wheel (hierarchical timing wheel) */
    struct {
        claw_event_t   *wheel[4][256];  /* 4-level wheel */
        uint64_t        current_tick;
        uint64_t        tick_resolution_us;
    } timers;
    
    /* Signal handling */
    struct {
        claw_event_t   *handlers[CLAW_MAX_SIGNALS];
        int             self_pipe[2];   /* For signal delivery */
    } signals;
    
    /* Channels for cross-thread communication */
    claw_channel_t     *channels[16];
    uint32_t            channel_count;
    
    /* State */
    atomic_int          running;
    atomic_int          should_stop;
    
    /* Statistics */
    atomic_uint64_t     events_processed;
    atomic_uint64_t     events_pending;
    
    /* Threading */
    pthread_t           thread;
    pthread_mutex_t     lock;
} claw_event_loop_t;

/* Event loop API */
int claw_event_loop_init(claw_event_loop_t *loop);
void claw_event_loop_destroy(claw_event_loop_t *loop);
int claw_event_loop_run(claw_event_loop_t *loop);
void claw_event_loop_stop(claw_event_loop_t *loop);

/* Event registration */
int claw_event_add_io(claw_event_loop_t *loop, int fd, uint32_t flags,
                      claw_event_handler_t handler, void *userdata);
int claw_event_add_timer(claw_event_loop_t *loop, uint64_t timeout_ms,
                         uint64_t interval_ms, claw_event_handler_t handler,
                         void *userdata);
int claw_event_add_signal(claw_event_loop_t *loop, int signum,
                          claw_event_handler_t handler, void *userdata);

/* Channel API (lock-free) */
claw_channel_t *claw_channel_create(size_t capacity, size_t elem_size);
void claw_channel_destroy(claw_channel_t *ch);
int claw_channel_send(claw_channel_t *ch, const void *data);
int claw_channel_recv(claw_channel_t *ch, void *data);
```

---

## 4. Memory Management Strategy

```c
/* ============================================================================
 * MEMORY MANAGEMENT - Arena + Pool + mmap strategies
 * ============================================================================ */

/* Arena allocator for short-lived allocations */
typedef struct claw_arena {
    uint8_t            *base;
    size_t              size;
    size_t              used;
    size_t              committed;
    
    /* Linked list of chunks */
    struct claw_arena  *next;
    struct claw_arena  *prev;
    
    /* Save points for rollback */
    size_t              save_stack[16];
    uint32_t            save_depth;
} claw_arena_t;

/* Object pool for fixed-size allocations */
typedef struct claw_pool {
    size_t              obj_size;
    size_t              objs_per_chunk;
    
    struct pool_chunk {
        uint8_t        *bitmap;         /* Free/used bitmap */
        uint8_t        *data;
        struct pool_chunk *next;
    } *chunks;
    
    /* Free list for O(1) allocation */
    void               *free_list;
    
    atomic_size_t       alloc_count;
    atomic_size_t       free_count;
} claw_pool_t;

/* mmap-based large allocation */
typedef struct claw_mmap_alloc {
    void               *addr;
    size_t              size;
    int                 fd;             /* -1 for anonymous */
    uint64_t            file_offset;
    uint32_t            flags;
    uint32_t            ref_count;
} claw_mmap_alloc_t;

/* Memory statistics */
typedef struct {
    atomic_size_t       total_allocated;
    atomic_size_t       total_freed;
    atomic_size_t       current_used;
    atomic_size_t       mmap_bytes;
    atomic_size_t       arena_bytes;
    atomic_size_t       pool_bytes;
    atomic_size_t       malloc_bytes;
} claw_mem_stats_t;

/* Global memory context */
typedef struct {
    /* Thread-local arenas */
    _Thread_local claw_arena_t *tls_arena;
    
    /* Shared pools */
    claw_pool_t        *message_pool;
    claw_pool_t        *event_pool;
    claw_pool_t        *tensor_pool;
    
    /* mmap cache */
    struct mmap_cache {
        claw_mmap_alloc_t *entries[64];
        uint32_t count;
    } mmap_cache;
    
    /* Statistics */
    claw_mem_stats_t    stats;
    
    /* Configuration */
    size_t              arena_chunk_size;
    size_t              max_mmap_cache;
} claw_mem_context_t;

/* Memory API */
void *claw_arena_alloc(claw_arena_t *arena, size_t size);
void *claw_arena_alloc_aligned(claw_arena_t *arena, size_t size, size_t align);
void claw_arena_save(claw_arena_t *arena);
void claw_arena_restore(claw_arena_t *arena);
void claw_arena_reset(claw_arena_t *arena);

void *claw_pool_alloc(claw_pool_t *pool);
void claw_pool_free(claw_pool_t *pool, void *ptr);

void *claw_mmap_alloc(size_t size, int fd, uint64_t offset, uint32_t flags);
void claw_mmap_free(void *ptr);
```

---

## 5. IPC Mechanism for External API Calls

```c
/* ============================================================================
 * IPC SYSTEM - Unix domain sockets + shared memory
 * ============================================================================ */

#define CLAW_SOCKET_PATH        "/tmp/openclaw.sock"
#define CLAW_SHM_PREFIX         "/openclaw_"
#define CLAW_MAX_SHM_SIZE       (256 * 1024 * 1024)  /* 256MB */

typedef enum {
    CLAW_MSG_HELLO,             /* Client registration */
    CLAW_MSG_REQUEST,           /* API request */
    CLAW_MSG_RESPONSE,          /* API response */
    CLAW_MSG_STREAM_START,      /* Streaming response start */
    CLAW_MSG_STREAM_DATA,       /* Streaming chunk */
    CLAW_MSG_STREAM_END,        /* Streaming end */
    CLAW_MSG_ERROR,             /* Error response */
    CLAW_MSG_HEARTBEAT,         /* Keepalive */
    CLAW_MSG_DISCONNECT,        /* Graceful disconnect */
} claw_ipc_msg_type_t;

/* IPC message header */
typedef struct __attribute__((packed)) {
    uint32_t            magic;          /* 0x49504321 "IPC!" */
    uint32_t            version;
    claw_ipc_msg_type_t type;
    uint32_t            seq_num;
    uint32_t            payload_len;
    uint64_t            shm_offset;     /* If using shared memory */
    char                request_id[64];
} claw_ipc_header_t;

/* Shared memory region */
typedef struct {
    char                name[64];
    int                 fd;
    void               *addr;
    size_t              size;
    
    /* Lock-free ring buffer within shm */
    struct {
        atomic_size_t   write_idx;
        atomic_size_t   read_idx;
        uint8_t         data[];
    } *ring;
} claw_shm_region_t;

/* IPC connection */
typedef struct {
    int                 socket_fd;
    claw_shm_region_t  *shm;
    
    /* Outgoing message queue */
    struct msg_queue {
        claw_ipc_header_t *msgs[256];
        atomic_uint        head;
        atomic_uint        tail;
    } out_queue;
    
    /* State */
    uint32_t            seq_num;
    uint8_t             authenticated;
    uint8_t             reserved[3];
    
    /* User data */
    void               *userdata;
} claw_ipc_conn_t;

/* IPC server */
typedef struct {
    int                 listen_fd;
    char                socket_path[256];
    
    /* Connection pool */
    claw_ipc_conn_t    *connections[64];
    uint32_t            conn_count;
    
    /* Event loop integration */
    claw_event_loop_t  *loop;
    
    /* Callbacks */
    void (*on_connect)(claw_ipc_conn_t *conn);
    void (*on_message)(claw_ipc_conn_t *conn, claw_ipc_header_t *msg, void *payload);
    void (*on_disconnect)(claw_ipc_conn_t *conn);
} claw_ipc_server_t;

/* IPC client */
typedef struct {
    int                 socket_fd;
    char                socket_path[256];
    claw_shm_region_t  *shm;
    
    /* Pending requests */
    struct pending_req {
        char            request_id[64];
        uint32_t        seq_num;
        void           *response_buf;
        size_t          response_len;
        pthread_cond_t  cond;
        pthread_mutex_t lock;
        uint8_t         completed;
    } *pending[64];
    
    /* Thread for receiving */
    pthread_t           recv_thread;
} claw_ipc_client_t;

/* API request/response types */
typedef struct {
    char                method[32];     /* "chat", "tool_call", etc. */
    char               *payload;
    size_t              payload_len;
    uint32_t            timeout_ms;
    uint8_t             streaming;
} claw_api_request_t;

typedef struct {
    int                 status;
    char               *data;
    size_t              data_len;
    char                error_msg[256];
} claw_api_response_t;
```

---

## 6. Plugin System for External Models

```c
/* ============================================================================
 * PLUGIN SYSTEM - External model providers
 * ============================================================================ */

#define CLAW_PLUGIN_MAGIC       0x504C5547  /* "PLUG" */
#define CLAW_PLUGIN_VERSION     1
#define CLAW_MAX_PLUGINS        16
#define CLAW_PLUGIN_NAME_LEN    64

typedef enum {
    CLAW_PLUGIN_OLLAMA,         /* Local Ollama instance */
    CLAW_PLUGIN_RUNPOD,         /* RunPod serverless */
    CLAW_PLUGIN_KIMI,           /* Moonshot Kimi */
    CLAW_PLUGIN_OPENAI,         /* OpenAI API */
    CLAW_PLUGIN_ANTHROPIC,      /* Anthropic Claude */
    CLAW_PLUGIN_BEDROCK,        /* AWS Bedrock */
    CLAW_PLUGIN_CUSTOM,         /* User-defined */
} claw_plugin_type_t;

/* Plugin capabilities */
typedef struct {
    uint32_t            streaming : 1;
    uint32_t            function_calling : 1;
    uint32_t            vision : 1;
    uint32_t            json_mode : 1;
    uint32_t            tool_use : 1;
    uint32_t            reserved : 27;
} claw_plugin_caps_t;

/* Plugin configuration */
typedef struct {
    claw_plugin_type_t  type;
    char                name[CLAW_PLUGIN_NAME_LEN];
    char                endpoint[256];
    char                api_key[256];
    
    /* Model mapping */
    struct model_mapping {
        char            local_name[64];
        char            remote_name[64];
    } models[16];
    uint32_t            model_count;
    
    /* Request parameters */
    uint32_t            timeout_ms;
    uint32_t            max_retries;
    uint32_t            retry_delay_ms;
    
    /* Rate limiting */
    struct {
        uint32_t        requests_per_minute;
        uint32_t        tokens_per_minute;
        atomic_uint     current_requests;
        atomic_uint     current_tokens;
    } rate_limit;
} claw_plugin_config_t;

/* Plugin interface (vtable) */
typedef struct {
    uint32_t            magic;
    uint32_t            version;
    
    /* Lifecycle */
    int (*init)(void **ctx, const claw_plugin_config_t *config);
    void (*destroy)(void *ctx);
    
    /* Capabilities */
    claw_plugin_caps_t (*get_caps)(void *ctx);
    
    /* Model operations */
    int (*list_models)(void *ctx, char ***models, uint32_t *count);
    int (*get_model_info)(void *ctx, const char *model, char **info_json);
    
    /* Inference */
    int (*chat)(void *ctx, const claw_api_request_t *req,
                claw_api_response_t *resp);
    int (*chat_stream)(void *ctx, const claw_api_request_t *req,
                       void (*on_chunk)(const char *data, size_t len, void *ud),
                       void *userdata);
    
    /* Embeddings */
    int (*embed)(void *ctx, const char **texts, uint32_t count,
                 float **embeddings, uint32_t *dim);
    
    /* Health check */
    int (*health_check)(void *ctx);
} claw_plugin_vtable_t;

/* Plugin instance */
typedef struct {
    char                name[CLAW_PLUGIN_NAME_LEN];
    claw_plugin_type_t  type;
    claw_plugin_caps_t  caps;
    
    /* Dynamic library handle */
    void               *dlhandle;
    
    /* Plugin interface */
    claw_plugin_vtable_t *vtable;
    void               *ctx;
    
    /* Configuration */
    claw_plugin_config_t config;
    
    /* Statistics */
    atomic_uint64_t     requests_total;
    atomic_uint64_t     tokens_input;
    atomic_uint64_t     tokens_output;
    atomic_uint64_t     errors;
    
    /* State */
    uint8_t             loaded;
    uint8_t             healthy;
    uint8_t             reserved[2];
} claw_plugin_t;

/* Plugin manager */
typedef struct {
    claw_plugin_t       plugins[CLAW_MAX_PLUGINS];
    uint32_t            plugin_count;
    
    /* Plugin directory */
    char                plugin_dir[256];
    
    /* Model routing table */
    struct model_route {
        char            model_pattern[128];  /* Glob pattern */
        char            plugin_name[CLAW_PLUGIN_NAME_LEN];
        float           priority;
    } routes[64];
    uint32_t            route_count;
    
    pthread_rwlock_t    lock;
} claw_plugin_manager_t;

/* Built-in plugin implementations */
extern claw_plugin_vtable_t claw_ollama_vtable;
extern claw_plugin_vtable_t claw_runpod_vtable;
extern claw_plugin_vtable_t claw_kimi_vtable;
extern claw_plugin_vtable_t claw_openai_vtable;
```

---

## 7. Recommended C Libraries

### 7.1 Core Dependencies

| Library | Version | Purpose | Justification |
|---------|---------|---------|---------------|
| **llama.cpp** | latest | Local LLM inference | Industry standard, GGML backend, Qwen support, mmap-friendly |
| **ggml** | bundled | Tensor operations | Optimized CPU/GPU kernels, quantization support |
| **ncurses** | 6.4+ | Terminal UI | Portable, battle-tested, wide terminal support |
| **libcurl** | 8.0+ | HTTP client | HTTP/2, WebSocket, async support, proxy support |
| **jansson** | 2.14+ | JSON parsing | Simple API, no dependencies, streaming support |
| **pcre2** | 10.42+ | Regex/grammar | Unicode support, JIT compilation, grammar parsing |
| **openssl** | 3.0+ | TLS/crypto | Industry standard, FIPS support |
| **sqlite3** | 3.40+ | Local storage | Embedded, ACID, full-text search |
| **libuv** | 1.45+ | Cross-platform async | Unified epoll/kqueue/IOCP, used by Node.js |

### 7.2 Optional Dependencies

| Library | Purpose | When to Use |
|---------|---------|-------------|
| **CUDA** | GPU acceleration | When NVIDIA GPU available |
| **Vulkan** | Cross-platform GPU | AMD/Intel GPU support |
| **Metal** | Apple GPU | macOS/iOS deployment |
| **onnxruntime** | Model compatibility | ONNX model support |
| **sentencepiece** | Tokenization | Custom tokenizer support |
| **libmagic** | File type detection | Media understanding |
| **libav** | Audio/video processing | Media understanding |

### 7.3 Build System

```cmake
# CMakeLists.txt excerpt
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

# Core dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(NCURSES REQUIRED ncursesw)
pkg_check_modules(JANSSON REQUIRED jansson)
pkg_check_modules(LIBCURL REQUIRED libcurl)
pkg_check_modules(PCRE2 REQUIRED libpcre2-8)

# llama.cpp as submodule
add_subdirectory(third_party/llama.cpp)

# Optional GPU support
option(CLAW_CUDA "Enable CUDA support" OFF)
option(CLAW_VULKAN "Enable Vulkan support" OFF)
option(CLAW_METAL "Enable Metal support" OFF)
```

---

## 8. Memory Layout for mmap-Based Model Access

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                         MODEL FILE LAYOUT (.clawmodel)                          │
├─────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│  ┌─────────────────────────────────────────────────────────────────────────┐   │
│  │                         HEADER SECTION (4KB aligned)                     │   │
│  │  ┌─────────────────────────────────────────────────────────────────┐   │   │
│  │  │ claw_model_header_t (256 bytes)                                  │   │   │
│  │  │   - magic, version, checksum                                     │   │   │
│  │  │   - tensor_count, tensor_data_offset                             │   │   │
│  │  │   - arch config (vocab_size, hidden_size, etc.)                  │   │   │
│  │  └─────────────────────────────────────────────────────────────────┘   │   │
│  │                                                                          │   │
│  │  ┌─────────────────────────────────────────────────────────────────┐   │   │
│  │  │ Tensor Descriptor Table (tensor_count * 192 bytes each)          │   │   │
│  │  │                                                                  │   │   │
│  │  │  [0] tok_embeddings.weight                                       │   │   │
│  │  │      - name_hash: 0xA1B2C3D4                                     │   │   │
│  │  │      - file_offset: 0x10000                                      │   │   │
│  │  │      - size_bytes: 524288000  (vocab * hidden * 2 bytes)         │   │   │
│  │  │      - dims: [32000, 4096], dtype: Q4_K                          │   │   │
│  │  │                                                                  │   │   │
│  │  │  [1] layers.0.attention.wq.weight                                │   │   │
│  │  │      - file_offset: 0x1F4A0000                                   │   │   │
│  │  │      - size_bytes: 12582912                                      │   │   │
│  │  │      - layer_idx: 0, tensor_type: ATTN_Q                         │   │   │
│  │  │                                                                  │   │   │
│  │  │  ... (one entry per tensor)                                      │   │   │
│  │  │                                                                  │   │   │
│  │  │  [N] output_norm.weight                                          │   │   │
│  │  │  [N+1] output.weight                                             │   │   │
│  │  └─────────────────────────────────────────────────────────────────┘   │   │
│  │                                                                          │   │
│  │  ┌─────────────────────────────────────────────────────────────────┐   │   │
│  │  │ Hash Index for O(1) Lookup (separate from descriptor table)      │   │   │
│  │  │   - bucket_count = next_prime(tensor_count * 1.5)                │   │   │
│  │  │   - Each bucket: [hash, tensor_idx, next_offset]                 │   │   │
│  │  └─────────────────────────────────────────────────────────────────┘   │   │
│  └─────────────────────────────────────────────────────────────────────────┘   │
│                                  │                                              │
│                                  ▼                                              │
│  ┌─────────────────────────────────────────────────────────────────────────┐   │
│  │                      TENSOR DATA SECTION (page-aligned)                  │   │
│  │                                                                          │   │
│  │  ┌─────────────────────────────────────────────────────────────────┐   │   │
│  │  │ EMBEDDING TABLE (mmap'd as read-only, shared)                    │   │   │
│  │  │  tok_embeddings.weight                                           │   │   │
│  │  │  ├─ Size: vocab_size * hidden_size * sizeof(qtype)               │   │   │
│  │  │  ├─ Access: Random during token embedding lookup                 │   │   │
│  │  │  └─ Cache: Frequently accessed tokens cached in RAM              │   │   │
│  │  └─────────────────────────────────────────────────────────────────┘   │   │
│  │                                                                          │   │
│  │  ┌─────────────────────────────────────────────────────────────────┐   │   │
│  │  │ TRANSFORMER LAYERS (mmap'd on-demand, per-layer)                 │   │   │
│  │  │                                                                  │   │   │
│  │  │  Layer 0:                                                        │   │   │
│  │  │  ├─ attention.wq.weight      [Q4_K]  (mmap'd when layer active)  │   │   │
│  │  │  ├─ attention.wk.weight      [Q4_K]                              │   │   │
│  │  │  ├─ attention.wv.weight      [Q4_K]                              │   │   │
│  │  │  ├─ attention.wo.weight      [Q4_K]                              │   │   │
│  │  │  ├─ feed_forward.w1.weight   [Q4_K]  (gate projection)           │   │   │
│  │  │  ├─ feed_forward.w2.weight   [Q4_K]  (down projection)           │   │   │
│  │  │  ├─ feed_forward.w3.weight   [Q4_K]  (up projection)             │   │   │
│  │  │  ├─ attention_norm.weight    [F32]                              │   │   │
│  │  │  └─ ffn_norm.weight          [F32]                              │   │   │
│  │  │                                                                  │   │   │
│  │  │  Layer 1...N: (same structure)                                   │   │   │
│  │  │                                                                  │   │   │
│  │  │  Each layer: ~500MB (Q4_K quantized 7B model)                    │   │   │
│  │  │  Only 2-3 layers resident in RAM at once                         │   │   │
│  │  └─────────────────────────────────────────────────────────────────┘   │   │
│  │                                                                          │   │
│  │  ┌─────────────────────────────────────────────────────────────────┐   │   │
│  │  │ OUTPUT HEAD (mmap'd during final generation)                     │   │   │
│  │  │  output_norm.weight        [F32]                                 │   │   │
│  │  │  output.weight             [F16/Q4_K]  (can be large!)           │   │   │
│  │  │  ├─ Size: vocab_size * hidden_size * 2 bytes                     │   │   │
│  │  │  └─ Optimization: Use sparse matmul or chunked access            │   │   │
│  │  └─────────────────────────────────────────────────────────────────┘   │   │
│  │                                                                          │   │
│  │  ┌─────────────────────────────────────────────────────────────────┐   │   │
│  │  │ ROPE FREQUENCY CACHES (precomputed, read-only)                   │   │   │
│  │  │  cos_sin_cache             [F32]                                 │   │   │
│  │  │  ├─ Size: max_seq_len * head_dim * 2                           │   │   │
│  │  │  └─ Always kept in RAM (small, frequently accessed)              │   │   │
│  │  └─────────────────────────────────────────────────────────────────┘   │   │
│  └─────────────────────────────────────────────────────────────────────────┘   │
│                                                                                 │
└─────────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────────┐
│                      RUNTIME MEMORY MAP (Virtual Address Space)                 │
├─────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│  Address Range              │ Content                    │ Access Pattern       │
│  ─────────────────────────────────────────────────────────────────────────────  │
│  0x7f0000000000-0x7f00100000│ Model Header (mmap RO)     │ Always resident      │
│  0x7f0010000000-0x7f00200000│ Tensor Index (mmap RO)     │ Always resident      │
│  0x7f0020000000-0x7f00A00000│ Embeddings (mmap RO)       │ Random, cached       │
│  0x7f00A0000000-0x7f00B00000│ Layer 0-1 (mmap RO)        │ Sequential, evictable│
│  0x7f00B0000000-0x7f00C00000│ Layer 2-3 (mmap RO)        │ Sequential, evictable│
│  0x7f00C0000000-0x7f00D00000│ Layer 4-5 (mmap RO)        │ Sequential, evictable│
│  ...                        │ ...                        │ ...                  │
│  0x7f0FF0000000-0x7f10000000│ Output Head (mmap RO)      │ Final layer only     │
│  ─────────────────────────────────────────────────────────────────────────────  │
│  0x7f1000000000-0x7f10100000│ KV Cache (mmap RW)         │ Per-session, grows   │
│  0x7f1010000000-0x7f10200000│ Activation Buffers (malloc)│ Temporary, per-layer │
│  0x7f1020000000-0x7f10300000│ Tool Registry (mmap RO)    │ Always resident      │
│  0x7f1030000000-0x7f10400000│ Context Messages (arena)   │ Grows with chat      │
│                                                                                 │
└─────────────────────────────────────────────────────────────────────────────────┘
```

### 8.1 mmap Access Patterns

```c
/* ============================================================================
 * mmap ACCESS PATTERNS - Lazy loading with prefetch hints
 * ============================================================================ */

/* Layer-wise inference - only 2 layers resident at once */
void claw_model_forward(claw_model_t *model, 
                        const uint32_t *tokens, 
                        uint32_t num_tokens,
                        float *output_logits) {
    claw_tensor_cache_t *cache = model->cache;
    
    /* Embeddings - random access, cache hot tokens */
    float *embeddings = claw_get_embeddings(model, tokens, num_tokens);
    
    /* Transformer layers - sequential access, prefetch next */
    float *hidden = embeddings;
    for (uint32_t layer = 0; layer < model->header->arch.num_layers; layer++) {
        /* Prefetch next layer while computing current */
        if (layer + 1 < model->header->arch.num_layers) {
            claw_prefetch_layer(model, layer + 1);
        }
        
        /* Compute current layer */
        hidden = claw_transformer_layer(model, layer, hidden, num_tokens);
        
        /* Evict previous layer from cache */
        if (layer > 0) {
            claw_evict_layer(model, layer - 1);
        }
    }
    
    /* Output head - only final token needed for next token prediction */
    claw_compute_logits(model, hidden, output_logits);
}

/* Prefetch hints for madvise() */
void claw_prefetch_layer(claw_model_t *model, uint32_t layer_idx) {
    /* Get layer tensor range */
    uint64_t start_offset = model->layer_offsets[layer_idx];
    uint64_t end_offset = model->layer_offsets[layer_idx + 1];
    size_t size = end_offset - start_offset;
    
    /* Hint to kernel: will need this soon */
    uint8_t *addr = (uint8_t*)model->mmap_base + start_offset;
    madvise(addr, size, MADV_WILLNEED);
    
    /* Optional: Use readahead for sequential prefetch */
    readahead(model->fd, start_offset, size);
}

/* Cache eviction using MADV_DONTNEED */
void claw_evict_layer(claw_model_t *model, uint32_t layer_idx) {
    uint64_t start_offset = model->layer_offsets[layer_idx];
    uint64_t end_offset = model->layer_offsets[layer_idx + 1];
    size_t size = end_offset - start_offset;
    
    uint8_t *addr = (uint8_t*)model->mmap_base + start_offset;
    madvise(addr, size, MADV_DONTNEED);
}
```

---

## 9. File System Organization

```
~/.openclaw/
├── config/
│   └── openclaw.json           # Main configuration
├── models/
│   ├── qwen2.5-7b-q4_k_m.clawmodel    # mmap'd model file
│   ├── qwen2.5-7b-q5_k_m.clawmodel
│   └── index.json              # Model registry
├── cache/
│   ├── embeddings/             # Cached token embeddings
│   ├── kv_cache/               # Session KV caches
│   └── tensor_cache/           # Dequantized tensor cache
├── tools/
│   ├── registry.bin            # Memory-mapped tool registry
│   ├── builtin.so              # Built-in tools library
│   └── plugins/                # Plugin tool libraries
├── sessions/
│   ├── <session_id>/
│   │   ├── context.bin         # Message context
│   │   ├── kv_cache.bin        # KV cache for session
│   │   └── state.json          # Session state
├── skills/
│   └── <skill_name>/
│       └── SKILL.md            # Skill definition
├── logs/
│   └── openclaw.log
└── ipc/
    └── socket                  # Unix domain socket
```

---

## 10. Build and Deployment

### 10.1 Compilation Flags

```bash
# Optimized release build
CFLAGS="-O3 -march=native -mtune=native -flto \
        -DCLAW_USE_MMAP=1 \
        -DCLAW_USE_EPOLL=1 \
        -DCLAW_CACHE_SIZE_MB=512 \
        -DCLAW_MAX_AGENTS=256 \
        -DCLAW_MAX_CONTEXT=32768"

# Debug build
CFLAGS="-O0 -g3 -fsanitize=address,undefined \
        -DCLAW_DEBUG=1 \
        -DCLAW_TRACE_MALLOC=1"
```

### 10.2 Runtime Configuration

```json
{
    "model": {
        "path": "~/.openclaw/models/qwen2.5-7b-q4_k_m.clawmodel",
        "cache_size_mb": 512,
        "threads": 8,
        "batch_size": 512
    },
    "inference": {
        "temperature": 0.7,
        "top_p": 0.9,
        "top_k": 40,
        "max_tokens": 4096,
        "context_window": 32768
    },
    "tools": {
        "registry_path": "~/.openclaw/tools/registry.bin",
        "auto_confirm_safe": true,
        "sandbox_dangerous": true
    },
    "channels": {
        "terminal": { "enabled": true },
        "telegram": { "enabled": false },
        "slack": { "enabled": false }
    },
    "plugins": {
        "ollama": { "endpoint": "http://localhost:11434" },
        "openai": { "api_key": "${OPENAI_API_KEY}" }
    }
}
```

---

## Summary

This architecture provides:

1. **mmap-based model loading** - Only active layers in RAM, 4GB+ models on 8GB systems
2. **O(1) tensor lookup** - Hash-indexed descriptors for instant weight access
3. **Memory-mapped tool registry** - Zero-copy tool discovery
4. **Grammar-constrained generation** - Formal BNF grammars for structured output
5. **Native 7B Qwen inference** - llama.cpp backend with custom optimizations
6. **Terminal UI** - ncurses-based interface with real-time updates
7. **Event-driven architecture** - epoll/kqueue for scalable I/O

The design prioritizes:
- **Memory efficiency** through mmap and LRU caching
- **Low latency** through O(1) lookups and prefetching
- **Extensibility** through the plugin system
- **Safety** through sandboxing and tool safety levels
