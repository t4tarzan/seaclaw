/*
 * OpenClaw C Implementation - Core Header
 * 
 * This header defines the core data structures and APIs for a C-language
 * reimplementation of OpenClaw with mmap-based model loading.
 * 
 * License: MIT
 */

#ifndef OPENCLAW_H
#define OPENCLAW_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <pthread.h>

/* ============================================================================
 * Version and Constants
 * ============================================================================ */

#define CLAW_VERSION_MAJOR      1
#define CLAW_VERSION_MINOR      0
#define CLAW_VERSION_PATCH      0

#define CLAW_MODEL_MAGIC        0x434C4157  /* "CLAW" */
#define CLAW_TOOL_MAGIC         0x544F4F4C  /* "TOOL" */
#define CLAW_PLUGIN_MAGIC       0x504C5547  /* "PLUG" */
#define CLAW_IPC_MAGIC          0x49504321  /* "IPC!" */

#define CLAW_MODEL_VERSION      1
#define CLAW_TOOL_VERSION       1
#define CLAW_PLUGIN_VERSION     1

#define CLAW_MAX_LAYERS         256
#define CLAW_MAX_TENSORS        65536
#define CLAW_MAX_TOOLS          4096
#define CLAW_MAX_PLUGINS        16
#define CLAW_MAX_AGENTS         256
#define CLAW_MAX_EVENTS         1024
#define CLAW_MAX_TIMERS         256
#define CLAW_MAX_SIGNALS        32
#define CLAW_MAX_CONTEXT_MSGS   32768
#define CLAW_MAX_TOOLS_PER_CALL 32

#define CLAW_TENSOR_NAME_LEN    128
#define CLAW_TOOL_NAME_LEN      64
#define CLAW_TOOL_DESC_LEN      512
#define CLAW_TOOL_SCHEMA_LEN    4096
#define CLAW_TOOL_PATH_LEN      256
#define CLAW_PLUGIN_NAME_LEN    64
#define CLAW_SESSION_ID_LEN     64
#define CLAW_AGENT_NAME_LEN     64
#define CLAW_SYMBOL_NAME_LEN    64
#define CLAW_GRAMMAR_MAX_RULES  1024
#define CLAW_GRAMMAR_MAX_SYMBOLS 4096

#define CLAW_SOCKET_PATH        "/tmp/openclaw.sock"
#define CLAW_SHM_PREFIX         "/openclaw_"
#define CLAW_MAX_SHM_SIZE       (256 * 1024 * 1024)

/* ============================================================================
 * Enums
 * ============================================================================ */

typedef enum {
    CLAW_DTYPE_F32      = 0,
    CLAW_DTYPE_F16      = 1,
    CLAW_DTYPE_Q4_0     = 2,
    CLAW_DTYPE_Q4_1     = 3,
    CLAW_DTYPE_Q5_0     = 4,
    CLAW_DTYPE_Q5_1     = 5,
    CLAW_DTYPE_Q8_0     = 6,
    CLAW_DTYPE_Q8_1     = 7,
    CLAW_DTYPE_Q2_K     = 8,
    CLAW_DTYPE_Q3_K     = 9,
    CLAW_DTYPE_Q4_K     = 10,
    CLAW_DTYPE_Q5_K     = 11,
    CLAW_DTYPE_Q6_K     = 12,
    CLAW_DTYPE_Q8_K     = 13,
    CLAW_DTYPE_IQ4_NL   = 14,
} claw_dtype_t;

typedef enum {
    CLAW_TENSOR_ATTN_Q      = 0x01,
    CLAW_TENSOR_ATTN_K      = 0x02,
    CLAW_TENSOR_ATTN_V      = 0x04,
    CLAW_TENSOR_ATTN_O      = 0x08,
    CLAW_TENSOR_FFN_UP      = 0x10,
    CLAW_TENSOR_FFN_GATE    = 0x20,
    CLAW_TENSOR_FFN_DOWN    = 0x40,
    CLAW_TENSOR_EMBED       = 0x80,
    CLAW_TENSOR_NORM        = 0x100,
    CLAW_TENSOR_OUTPUT      = 0x200,
} claw_tensor_type_t;

typedef enum {
    CLAW_TOOL_BUILTIN     = 0,
    CLAW_TOOL_PLUGIN      = 1,
    CLAW_TOOL_SCRIPT      = 2,
    CLAW_TOOL_HTTP        = 3,
    CLAW_TOOL_WEBSOCKET   = 4,
} claw_tool_type_t;

typedef enum {
    CLAW_TOOL_SAFE        = 0,
    CLAW_TOOL_CAUTIOUS    = 1,
    CLAW_TOOL_DANGEROUS   = 2,
    CLAW_TOOL_SANDBOXED   = 3,
} claw_tool_safety_t;

typedef enum {
    CLAW_PLUGIN_OLLAMA    = 0,
    CLAW_PLUGIN_RUNPOD    = 1,
    CLAW_PLUGIN_KIMI      = 2,
    CLAW_PLUGIN_OPENAI    = 3,
    CLAW_PLUGIN_ANTHROPIC = 4,
    CLAW_PLUGIN_BEDROCK   = 5,
    CLAW_PLUGIN_CUSTOM    = 6,
} claw_plugin_type_t;

typedef enum {
    CLAW_AGENT_IDLE         = 0,
    CLAW_AGENT_THINKING     = 1,
    CLAW_AGENT_CALLING_TOOL = 2,
    CLAW_AGENT_STREAMING    = 3,
    CLAW_AGENT_COMPACTING   = 4,
    CLAW_AGENT_PAUSED       = 5,
    CLAW_AGENT_ERROR        = 6,
    CLAW_AGENT_SHUTDOWN     = 7,
} claw_agent_state_t;

typedef enum {
    CLAW_THINK_OFF          = 0,
    CLAW_THINK_MINIMAL      = 1,
    CLAW_THINK_LOW          = 2,
    CLAW_THINK_MEDIUM       = 3,
    CLAW_THINK_HIGH         = 4,
    CLAW_THINK_XHIGH        = 5,
} claw_thinking_level_t;

typedef enum {
    CLAW_ROLE_SYSTEM        = 0,
    CLAW_ROLE_USER          = 1,
    CLAW_ROLE_ASSISTANT     = 2,
    CLAW_ROLE_TOOL          = 3,
} claw_message_role_t;

typedef enum {
    CLAW_EV_READABLE        = 0x01,
    CLAW_EV_WRITABLE        = 0x02,
    CLAW_EV_ERROR           = 0x04,
    CLAW_EV_HUP             = 0x08,
    CLAW_EV_ET              = 0x10,
} claw_event_flags_t;

typedef enum {
    CLAW_EV_IO              = 0,
    CLAW_EV_TIMER           = 1,
    CLAW_EV_SIGNAL          = 2,
    CLAW_EV_CUSTOM          = 3,
    CLAW_EV_CHANNEL         = 4,
} claw_event_type_t;

typedef enum {
    CLAW_MSG_HELLO          = 0,
    CLAW_MSG_REQUEST        = 1,
    CLAW_MSG_RESPONSE       = 2,
    CLAW_MSG_STREAM_START   = 3,
    CLAW_MSG_STREAM_DATA    = 4,
    CLAW_MSG_STREAM_END     = 5,
    CLAW_MSG_ERROR          = 6,
    CLAW_MSG_HEARTBEAT      = 7,
    CLAW_MSG_DISCONNECT     = 8,
} claw_ipc_msg_type_t;

typedef enum {
    CLAW_SYM_TERMINAL       = 0,
    CLAW_SYM_NONTERM        = 1,
    CLAW_SYM_REGEX          = 2,
    CLAW_SYM_CHAR_RANGE     = 3,
    CLAW_SYM_SEQUENCE       = 4,
    CLAW_SYM_CHOICE         = 5,
    CLAW_SYM_OPTIONAL       = 6,
    CLAW_SYM_STAR           = 7,
    CLAW_SYM_PLUS           = 8,
} claw_symbol_type_t;

typedef enum {
    CLAW_GRAMMAR_JSON       = 0,
    CLAW_GRAMMAR_JSON_ARRAY = 1,
    CLAW_GRAMMAR_JSON_OBJECT= 2,
    CLAW_GRAMMAR_TOOL_CALL  = 3,
    CLAW_GRAMMAR_CHAT_RESPONSE = 4,
    CLAW_GRAMMAR_CODE_BLOCK = 5,
    CLAW_GRAMMAR_CUSTOM     = 6,
} claw_grammar_template_t;

/* ============================================================================
 * Model Index Structures
 * ============================================================================ */

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
    uint32_t    sliding_window;
    uint8_t     use_gqa;
    uint8_t     use_sliding_window;
    uint8_t     reserved[6];
} claw_model_arch_t;

typedef struct __attribute__((packed)) {
    uint32_t    name_hash;
    uint64_t    file_offset;
    uint64_t    size_bytes;
    uint32_t    dims[4];
    uint16_t    ndim;
    uint16_t    dtype;
    uint16_t    tensor_type;
    uint16_t    layer_idx;
    char        name[CLAW_TENSOR_NAME_LEN];
} claw_tensor_desc_t;

typedef struct __attribute__((packed)) {
    uint32_t            magic;
    uint32_t            version;
    uint64_t            header_size;
    uint64_t            tensor_data_offset;
    uint64_t            tensor_count;
    claw_model_arch_t   arch;
    char                model_name[64];
    char                quantization[16];
    uint64_t            checksum;
    uint8_t             reserved[64];
} claw_model_header_t;

struct claw_index_entry {
    uint32_t                hash;
    uint32_t                idx;
    struct claw_index_entry *next;
};

struct claw_tensor_index {
    uint32_t                bucket_count;
    struct claw_index_entry **buckets;
};

struct claw_tensor_cache;

typedef struct {
    int                     fd;
    size_t                  file_size;
    void                   *mmap_base;
    size_t                  mmap_size;
    claw_model_header_t    *header;
    claw_tensor_desc_t     *tensor_table;
    struct claw_tensor_index *index;
    atomic_uint             ref_count;
    struct claw_tensor_cache *cache;
    pthread_rwlock_t        lock;
    uint64_t               *layer_offsets;
} claw_model_t;

/* ============================================================================
 * Tool Registry Structures
 * ============================================================================ */

typedef struct {
    char        name[64];
    char        type[32];
    char        description[256];
    uint8_t     required;
    uint8_t     array;
    uint8_t     nullable;
    uint8_t     reserved;
    char        default_value[256];
} claw_tool_param_t;

typedef struct __attribute__((packed)) {
    uint32_t            magic;
    uint32_t            version;
    char                name[CLAW_TOOL_NAME_LEN];
    char                namespace[64];
    uint32_t            name_hash;
    uint32_t            ns_hash;
    char                description[CLAW_TOOL_DESC_LEN];
    claw_tool_type_t    type;
    claw_tool_safety_t  safety_level;
    uint32_t            timeout_ms;
    uint32_t            param_count;
    claw_tool_param_t   params[32];
    char                returns_schema[CLAW_TOOL_SCHEMA_LEN];
    union {
        struct { char symbol_name[128]; char lib_path[CLAW_TOOL_PATH_LEN]; } plugin;
        struct { char interpreter[32]; char script_path[CLAW_TOOL_PATH_LEN]; } script;
        struct { char endpoint[256]; char method[16]; char headers[1024]; } http;
    } exec;
    atomic_uint64_t     call_count;
    atomic_uint64_t     total_latency_us;
    atomic_uint64_t     error_count;
    uint32_t            enabled : 1;
    uint32_t            streaming : 1;
    uint32_t            async_only : 1;
    uint32_t            reserved_flags : 29;
    uint8_t             padding[64];
} claw_tool_desc_t;

typedef struct __attribute__((packed)) {
    uint32_t            magic;
    uint32_t            version;
    uint64_t            header_size;
    uint64_t            tool_count;
    uint64_t            max_tools;
    uint64_t            desc_size;
    uint64_t            last_modified;
    uint8_t             reserved[64];
} claw_tool_registry_header_t;

struct tool_index_entry {
    uint32_t                hash;
    uint32_t                idx;
    struct tool_index_entry *next;
};

struct claw_tool_index {
    uint32_t                bucket_count;
    struct tool_index_entry **buckets;
};

typedef struct {
    int                         fd;
    void                       *mmap_base;
    size_t                      mmap_size;
    claw_tool_registry_header_t *header;
    claw_tool_desc_t           *tools;
    struct claw_tool_index     *index;
    int                         inotify_fd;
    pthread_rwlock_t            lock;
} claw_tool_registry_t;

typedef struct {
    char                tool_name[CLAW_TOOL_NAME_LEN];
    char                namespace[64];
    char                call_id[64];
    char               *params_json;
    size_t              params_len;
    uint32_t            timeout_ms;
    void               *user_data;
} claw_tool_call_t;

typedef struct {
    char                call_id[64];
    int                 status;
    char               *result_json;
    size_t              result_len;
    uint64_t            latency_us;
    char                error_msg[256];
} claw_tool_result_t;

typedef int (*claw_tool_func_t)(
    const claw_tool_call_t *call,
    claw_tool_result_t *result,
    void *context
);

/* ============================================================================
 * Agent State Machine Structures
 * ============================================================================ */

typedef struct claw_message {
    uint64_t            id;
    claw_message_role_t role;
    uint64_t            timestamp_ms;
    char               *content;
    size_t              content_len;
    struct {
        char            tool_name[CLAW_TOOL_NAME_LEN];
        char            call_id[64];
        char           *arguments;
    } tool_call;
    struct {
        char            call_id[64];
        char           *output;
        int             status;
    } tool_result;
    uint32_t            token_count;
    uint32_t            reserved;
    struct claw_message *prev;
    struct claw_message *next;
} claw_message_t;

typedef struct {
    claw_message_t     *head;
    claw_message_t     *tail;
    uint32_t            count;
    uint32_t            total_tokens;
    uint32_t            max_tokens;
    char               *summary;
    uint32_t            summary_tokens;
} claw_context_window_t;

typedef struct claw_grammar claw_grammar_t;
struct claw_inference_ctx;
struct claw_event;

typedef struct {
    char                name[CLAW_AGENT_NAME_LEN];
    char                session_id[CLAW_SESSION_ID_LEN];
    char                model_id[64];
    claw_model_t       *model;
    claw_thinking_level_t thinking_level;
    uint32_t            max_tokens;
    float               temperature;
    float               top_p;
    uint32_t            top_k;
    float               repetition_penalty;
    char                available_tools[CLAW_MAX_TOOLS_PER_CALL][CLAW_TOOL_NAME_LEN];
    uint32_t            tool_count;
    uint8_t             auto_tool_confirm;
    uint32_t            context_window;
    uint32_t            compact_threshold;
    char                channel_type[32];
    char                channel_id[128];
    uint8_t             sandbox_mode;
    uint8_t             require_confirmation;
} claw_agent_config_t;

typedef struct claw_agent {
    uint64_t            id;
    claw_agent_state_t  state;
    claw_agent_config_t config;
    claw_context_window_t context;
    char               *system_prompt;
    char               *soul_md;
    char               *agents_md;
    char               *tools_md;
    claw_grammar_t     *output_grammar;
    struct claw_inference_ctx *inference;
    atomic_uint64_t     messages_received;
    atomic_uint64_t     messages_sent;
    atomic_uint64_t     tokens_generated;
    atomic_uint64_t     tool_calls;
    pthread_mutex_t     state_lock;
    pthread_cond_t      state_cond;
    struct claw_event   *pending_event;
    struct claw_agent   *next;
    struct claw_agent   *prev;
} claw_agent_t;

struct agent_index_entry {
    char                session_id[CLAW_SESSION_ID_LEN];
    claw_agent_t       *agent;
    struct agent_index_entry *next;
};

typedef struct {
    claw_agent_t       *agents;
    uint32_t            count;
    uint32_t            max_agents;
    struct {
        uint32_t bucket_count;
        struct agent_index_entry **buckets;
    } *index;
    pthread_rwlock_t    lock;
} claw_agent_pool_t;

/* ============================================================================
 * Grammar Constraint Structures
 * ============================================================================ */

typedef struct claw_symbol {
    claw_symbol_type_t  type;
    char                name[CLAW_SYMBOL_NAME_LEN];
    uint32_t            id;
    union {
        struct { char *value; size_t len; } terminal;
        struct { uint32_t start; uint32_t end; } range;
        struct { char *pattern; void *regex; } regex;
        struct { struct claw_symbol **children; uint32_t child_count; } composite;
        struct { struct claw_symbol *child; } quantified;
        struct { uint32_t rule_id; } ref;
    } data;
} claw_symbol_t;

typedef struct {
    char                name[CLAW_SYMBOL_NAME_LEN];
    uint32_t            id;
    claw_symbol_t      *rhs;
    uint8_t             is_start;
    uint8_t             reserved[7];
} claw_grammar_rule_t;

struct grammar_symtab_entry {
    char                    name[CLAW_SYMBOL_NAME_LEN];
    uint32_t                rule_id;
    struct grammar_symtab_entry *next;
};

struct claw_grammar {
    char                    name[64];
    claw_grammar_rule_t    *rules;
    uint32_t                rule_count;
    uint32_t                max_rules;
    struct {
        uint32_t bucket_count;
        struct grammar_symtab_entry **buckets;
    } *symtab;
    int                    *ll1_table;
    uint64_t               *first_sets;
    uint64_t               *follow_sets;
};

typedef struct {
    claw_grammar_t         *grammar;
    struct {
        claw_symbol_t      *current_symbol;
        uint32_t            rule_stack[64];
        uint32_t            stack_depth;
    } parser_state;
    uint32_t               *valid_tokens;
    uint32_t                vocab_size;
    struct parse_node      *parse_tree;
} claw_grammar_constraint_t;

/* ============================================================================
 * Event Loop Structures
 * ============================================================================ */

struct claw_event_loop;

typedef void (*claw_event_handler_t)(
    struct claw_event_loop *loop,
    struct claw_event *event,
    void *userdata
);

typedef struct claw_event {
    claw_event_type_t   type;
    uint32_t            flags;
    union {
        struct { int fd; } io;
        struct { uint64_t timeout_ms; uint64_t interval_ms; uint64_t fired_count; } timer;
        struct { int signum; } signal;
        struct { void *data; size_t len; } custom;
    } data;
    claw_event_handler_t handler;
    void               *userdata;
    struct claw_event  *timer_next;
    struct claw_event  *timer_prev;
    uint8_t             active;
    uint8_t             pending;
    uint8_t             reserved[2];
} claw_event_t;

typedef struct {
    uint8_t            *buffer;
    size_t              capacity;
    size_t              elem_size;
    atomic_size_t       write_idx;
    atomic_size_t       read_idx;
    int                 event_fd;
} claw_channel_t;

typedef struct claw_event_loop {
    union {
        int             epoll_fd;
        int             kqueue_fd;
        void           *iocp_handle;
    } backend;
    claw_event_t        events[CLAW_MAX_EVENTS];
    uint32_t            event_count;
    struct {
        claw_event_t   *wheel[4][256];
        uint64_t        current_tick;
        uint64_t        tick_resolution_us;
    } timers;
    struct {
        claw_event_t   *handlers[CLAW_MAX_SIGNALS];
        int             self_pipe[2];
    } signals;
    claw_channel_t     *channels[16];
    uint32_t            channel_count;
    atomic_int          running;
    atomic_int          should_stop;
    atomic_uint64_t     events_processed;
    atomic_uint64_t     events_pending;
    pthread_t           thread;
    pthread_mutex_t     lock;
} claw_event_loop_t;

/* ============================================================================
 * IPC Structures
 * ============================================================================ */

typedef struct __attribute__((packed)) {
    uint32_t            magic;
    uint32_t            version;
    claw_ipc_msg_type_t type;
    uint32_t            seq_num;
    uint32_t            payload_len;
    uint64_t            shm_offset;
    char                request_id[64];
} claw_ipc_header_t;

typedef struct {
    char                name[64];
    int                 fd;
    void               *addr;
    size_t              size;
    struct {
        atomic_size_t   write_idx;
        atomic_size_t   read_idx;
        uint8_t         data[];
    } *ring;
} claw_shm_region_t;

typedef struct {
    int                 socket_fd;
    claw_shm_region_t  *shm;
    struct {
        claw_ipc_header_t *msgs[256];
        atomic_uint        head;
        atomic_uint        tail;
    } out_queue;
    uint32_t            seq_num;
    uint8_t             authenticated;
    uint8_t             reserved[3];
    void               *userdata;
} claw_ipc_conn_t;

typedef struct {
    int                 listen_fd;
    char                socket_path[256];
    claw_ipc_conn_t    *connections[64];
    uint32_t            conn_count;
    claw_event_loop_t  *loop;
    void (*on_connect)(claw_ipc_conn_t *conn);
    void (*on_message)(claw_ipc_conn_t *conn, claw_ipc_header_t *msg, void *payload);
    void (*on_disconnect)(claw_ipc_conn_t *conn);
} claw_ipc_server_t;

typedef struct {
    char                request_id[64];
    uint32_t            seq_num;
    void               *response_buf;
    size_t              response_len;
    pthread_cond_t      cond;
    pthread_mutex_t     lock;
    uint8_t             completed;
} claw_pending_req_t;

typedef struct {
    int                 socket_fd;
    char                socket_path[256];
    claw_shm_region_t  *shm;
    claw_pending_req_t *pending[64];
    pthread_t           recv_thread;
} claw_ipc_client_t;

typedef struct {
    char                method[32];
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

/* ============================================================================
 * Plugin System Structures
 * ============================================================================ */

typedef struct {
    uint32_t            streaming : 1;
    uint32_t            function_calling : 1;
    uint32_t            vision : 1;
    uint32_t            json_mode : 1;
    uint32_t            tool_use : 1;
    uint32_t            reserved : 27;
} claw_plugin_caps_t;

typedef struct {
    claw_plugin_type_t  type;
    char                name[CLAW_PLUGIN_NAME_LEN];
    char                endpoint[256];
    char                api_key[256];
    struct {
        char            local_name[64];
        char            remote_name[64];
    } models[16];
    uint32_t            model_count;
    uint32_t            timeout_ms;
    uint32_t            max_retries;
    uint32_t            retry_delay_ms;
    struct {
        uint32_t        requests_per_minute;
        uint32_t        tokens_per_minute;
        atomic_uint     current_requests;
        atomic_uint     current_tokens;
    } rate_limit;
} claw_plugin_config_t;

typedef struct claw_plugin_vtable {
    uint32_t            magic;
    uint32_t            version;
    int (*init)(void **ctx, const claw_plugin_config_t *config);
    void (*destroy)(void *ctx);
    claw_plugin_caps_t (*get_caps)(void *ctx);
    int (*list_models)(void *ctx, char ***models, uint32_t *count);
    int (*get_model_info)(void *ctx, const char *model, char **info_json);
    int (*chat)(void *ctx, const claw_api_request_t *req, claw_api_response_t *resp);
    int (*chat_stream)(void *ctx, const claw_api_request_t *req,
                       void (*on_chunk)(const char *data, size_t len, void *ud),
                       void *userdata);
    int (*embed)(void *ctx, const char **texts, uint32_t count,
                 float **embeddings, uint32_t *dim);
    int (*health_check)(void *ctx);
} claw_plugin_vtable_t;

typedef struct {
    char                name[CLAW_PLUGIN_NAME_LEN];
    claw_plugin_type_t  type;
    claw_plugin_caps_t  caps;
    void               *dlhandle;
    claw_plugin_vtable_t *vtable;
    void               *ctx;
    claw_plugin_config_t config;
    atomic_uint64_t     requests_total;
    atomic_uint64_t     tokens_input;
    atomic_uint64_t     tokens_output;
    atomic_uint64_t     errors;
    uint8_t             loaded;
    uint8_t             healthy;
    uint8_t             reserved[2];
} claw_plugin_t;

typedef struct {
    claw_plugin_t       plugins[CLAW_MAX_PLUGINS];
    uint32_t            plugin_count;
    char                plugin_dir[256];
    struct {
        char            model_pattern[128];
        char            plugin_name[CLAW_PLUGIN_NAME_LEN];
        float           priority;
    } routes[64];
    uint32_t            route_count;
    pthread_rwlock_t    lock;
} claw_plugin_manager_t;

/* ============================================================================
 * Memory Management Structures
 * ============================================================================ */

typedef struct claw_arena {
    uint8_t            *base;
    size_t              size;
    size_t              used;
    size_t              committed;
    struct claw_arena  *next;
    struct claw_arena  *prev;
    size_t              save_stack[16];
    uint32_t            save_depth;
} claw_arena_t;

typedef struct claw_pool {
    size_t              obj_size;
    size_t              objs_per_chunk;
    struct pool_chunk {
        uint8_t        *bitmap;
        uint8_t        *data;
        struct pool_chunk *next;
    } *chunks;
    void               *free_list;
    atomic_size_t       alloc_count;
    atomic_size_t       free_count;
} claw_pool_t;

typedef struct claw_mmap_alloc {
    void               *addr;
    size_t              size;
    int                 fd;
    uint64_t            file_offset;
    uint32_t            flags;
    uint32_t            ref_count;
} claw_mmap_alloc_t;

typedef struct {
    atomic_size_t       total_allocated;
    atomic_size_t       total_freed;
    atomic_size_t       current_used;
    atomic_size_t       mmap_bytes;
    atomic_size_t       arena_bytes;
    atomic_size_t       pool_bytes;
    atomic_size_t       malloc_bytes;
} claw_mem_stats_t;

typedef struct {
    claw_arena_t       *tls_arena;
    claw_pool_t        *message_pool;
    claw_pool_t        *event_pool;
    claw_pool_t        *tensor_pool;
    struct {
        claw_mmap_alloc_t *entries[64];
        uint32_t count;
    } mmap_cache;
    claw_mem_stats_t    stats;
    size_t              arena_chunk_size;
    size_t              max_mmap_cache;
} claw_mem_context_t;

/* ============================================================================
 * API Function Declarations
 * ============================================================================ */

/* Model API */
int claw_model_load(const char *path, claw_model_t **model);
void claw_model_unload(claw_model_t *model);
claw_tensor_desc_t *claw_model_get_tensor(claw_model_t *model, const char *name);
void *claw_model_map_tensor(claw_model_t *model, claw_tensor_desc_t *desc);
void claw_model_prefetch_layer(claw_model_t *model, uint32_t layer_idx);
void claw_model_evict_layer(claw_model_t *model, uint32_t layer_idx);

/* Tool Registry API */
int claw_tool_registry_load(const char *path, claw_tool_registry_t **registry);
void claw_tool_registry_unload(claw_tool_registry_t *registry);
claw_tool_desc_t *claw_tool_lookup(claw_tool_registry_t *registry, 
                                    const char *namespace, 
                                    const char *name);
int claw_tool_call(claw_tool_registry_t *registry, 
                   const claw_tool_call_t *call, 
                   claw_tool_result_t *result);

/* Agent API */
int claw_agent_create(const claw_agent_config_t *config, claw_agent_t **agent);
void claw_agent_destroy(claw_agent_t *agent);
int claw_agent_process_message(claw_agent_t *agent, 
                                const char *message, 
                                char **response);
int claw_agent_set_state(claw_agent_t *agent, claw_agent_state_t new_state);

/* Event Loop API */
int claw_event_loop_init(claw_event_loop_t *loop);
void claw_event_loop_destroy(claw_event_loop_t *loop);
int claw_event_loop_run(claw_event_loop_t *loop);
void claw_event_loop_stop(claw_event_loop_t *loop);
int claw_event_add_io(claw_event_loop_t *loop, int fd, uint32_t flags,
                      claw_event_handler_t handler, void *userdata);
int claw_event_add_timer(claw_event_loop_t *loop, uint64_t timeout_ms,
                         uint64_t interval_ms, claw_event_handler_t handler,
                         void *userdata);

/* Grammar API */
int claw_grammar_from_template(claw_grammar_template_t tmpl, claw_grammar_t **grammar);
int claw_grammar_from_json_schema(const char *schema, claw_grammar_t **grammar);
void claw_grammar_destroy(claw_grammar_t *grammar);
int claw_grammar_get_valid_tokens(claw_grammar_t *grammar, 
                                   uint32_t *valid_mask, 
                                   uint32_t vocab_size);

/* Memory API */
void *claw_arena_alloc(claw_arena_t *arena, size_t size);
void claw_arena_save(claw_arena_t *arena);
void claw_arena_restore(claw_arena_t *arena);
void *claw_pool_alloc(claw_pool_t *pool);
void claw_pool_free(claw_pool_t *pool, void *ptr);

/* Plugin API */
int claw_plugin_load(claw_plugin_manager_t *mgr, 
                      const char *path, 
                      claw_plugin_t **plugin);
void claw_plugin_unload(claw_plugin_t *plugin);
int claw_plugin_chat(claw_plugin_t *plugin, 
                      const claw_api_request_t *req, 
                      claw_api_response_t *resp);

#ifdef __cplusplus
}
#endif

#endif /* OPENCLAW_H */
