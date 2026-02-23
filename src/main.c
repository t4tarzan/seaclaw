/*
 * main.c — The Nervous System
 *
 * Sea-Claw Sovereign Terminal entry point.
 * Single-threaded event loop. Arena-based memory.
 * "We stop building software that breaks.
 *  We start building logic that survives."
 */

#include "seaclaw/sea_types.h"
#include "seaclaw/sea_arena.h"
#include "seaclaw/sea_log.h"
#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_shield.h"
#include "seaclaw/sea_json.h"
#include "seaclaw/sea_telegram.h"
#include "seaclaw/sea_bus.h"
#include "seaclaw/sea_channel.h"
#include "seaclaw/sea_db.h"
#include "seaclaw/sea_config.h"
#include "seaclaw/sea_agent.h"
#include "seaclaw/sea_a2a.h"
#include "seaclaw/sea_cron.h"
#include "seaclaw/sea_memory.h"
#include "seaclaw/sea_skill.h"
#include "seaclaw/sea_usage.h"
#include "seaclaw/sea_recall.h"
#include "seaclaw/sea_pii.h"
#include "seaclaw/sea_mesh.h"
#include "sea_zero.h"
#include "sea_proxy.h"
#include "sea_workspace.h"
#include "seaclaw/sea_cli.h"
#include "seaclaw/sea_ext.h"
#include "seaclaw/sea_auth.h"
#include "seaclaw/sea_heartbeat.h"
#include "seaclaw/sea_graph.h"
#include "seaclaw/sea_ws.h"
#include <pthread.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>

#ifdef HAVE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

/* ── Constants ────────────────────────────────────────────── */

#define ARENA_SIZE       (16 * 1024 * 1024)   /* 16 MB */
#define REQUEST_ARENA    (1  * 1024 * 1024)    /* 1 MB per request */
#define INPUT_BUF_SIZE   4096
#define DEFAULT_DB_PATH  "seaclaw.db"

/* ── ASCII Banner ─────────────────────────────────────────── */

static const char* BANNER =
    "\033[36m"
    "  ____  ______  ___      ________  ___  ___      __\n"
    " / ___// ____/ / /  |   / ____/ / / /  | |     / /\n"
    " \\__ \\/ __/   / /|  |  / /   / / / /   | | /| / / \n"
    " ___/ / /___  / ___ | / /___/ /_/ /    | |/ |/ /  \n"
    "/____/_____/ /_/  |_| \\____/\\____/     |_/__|__/  \n"
    "\033[0m";

/* ── Global state ─────────────────────────────────────────── */

static volatile bool s_running = true;
static SeaArena      s_session_arena;
static SeaArena      s_request_arena;
static SeaTelegram   s_telegram;
static bool          s_telegram_mode = false;
static bool          s_gateway_mode = false;
SeaDb*               s_db = NULL;
static const char*   s_db_path = DEFAULT_DB_PATH;
static SeaConfig     s_config;
static const char*   s_config_path = "config.json";
SeaAgentConfig s_agent_cfg;
static SeaBus        s_bus;
SeaBus*              s_bus_ptr = NULL;
static SeaChannelManager s_chan_mgr;
static SeaCronScheduler  s_cron_inst;
SeaCronScheduler*        s_cron = NULL;
static SeaMemory         s_memory_inst;
SeaMemory*               s_memory = NULL;
static SeaSkillRegistry  s_skill_reg;
static SeaUsageTracker   s_usage_inst;
SeaUsageTracker*         s_usage = NULL;
static SeaRecall         s_recall_inst;
SeaRecall*               s_recall = NULL;
static SeaMesh           s_mesh_inst;
SeaMesh*                 s_mesh = NULL;
static bool              s_mesh_mode = false;
static const char*       s_mesh_role_str = NULL;
static SeaCli            s_cli;
static SeaExtRegistry    s_ext_reg;
static SeaAuth           s_auth_inst;
SeaAuth*                 s_auth = NULL;
static SeaHeartbeat      s_heartbeat_inst;
SeaHeartbeat*            s_heartbeat = NULL;
static SeaGraph          s_graph_inst;
SeaGraph*                s_graph = NULL;
static SeaWsServer       s_ws_inst;

/* ── Signal handler ───────────────────────────────────────── */

static void handle_signal(int sig) {
    (void)sig;
    s_running = false;
}

/* ── .env file loader ────────────────────────────────────── */

static void load_dotenv(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return;
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        /* Skip comments and empty lines */
        char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\0') continue;
        /* Find = separator */
        char* eq = strchr(p, '=');
        if (!eq) continue;
        *eq = '\0';
        char* key = p;
        char* val = eq + 1;
        /* Trim trailing whitespace/newline from key */
        char* ke = eq - 1;
        while (ke > key && (*ke == ' ' || *ke == '\t')) *ke-- = '\0';
        /* Trim leading whitespace from value */
        while (*val == ' ' || *val == '\t') val++;
        /* Trim trailing whitespace/newline from value */
        size_t vlen = strlen(val);
        while (vlen > 0 && (val[vlen-1] == '\n' || val[vlen-1] == '\r' ||
               val[vlen-1] == ' ' || val[vlen-1] == '\t')) val[--vlen] = '\0';
        /* Strip surrounding quotes */
        if (vlen >= 2 && ((val[0] == '"' && val[vlen-1] == '"') ||
                          (val[0] == '\'' && val[vlen-1] == '\''))) {
            val[vlen-1] = '\0';
            val++;
        }
        /* Only set if not already in environment */
        setenv(key, val, 0);
    }
    fclose(f);
    SEA_LOG_INFO("CONFIG", "Loaded .env file: %s", path);
}

/* ── Provider name → enum ────────────────────────────────── */

static SeaLlmProvider parse_provider(const char* name) {
    if (!name) return SEA_LLM_OPENAI;
    if (strcmp(name, "anthropic") == 0)  return SEA_LLM_ANTHROPIC;
    if (strcmp(name, "gemini") == 0)     return SEA_LLM_GEMINI;
    if (strcmp(name, "openrouter") == 0) return SEA_LLM_OPENROUTER;
    if (strcmp(name, "local") == 0)      return SEA_LLM_LOCAL;
    if (strcmp(name, "zai") == 0)        return SEA_LLM_ZAI;
    return SEA_LLM_OPENAI;
}

/* ── Streaming callback for TUI ───────────────────────────── */

static bool tui_stream_cb(const char* chunk, u32 chunk_len, void* user_data) {
    (void)user_data;
    if (chunk && chunk_len > 0) {
        fwrite(chunk, 1, chunk_len, stdout);
        fflush(stdout);
    }
    return true; /* continue streaming */
}

/* ── Provider name from enum ─────────────────────────────── */

static const char* provider_name(SeaLlmProvider p) {
    switch (p) {
        case SEA_LLM_OPENAI:     return "openai";
        case SEA_LLM_ANTHROPIC:  return "anthropic";
        case SEA_LLM_GEMINI:     return "gemini";
        case SEA_LLM_OPENROUTER: return "openrouter";
        case SEA_LLM_LOCAL:      return "local";
        case SEA_LLM_ZAI:        return "zai";
    }
    return "unknown";
}

/* ── TUI: /model ─────────────────────────────────────────── */

static void cmd_model(const char* input) {
    const char* rest = input + 6; /* skip "/model" */
    while (*rest == ' ') rest++;
    if (*rest == '\0') {
        printf("\n  \033[1mCurrent model:\033[0m %s\n", s_agent_cfg.model ? s_agent_cfg.model : "(none)");
        printf("  Usage: /model <model_name>\n");
        printf("  Examples: /model gpt-4o  /model claude-sonnet-4-20250514  /model gemini-2.5-flash\n\n");
        return;
    }
    sea_agent_set_model(&s_agent_cfg, rest);
    printf("  \033[32m✓\033[0m Model set to: %s\n", rest);
}

/* ── TUI: /provider ──────────────────────────────────────── */

static void cmd_provider(const char* input) {
    const char* rest = input + 9; /* skip "/provider" */
    while (*rest == ' ') rest++;
    if (*rest == '\0') {
        printf("\n  \033[1mCurrent provider:\033[0m %s\n", provider_name(s_agent_cfg.provider));
        printf("  \033[1mModel:\033[0m           %s\n", s_agent_cfg.model ? s_agent_cfg.model : "(default)");
        printf("  \033[1mAPI URL:\033[0m         %s\n", s_agent_cfg.api_url ? s_agent_cfg.api_url : "(default)");
        printf("  \033[1mAPI Key:\033[0m         %s\n", (s_agent_cfg.api_key && *s_agent_cfg.api_key) ? "\033[32mset\033[0m" : "\033[31mmissing\033[0m");
        printf("  Usage: /provider <name> [api_key]\n");
        printf("  Names: openai, anthropic, gemini, openrouter, local, zai\n\n");
        return;
    }
    /* Parse: /provider <name> [key] */
    char prov_name[32] = {0};
    const char* key = NULL;
    u32 i = 0;
    while (rest[i] && rest[i] != ' ' && i < sizeof(prov_name) - 1) {
        prov_name[i] = rest[i]; i++;
    }
    prov_name[i] = '\0';
    if (rest[i] == ' ') {
        key = rest + i + 1;
        while (*key == ' ') key++;
        if (*key == '\0') key = NULL;
    }
    SeaLlmProvider prov = parse_provider(prov_name);
    sea_agent_set_provider(&s_agent_cfg, prov, key, NULL);
    printf("  \033[32m✓\033[0m Provider: %s, Model: %s\n", prov_name, s_agent_cfg.model);
    if (key) printf("  \033[32m✓\033[0m API key updated\n");
}

/* ── TUI: /config ────────────────────────────────────────── */

static void cmd_config(void) {
    printf("\n  \033[1mSea-Claw Configuration\033[0m\n");
    printf("  ════════════════════════════════════════\n\n");
    printf("  \033[1mLLM:\033[0m\n");
    printf("    provider:    %s\n", provider_name(s_agent_cfg.provider));
    printf("    model:       %s\n", s_agent_cfg.model ? s_agent_cfg.model : "(default)");
    printf("    api_url:     %s\n", s_agent_cfg.api_url ? s_agent_cfg.api_url : "(default)");
    printf("    api_key:     %s\n", (s_agent_cfg.api_key && *s_agent_cfg.api_key) ? "\033[32m✓ set\033[0m" : "\033[31m✗ missing\033[0m");
    printf("    max_tokens:  %u\n", s_agent_cfg.max_tokens);
    printf("    temperature: %.1f\n", s_agent_cfg.temperature);
    printf("    think_level: %s\n", sea_agent_think_level_name(s_agent_cfg.think_level));
    printf("    tool_rounds: %u\n", s_agent_cfg.max_tool_rounds);
    printf("    streaming:   %s\n", s_agent_cfg.stream_cb ? "\033[32mon\033[0m" : "off");
    if (s_agent_cfg.fallback_count > 0) {
        printf("\n  \033[1mFallbacks:\033[0m (%u)\n", s_agent_cfg.fallback_count);
        for (u32 fi = 0; fi < s_agent_cfg.fallback_count; fi++) {
            printf("    %u. %s / %s %s\n", fi + 1,
                   provider_name(s_agent_cfg.fallbacks[fi].provider),
                   s_agent_cfg.fallbacks[fi].model ? s_agent_cfg.fallbacks[fi].model : "(default)",
                   (s_agent_cfg.fallbacks[fi].api_key && *s_agent_cfg.fallbacks[fi].api_key) ? "\033[32m✓\033[0m" : "\033[31m✗\033[0m");
        }
    }
    printf("\n  \033[1mSystem:\033[0m\n");
    printf("    database:    %s\n", s_db ? "\033[32mconnected\033[0m" : "\033[31mnone\033[0m");
    printf("    tools:       %u registered\n", sea_tools_count());
    printf("    cron:        %u jobs\n", s_cron ? sea_cron_count(s_cron) : 0);
    printf("    recall:      %u facts\n", s_recall ? sea_recall_count(s_recall) : 0);
    printf("    graph:       %u entities\n", s_graph ? sea_graph_entity_count(s_graph) : 0);
    printf("    auth:        %u tokens\n", s_auth ? s_auth->count : 0);
    printf("\n  \033[1mRuntime:\033[0m\n");
    printf("    config_file: %s\n", s_config_path);
    printf("    db_path:     %s\n", s_db_path);
#ifdef HAVE_READLINE
    printf("    readline:    \033[32menabled\033[0m\n");
#else
    printf("    readline:    disabled\n");
#endif
    printf("\n");
}

/* ── TUI: /stream ────────────────────────────────────────── */

static void cmd_stream(const char* input) {
    const char* rest = input + 7; /* skip "/stream" */
    while (*rest == ' ') rest++;
    if (strcmp(rest, "on") == 0) {
        s_agent_cfg.stream_cb = tui_stream_cb;
        s_agent_cfg.stream_user_data = NULL;
        printf("  \033[32m✓\033[0m Streaming enabled\n");
    } else if (strcmp(rest, "off") == 0) {
        s_agent_cfg.stream_cb = NULL;
        s_agent_cfg.stream_user_data = NULL;
        printf("  \033[32m✓\033[0m Streaming disabled\n");
    } else {
        printf("  Streaming: %s\n", s_agent_cfg.stream_cb ? "\033[32mon\033[0m" : "off");
        printf("  Usage: /stream on|off\n");
    }
}

/* ── TUI: /think ─────────────────────────────────────────── */

static void cmd_think(const char* input) {
    const char* rest = input + 6; /* skip "/think" */
    while (*rest == ' ') rest++;
    if (strcmp(rest, "off") == 0) sea_agent_set_think_level(&s_agent_cfg, SEA_THINK_OFF);
    else if (strcmp(rest, "low") == 0) sea_agent_set_think_level(&s_agent_cfg, SEA_THINK_LOW);
    else if (strcmp(rest, "medium") == 0) sea_agent_set_think_level(&s_agent_cfg, SEA_THINK_MEDIUM);
    else if (strcmp(rest, "high") == 0) sea_agent_set_think_level(&s_agent_cfg, SEA_THINK_HIGH);
    else {
        printf("  Think level: %s\n", sea_agent_think_level_name(s_agent_cfg.think_level));
        printf("  Usage: /think off|low|medium|high\n");
        return;
    }
    printf("  \033[32m✓\033[0m Think: %s (temp=%.1f, max_tokens=%u)\n",
           sea_agent_think_level_name(s_agent_cfg.think_level),
           s_agent_cfg.temperature, s_agent_cfg.max_tokens);
}

/* ── Command dispatch ─────────────────────────────────────── */

static void cmd_help(void) {
    printf("\n  \033[1mCommands:\033[0m\n");
    printf("    /help              Show this help\n");
    printf("    /status            System status\n");
    printf("    /tools             List available tools\n");
    printf("    /exec <tool> <arg> Execute a tool\n");
    printf("    /tasks             List pending tasks\n");
    printf("    /clear             Clear screen\n");
    printf("    /quit              Exit Sea-Claw\n");
    printf("\n  \033[1mModules:\033[0m\n");
    printf("    /skills            List installed skills\n");
    printf("    /auth              List auth tokens\n");
    printf("    /auth create <lbl> Create a new auth token\n");
    printf("    /auth revoke <pfx> Revoke token by prefix\n");
    printf("    /heartbeat         Heartbeat status + pending tasks\n");
    printf("    /graph             Knowledge graph stats\n");
    printf("    /graph add <t> <n> Add entity (type: person/project/...)\n");
    printf("    /graph link <f><r><t> Link two entities\n");
    printf("    /graph search <q>  Search entities by name\n");
    printf("    /cron              List cron jobs\n");
    printf("    /cron add <n><s><c> Add cron job (name schedule cmd)\n");
    printf("    /cron pause <id>   Pause a cron job\n");
    printf("    /cron resume <id>  Resume a cron job\n");
    printf("    /cron remove <id>  Remove a cron job\n");
    printf("    /recall            Memory vault stats\n");
    printf("    /recall search <q> Search facts by query\n");
    printf("    /recall store <c><t> Store fact (category text)\n");
    printf("    /recall forget <id> Forget a fact\n");
    printf("    /mesh              Mesh network status\n");
    printf("\n  \033[1mLLM Control:\033[0m\n");
    printf("    /config            Show current configuration\n");
    printf("    /model [name]      View or change LLM model\n");
    printf("    /provider [name]   View or change LLM provider\n");
    printf("    /stream on|off     Toggle token streaming\n");
    printf("    /think <level>     Set think level (off/low/medium/high)\n");
    printf("\n  \033[1mSeaZero (Agent Zero):\033[0m\n");
    printf("    /agents            List Agent Zero instances\n");
    printf("    /delegate <task>   Delegate task to Agent Zero\n");
    printf("    /sztasks           Show delegated task history\n");
    printf("    /usage             LLM token usage breakdown\n");
    printf("    /audit             Recent security events\n");
    printf("\n");
}

static void cmd_status(void) {
    SeaSlice output;
    SeaSlice args = SEA_SLICE_EMPTY;
    SeaError err = sea_tool_exec("system_status", args, &s_request_arena, &output);
    if (err == SEA_OK) {
        printf("\n  %.*s\n\n", (int)output.len, (const char*)output.data);
    } else {
        printf("  Error: %s\n", sea_error_str(err));
    }
    sea_arena_reset(&s_request_arena);
}

static void cmd_exec(const char* input) {
    /* Skip "/exec " prefix */
    const char* rest = input + 6;
    while (*rest == ' ') rest++;

    if (*rest == '\0') {
        printf("  Usage: /exec <tool_name> [args]\n");
        return;
    }

    /* Extract tool name */
    char tool_name[SEA_MAX_TOOL_NAME];
    u32 i = 0;
    while (rest[i] && rest[i] != ' ' && i < SEA_MAX_TOOL_NAME - 1) {
        tool_name[i] = rest[i];
        i++;
    }
    tool_name[i] = '\0';

    /* Extract args */
    const char* arg_start = rest + i;
    while (*arg_start == ' ') arg_start++;
    SeaSlice args = {
        .data = (const u8*)arg_start,
        .len  = (u32)strlen(arg_start)
    };

    u64 t0 = sea_log_elapsed_ms();

    SeaSlice output;
    SeaError err = sea_tool_exec(tool_name, args, &s_request_arena, &output);

    u64 t1 = sea_log_elapsed_ms();

    if (err == SEA_OK) {
        printf("\n  \033[32m✓\033[0m %.*s\n", (int)output.len, (const char*)output.data);
    } else {
        printf("  \033[31m✗\033[0m %s\n", sea_error_str(err));
    }
    printf("  \033[2m(%lu ms)\033[0m\n\n", (unsigned long)(t1 - t0));

    sea_arena_reset(&s_request_arena);
}

/* ── TUI: /skills ─────────────────────────────────────────── */

static void cmd_skills(void) {
    u32 count = sea_skill_count(&s_skill_reg);
    printf("\n  \033[1mSkills (%u):\033[0m\n", count);
    if (count == 0) {
        printf("    (none installed — use /exec skill_install <url>)\n");
    }
    for (u32 i = 0; i < count; i++) {
        const SeaSkill* s = &s_skill_reg.skills[i];
        const char* icon = s->enabled ? "\033[32m●\033[0m" : "\033[31m○\033[0m";
        printf("    %s %-20s  trigger: %-12s  %s\n",
               icon, s->name, s->trigger[0] ? s->trigger : "(none)",
               s->enabled ? "enabled" : "disabled");
    }
    printf("\n");
}

/* ── TUI: /auth ──────────────────────────────────────────── */

static void cmd_auth(const char* input) {
    if (!s_auth) {
        printf("  Auth not initialized.\n");
        return;
    }

    /* /auth create <label> */
    if (strncmp(input, "/auth create ", 13) == 0) {
        const char* label = input + 13;
        while (*label == ' ') label++;
        if (*label == '\0') {
            printf("  Usage: /auth create <label>\n");
            return;
        }
        char token[SEA_TOKEN_LEN + 1];
        SeaError err = sea_auth_create_token(s_auth, label, SEA_PERM_ALL, 0, token);
        if (err == SEA_OK) {
            printf("\n  \033[32m✓\033[0m Token created for '%s'\n", label);
            printf("    Token: %.16s...%.4s\n", token, token + SEA_TOKEN_LEN - 4);
            printf("    Perms: ALL (0xFF)\n");
            printf("    \033[33m⚠ Save this token — it won't be shown again in full.\033[0m\n\n");
        } else {
            printf("  \033[31m✗\033[0m Failed: %s\n", sea_error_str(err));
        }
        return;
    }

    /* /auth revoke <token_prefix> */
    if (strncmp(input, "/auth revoke ", 13) == 0) {
        const char* prefix = input + 13;
        while (*prefix == ' ') prefix++;
        /* Find token by prefix match */
        bool found = false;
        for (u32 i = 0; i < s_auth->count; i++) {
            if (strncmp(s_auth->tokens[i].token, prefix, strlen(prefix)) == 0) {
                SeaError err = sea_auth_revoke(s_auth, s_auth->tokens[i].token);
                if (err == SEA_OK) {
                    printf("  \033[32m✓\033[0m Revoked token '%s'\n", s_auth->tokens[i].label);
                    found = true;
                }
                break;
            }
        }
        if (!found) printf("  \033[31m✗\033[0m No token matching prefix '%s'\n", prefix);
        return;
    }

    /* /auth — list tokens */
    u32 total = s_auth->count;
    u32 active = sea_auth_active_count(s_auth);
    printf("\n  \033[1mAuth Tokens (%u total, %u active):\033[0m\n", total, active);
    if (total == 0) {
        printf("    (none — create with /auth create <label>)\n");
    }
    for (u32 i = 0; i < total; i++) {
        const SeaAuthToken* t = &s_auth->tokens[i];
        const char* icon = t->revoked ? "\033[31m✗\033[0m" : "\033[32m●\033[0m";
        printf("    %s %-20s  perms: 0x%02X  token: %.8s...\n",
               icon, t->label, t->permissions, t->token);
    }
    printf("\n");
}

/* ── TUI: /heartbeat ─────────────────────────────────────── */

static void cmd_heartbeat(void) {
    if (!s_heartbeat) {
        printf("  Heartbeat not initialized (needs memory system).\n");
        return;
    }
    printf("\n  \033[1mHeartbeat Status:\033[0m\n");
    printf("    Enabled:    %s\n", s_heartbeat->enabled ? "\033[32myes\033[0m" : "\033[31mno\033[0m");
    printf("    Interval:   %llu sec (%llu min)\n",
           (unsigned long long)s_heartbeat->interval_sec,
           (unsigned long long)s_heartbeat->interval_sec / 60);
    printf("    Checks:     %u\n", sea_heartbeat_check_count(s_heartbeat));
    printf("    Injected:   %u tasks\n", sea_heartbeat_injected_count(s_heartbeat));
    printf("    DB logging: %s\n", s_heartbeat->db ? "\033[32myes\033[0m" : "no");

    /* Show pending tasks from HEARTBEAT.md */
    SeaHeartbeatTask tasks[SEA_HEARTBEAT_MAX_TASKS];
    u32 pending = sea_heartbeat_parse(s_heartbeat, tasks, SEA_HEARTBEAT_MAX_TASKS);
    if (pending > 0) {
        printf("\n  \033[1mPending Tasks (%u):\033[0m\n", pending);
        for (u32 i = 0; i < pending; i++) {
            printf("    ○ [L%u] %s\n", tasks[i].line, tasks[i].text);
        }
    } else {
        printf("\n    No pending tasks in HEARTBEAT.md\n");
    }
    printf("\n");
}

/* ── TUI: /graph ─────────────────────────────────────────── */

static const char* entity_type_str(SeaEntityType t) {
    switch (t) {
        case SEA_ENTITY_PERSON:     return "person";
        case SEA_ENTITY_PROJECT:    return "project";
        case SEA_ENTITY_DECISION:   return "decision";
        case SEA_ENTITY_COMMITMENT: return "commitment";
        case SEA_ENTITY_TOPIC:      return "topic";
        case SEA_ENTITY_TOOL:       return "tool";
        case SEA_ENTITY_LOCATION:   return "location";
        case SEA_ENTITY_CUSTOM:     return "custom";
    }
    return "unknown";
}

static SeaEntityType parse_entity_type(const char* s) {
    if (strcmp(s, "person") == 0)     return SEA_ENTITY_PERSON;
    if (strcmp(s, "project") == 0)    return SEA_ENTITY_PROJECT;
    if (strcmp(s, "decision") == 0)   return SEA_ENTITY_DECISION;
    if (strcmp(s, "commitment") == 0) return SEA_ENTITY_COMMITMENT;
    if (strcmp(s, "topic") == 0)      return SEA_ENTITY_TOPIC;
    if (strcmp(s, "tool") == 0)       return SEA_ENTITY_TOOL;
    if (strcmp(s, "location") == 0)   return SEA_ENTITY_LOCATION;
    return SEA_ENTITY_CUSTOM;
}

static SeaRelType parse_rel_type(const char* s) {
    if (strcmp(s, "works_on") == 0)     return SEA_REL_WORKS_ON;
    if (strcmp(s, "decided") == 0)      return SEA_REL_DECIDED;
    if (strcmp(s, "owns") == 0)         return SEA_REL_OWNS;
    if (strcmp(s, "depends_on") == 0)   return SEA_REL_DEPENDS_ON;
    if (strcmp(s, "mentioned_in") == 0) return SEA_REL_MENTIONED_IN;
    if (strcmp(s, "related_to") == 0)   return SEA_REL_RELATED_TO;
    if (strcmp(s, "blocked_by") == 0)   return SEA_REL_BLOCKED_BY;
    if (strcmp(s, "assigned_to") == 0)  return SEA_REL_ASSIGNED_TO;
    return SEA_REL_CUSTOM;
}

static void cmd_graph(const char* input) {
    if (!s_graph) {
        printf("  Knowledge graph not initialized (needs database).\n");
        return;
    }

    /* /graph add <type> <name> [summary] */
    if (strncmp(input, "/graph add ", 11) == 0) {
        const char* p = input + 11;
        while (*p == ' ') p++;
        char type_str[32], name[SEA_ENTITY_NAME_MAX], summary[SEA_ENTITY_SUMMARY_MAX] = "";
        if (sscanf(p, "%31s %127[^ ] %511[^\n]", type_str, name, summary) < 2) {
            printf("  Usage: /graph add <type> <name> [summary]\n");
            printf("  Types: person, project, decision, commitment, topic, tool, location\n");
            return;
        }
        SeaEntityType etype = parse_entity_type(type_str);
        i32 id = sea_graph_entity_upsert(s_graph, etype, name, summary);
        if (id >= 0) {
            printf("  \033[32m✓\033[0m Entity #%d: %s (%s)\n", id, name, type_str);
        } else {
            printf("  \033[31m✗\033[0m Failed to add entity\n");
        }
        sea_arena_reset(&s_request_arena);
        return;
    }

    /* /graph link <from_name> <relation> <to_name> */
    if (strncmp(input, "/graph link ", 12) == 0) {
        const char* p = input + 12;
        while (*p == ' ') p++;
        char from[SEA_ENTITY_NAME_MAX], rel[64], to[SEA_ENTITY_NAME_MAX];
        if (sscanf(p, "%127s %63s %127s", from, rel, to) != 3) {
            printf("  Usage: /graph link <from> <relation> <to>\n");
            printf("  Relations: works_on, decided, owns, depends_on, related_to, blocked_by, assigned_to\n");
            return;
        }
        SeaGraphEntity* fe = sea_graph_entity_find(s_graph, from, &s_request_arena);
        SeaGraphEntity* te = sea_graph_entity_find(s_graph, to, &s_request_arena);
        if (!fe) { printf("  \033[31m✗\033[0m Entity '%s' not found\n", from); sea_arena_reset(&s_request_arena); return; }
        if (!te) { printf("  \033[31m✗\033[0m Entity '%s' not found\n", to); sea_arena_reset(&s_request_arena); return; }
        SeaRelType rtype = parse_rel_type(rel);
        i32 rid = sea_graph_relate(s_graph, fe->id, te->id, rtype, rel);
        if (rid >= 0) {
            printf("  \033[32m✓\033[0m Relation #%d: %s —[%s]→ %s\n", rid, from, rel, to);
        } else {
            printf("  \033[31m✗\033[0m Failed to create relation\n");
        }
        sea_arena_reset(&s_request_arena);
        return;
    }

    /* /graph search <query> */
    if (strncmp(input, "/graph search ", 14) == 0) {
        const char* query = input + 14;
        while (*query == ' ') query++;
        SeaGraphEntity results[SEA_GRAPH_MAX_RESULTS];
        u32 count = sea_graph_entity_search(s_graph, query, results,
                                             SEA_GRAPH_MAX_RESULTS, &s_request_arena);
        printf("\n  \033[1mGraph Search: \"%s\" (%u results):\033[0m\n", query, count);
        for (u32 i = 0; i < count; i++) {
            printf("    #%-4d [%-10s] %-20s  %s\n",
                   results[i].id, entity_type_str(results[i].type),
                   results[i].name,
                   results[i].summary[0] ? results[i].summary : "");
        }
        if (count == 0) printf("    (no matches)\n");
        printf("\n");
        sea_arena_reset(&s_request_arena);
        return;
    }

    /* /graph — show stats */
    u32 total = sea_graph_entity_count(s_graph);
    printf("\n  \033[1mKnowledge Graph:\033[0m\n");
    printf("    Entities: %u\n", total);

    /* Count by type */
    static const SeaEntityType types[] = {
        SEA_ENTITY_PERSON, SEA_ENTITY_PROJECT, SEA_ENTITY_DECISION,
        SEA_ENTITY_COMMITMENT, SEA_ENTITY_TOPIC, SEA_ENTITY_TOOL,
        SEA_ENTITY_LOCATION, SEA_ENTITY_CUSTOM
    };
    for (u32 i = 0; i < 8; i++) {
        SeaGraphEntity tmp[1];
        u32 c = sea_graph_entity_list(s_graph, types[i], tmp, 0, &s_request_arena);
        if (c > 0) {
            printf("      %-12s %u\n", entity_type_str(types[i]), c);
        }
    }
    printf("\n  Commands:\n");
    printf("    /graph add <type> <name> [summary]\n");
    printf("    /graph link <from> <relation> <to>\n");
    printf("    /graph search <query>\n");
    printf("\n");
    sea_arena_reset(&s_request_arena);
}

/* ── TUI: /cron ──────────────────────────────────────────── */

static const char* cron_state_str(SeaCronJobState st) {
    switch (st) {
        case SEA_CRON_ACTIVE:    return "\033[32mactive\033[0m";
        case SEA_CRON_PAUSED:    return "\033[33mpaused\033[0m";
        case SEA_CRON_COMPLETED: return "\033[36mdone\033[0m";
        case SEA_CRON_FAILED:    return "\033[31mfailed\033[0m";
    }
    return "?";
}

static void cmd_cron(const char* input) {
    if (!s_cron) {
        printf("  Cron scheduler not initialized.\n");
        return;
    }

    /* /cron add <name> <schedule> <command> */
    if (strncmp(input, "/cron add ", 10) == 0) {
        const char* p = input + 10;
        while (*p == ' ') p++;
        char name[SEA_CRON_NAME_MAX], sched[SEA_CRON_EXPR_MAX];
        char cmd_buf[SEA_CRON_CMD_MAX] = "";
        if (sscanf(p, "%63s %63s %511[^\n]", name, sched, cmd_buf) < 2) {
            printf("  Usage: /cron add <name> <schedule> <command>\n");
            printf("  Schedule: '@every 5m', '@once 30s', '0 9 * * *'\n");
            return;
        }
        i32 id = sea_cron_add(s_cron, name, SEA_CRON_SHELL, sched, cmd_buf, NULL);
        if (id >= 0) {
            printf("  \033[32m✓\033[0m Job #%d '%s' added (schedule: %s)\n", id, name, sched);
        } else {
            printf("  \033[31m✗\033[0m Failed to add job\n");
        }
        return;
    }

    /* /cron pause <id> */
    if (strncmp(input, "/cron pause ", 12) == 0) {
        i32 id = atoi(input + 12);
        SeaError err = sea_cron_pause(s_cron, id);
        if (err == SEA_OK) printf("  \033[32m✓\033[0m Job #%d paused\n", id);
        else printf("  \033[31m✗\033[0m %s\n", sea_error_str(err));
        return;
    }

    /* /cron resume <id> */
    if (strncmp(input, "/cron resume ", 13) == 0) {
        i32 id = atoi(input + 13);
        SeaError err = sea_cron_resume(s_cron, id);
        if (err == SEA_OK) printf("  \033[32m✓\033[0m Job #%d resumed\n", id);
        else printf("  \033[31m✗\033[0m %s\n", sea_error_str(err));
        return;
    }

    /* /cron remove <id> */
    if (strncmp(input, "/cron remove ", 13) == 0) {
        i32 id = atoi(input + 13);
        SeaError err = sea_cron_remove(s_cron, id);
        if (err == SEA_OK) printf("  \033[32m✓\033[0m Job #%d removed\n", id);
        else printf("  \033[31m✗\033[0m %s\n", sea_error_str(err));
        return;
    }

    /* /cron — list jobs */
    u32 count = sea_cron_count(s_cron);
    printf("\n  \033[1mCron Jobs (%u):\033[0m\n", count);
    if (count == 0) {
        printf("    (none — add with /cron add <name> <schedule> <command>)\n");
    }
    SeaCronJob* jobs[SEA_MAX_CRON_JOBS];
    u32 listed = sea_cron_list(s_cron, jobs, SEA_MAX_CRON_JOBS);
    for (u32 i = 0; i < listed; i++) {
        SeaCronJob* j = jobs[i];
        printf("    #%-3d %-16s  %s  sched: %-16s  runs: %u  cmd: %.40s%s\n",
               j->id, j->name, cron_state_str(j->state),
               j->schedule, j->run_count,
               j->command, strlen(j->command) > 40 ? "..." : "");
    }
    printf("\n");
}

/* ── TUI: /recall ────────────────────────────────────────── */

static void cmd_recall(const char* input) {
    if (!s_recall) {
        printf("  Recall engine not initialized (needs database).\n");
        return;
    }

    /* /recall store <category> <content> */
    if (strncmp(input, "/recall store ", 14) == 0) {
        const char* p = input + 14;
        while (*p == ' ') p++;
        char category[64];
        char content[1024] = "";
        if (sscanf(p, "%63s %1023[^\n]", category, content) < 2) {
            printf("  Usage: /recall store <category> <content>\n");
            printf("  Categories: user, preference, fact, rule, context, identity\n");
            return;
        }
        SeaError err = sea_recall_store(s_recall, category, content, NULL, 5);
        if (err == SEA_OK) {
            printf("  \033[32m✓\033[0m Stored fact in '%s'\n", category);
        } else {
            printf("  \033[31m✗\033[0m %s\n", sea_error_str(err));
        }
        return;
    }

    /* /recall forget <id> */
    if (strncmp(input, "/recall forget ", 15) == 0) {
        i32 id = atoi(input + 15);
        SeaError err = sea_recall_forget(s_recall, id);
        if (err == SEA_OK) printf("  \033[32m✓\033[0m Fact #%d forgotten\n", id);
        else printf("  \033[31m✗\033[0m %s\n", sea_error_str(err));
        return;
    }

    /* /recall search <query> */
    if (strncmp(input, "/recall search ", 15) == 0) {
        const char* query = input + 15;
        while (*query == ' ') query++;
        SeaRecallFact facts[16];
        i32 count = sea_recall_query(s_recall, query, facts, 16, &s_request_arena);
        printf("\n  \033[1mRecall: \"%s\" (%d results):\033[0m\n", query, count);
        for (i32 i = 0; i < count; i++) {
            printf("    #%-4d [%-10s] score:%.2f  %s\n",
                   facts[i].id,
                   facts[i].category ? facts[i].category : "?",
                   facts[i].score,
                   facts[i].content ? facts[i].content : "");
        }
        if (count == 0) printf("    (no matches)\n");
        printf("\n");
        sea_arena_reset(&s_request_arena);
        return;
    }

    /* /recall — show stats */
    u32 total = sea_recall_count(s_recall);
    printf("\n  \033[1mRecall Memory (The Vault):\033[0m\n");
    printf("    Total facts:  %u\n", total);
    printf("    Token budget: %u\n", s_recall->max_context_tokens);

    static const char* categories[] = {
        "user", "preference", "fact", "rule", "context", "identity"
    };
    for (u32 i = 0; i < 6; i++) {
        u32 c = sea_recall_count_category(s_recall, categories[i]);
        if (c > 0) printf("      %-12s %u\n", categories[i], c);
    }
    printf("\n  Commands:\n");
    printf("    /recall search <query>\n");
    printf("    /recall store <category> <content>\n");
    printf("    /recall forget <id>\n");
    printf("\n");
}

/* ── TUI: /mesh ──────────────────────────────────────────── */

static void cmd_mesh(void) {
    if (!s_mesh) {
        printf("  Mesh not enabled. Use --mode captain|crew\n");
        return;
    }
    const char* status = sea_mesh_status(s_mesh, &s_request_arena);
    printf("\n  %s\n", status);
    sea_arena_reset(&s_request_arena);
}

static void dispatch_command(const char* input) {
    if (strcmp(input, "/help") == 0 || strcmp(input, "/?") == 0) {
        cmd_help();
    } else if (strcmp(input, "/status") == 0) {
        cmd_status();
    } else if (strcmp(input, "/tools") == 0) {
        printf("\n");
        sea_tools_list();
        printf("\n");
    } else if (strncmp(input, "/exec ", 6) == 0) {
        cmd_exec(input);
    } else if (strcmp(input, "/tasks") == 0) {
        if (!s_db) { printf("  No database loaded.\n"); }
        else {
            SeaDbTask tasks[32];
            i32 count = sea_db_task_list(s_db, NULL, tasks, 32, &s_request_arena);
            printf("\n  \033[1mTasks (%d):\033[0m\n", count);
            for (i32 i = 0; i < count; i++) {
                const char* icon = "○";
                if (strcmp(tasks[i].status, "completed") == 0) icon = "\033[32m✓\033[0m";
                else if (strcmp(tasks[i].status, "in_progress") == 0) icon = "\033[33m►\033[0m";
                printf("    %s [%s] %s\n", icon, tasks[i].priority, tasks[i].title);
            }
            printf("\n");
            sea_arena_reset(&s_request_arena);
        }
    } else if (strcmp(input, "/agents") == 0) {
        if (!s_db) { printf("  No database loaded.\n"); }
        else {
            SeaDbAgent agents[16];
            i32 ac = sea_db_sz_agent_list(s_db, agents, 16, &s_request_arena);
            printf("\n  \033[1mAgent Zero Instances (%d):\033[0m\n", ac);
            if (ac == 0) {
                printf("    (none registered — enable with 'make seazero-setup')\n");
            }
            for (i32 i = 0; i < ac; i++) {
                const char* icon = "\033[32m●\033[0m";
                if (strcmp(agents[i].status, "offline") == 0) icon = "\033[31m●\033[0m";
                else if (strcmp(agents[i].status, "busy") == 0) icon = "\033[33m●\033[0m";
                printf("    %s %s  port:%d  %s/%s  [%s]\n",
                       icon, agents[i].agent_id, agents[i].port,
                       agents[i].provider ? agents[i].provider : "?",
                       agents[i].model ? agents[i].model : "?",
                       agents[i].status);
            }
            printf("\n");
            sea_arena_reset(&s_request_arena);
        }
    } else if (strncmp(input, "/delegate ", 10) == 0) {
        const char* task_desc = input + 10;
        if (!task_desc[0]) { printf("  Usage: /delegate <task description>\n"); }
        else if (!s_db) { printf("  No database loaded.\n"); }
        else {
            printf("\n  \033[33m⟳\033[0m Delegating to Agent Zero: %.60s%s\n",
                   task_desc, strlen(task_desc) > 60 ? "..." : "");

            SeaZeroConfig zcfg = {0};
            SeaArena sz_arena;
            sea_arena_create(&sz_arena, 4096);
            const char* sz_url = sea_db_config_get(s_db, "seazero_agent_url", &sz_arena);
            sea_zero_init(&zcfg, sz_url);

            SeaZeroTask ztask = { .task = task_desc, .max_steps = 10 };
            SeaZeroResult zres = sea_zero_delegate(&zcfg, &ztask, &s_request_arena);

            if (zres.success) {
                printf("  \033[32m✓\033[0m Result:\n\n%s\n\n", zres.result);
            } else {
                printf("  \033[31m✗\033[0m %s\n\n", zres.error ? zres.error : "Unknown error");
            }
            sea_arena_destroy(&sz_arena);
            sea_arena_reset(&s_request_arena);
        }
    } else if (strcmp(input, "/sztasks") == 0) {
        if (!s_db) { printf("  No database loaded.\n"); }
        else {
            SeaDbSzTask sztasks[32];
            i32 tc = sea_db_sz_task_list(s_db, NULL, sztasks, 32, &s_request_arena);
            printf("\n  \033[1mSeaZero Tasks (%d):\033[0m\n", tc);
            if (tc == 0) {
                printf("    (no delegated tasks yet)\n");
            }
            for (i32 i = 0; i < tc; i++) {
                const char* icon = "○";
                if (strcmp(sztasks[i].status, "completed") == 0) icon = "\033[32m✓\033[0m";
                else if (strcmp(sztasks[i].status, "running") == 0) icon = "\033[33m►\033[0m";
                else if (strcmp(sztasks[i].status, "failed") == 0) icon = "\033[31m✗\033[0m";
                printf("    %s [%s] %s → %.50s%s\n",
                       icon, sztasks[i].status, sztasks[i].agent_id,
                       sztasks[i].task_text ? sztasks[i].task_text : "",
                       (sztasks[i].task_text && strlen(sztasks[i].task_text) > 50) ? "..." : "");
            }
            printf("\n");
            sea_arena_reset(&s_request_arena);
        }
    } else if (strcmp(input, "/usage") == 0) {
        if (!s_db) { printf("  No database loaded.\n"); }
        else {
            i64 sc_tokens = sea_db_sz_llm_total_tokens(s_db, "seaclaw");
            i64 az_tokens = sea_db_sz_llm_total_tokens(s_db, "agent-zero");
            i64 total = sc_tokens + az_tokens;
            printf("\n  \033[1mLLM Token Usage:\033[0m\n");
            printf("    SeaClaw:     %lld tokens\n", (long long)sc_tokens);
            printf("    Agent Zero:  %lld tokens\n", (long long)az_tokens);
            printf("    ─────────────────────\n");
            printf("    Total:       %lld tokens\n", (long long)total);
            if (total > 0) {
                printf("    Split:       %.0f%% SeaClaw / %.0f%% Agent Zero\n",
                       (double)sc_tokens / (double)total * 100.0,
                       (double)az_tokens / (double)total * 100.0);
            }
            printf("\n");
        }
    } else if (strcmp(input, "/audit") == 0) {
        if (!s_db) { printf("  No database loaded.\n"); }
        else {
            SeaDbAuditEvent events[20];
            i32 ec = sea_db_sz_audit_list(s_db, events, 20, &s_request_arena);
            printf("\n  \033[1mRecent Audit Events (%d):\033[0m\n", ec);
            if (ec == 0) {
                printf("    (no events)\n");
            }
            for (i32 i = 0; i < ec; i++) {
                const char* icon = "·";
                if (events[i].severity && strcmp(events[i].severity, "warn") == 0) icon = "\033[33m⚠\033[0m";
                else if (events[i].severity && strcmp(events[i].severity, "error") == 0) icon = "\033[31m✗\033[0m";
                printf("    %s [%s] %s → %s",
                       icon,
                       events[i].event_type ? events[i].event_type : "?",
                       events[i].source ? events[i].source : "?",
                       events[i].target ? events[i].target : "-");
                if (events[i].created_at) printf("  (%s)", events[i].created_at);
                printf("\n");
            }
            printf("\n");
            sea_arena_reset(&s_request_arena);
        }
    } else if (strcmp(input, "/skills") == 0) {
        cmd_skills();
    } else if (strcmp(input, "/auth") == 0 || strncmp(input, "/auth ", 6) == 0) {
        cmd_auth(input);
    } else if (strcmp(input, "/heartbeat") == 0) {
        cmd_heartbeat();
    } else if (strcmp(input, "/graph") == 0 || strncmp(input, "/graph ", 7) == 0) {
        cmd_graph(input);
    } else if (strcmp(input, "/cron") == 0 || strncmp(input, "/cron ", 6) == 0) {
        cmd_cron(input);
    } else if (strcmp(input, "/recall") == 0 || strncmp(input, "/recall ", 8) == 0) {
        cmd_recall(input);
    } else if (strcmp(input, "/mesh") == 0) {
        cmd_mesh();
    } else if (strcmp(input, "/config") == 0) {
        cmd_config();
    } else if (strcmp(input, "/model") == 0 || strncmp(input, "/model ", 7) == 0) {
        cmd_model(input);
    } else if (strcmp(input, "/provider") == 0 || strncmp(input, "/provider ", 10) == 0) {
        cmd_provider(input);
    } else if (strcmp(input, "/stream") == 0 || strncmp(input, "/stream ", 8) == 0) {
        cmd_stream(input);
    } else if (strcmp(input, "/think") == 0 || strncmp(input, "/think ", 7) == 0) {
        cmd_think(input);
    } else if (strcmp(input, "/clear") == 0) {
        printf("\033[2J\033[H");
        printf("%s\n", BANNER);
    } else if (strcmp(input, "/quit") == 0 || strcmp(input, "/q") == 0) {
        s_running = false;
    } else if (input[0] == '/') {
        printf("  Unknown command: %s (type /help)\n", input);
    } else {
        /* Natural language input */
        SeaSlice input_slice = { .data = (const u8*)input, .len = (u32)strlen(input) };

        /* Shield: validate input grammar */
        u64 t0 = sea_log_elapsed_ms();
        printf("\n  \033[33m[SHIELD]\033[0m Validating input grammar... ");

        if (sea_shield_detect_injection(input_slice)) {
            printf("\033[31mREJECTED\033[0m (injection detected)\n\n");
            sea_arena_reset(&s_request_arena);
            return;
        }
        if (!sea_shield_check(input_slice, SEA_GRAMMAR_SAFE_TEXT)) {
            printf("\033[31mREJECTED\033[0m (invalid characters)\n\n");
            sea_arena_reset(&s_request_arena);
            return;
        }
        printf("OK\n");

        printf("  \033[36m[BRAIN]\033[0m Processing: \"%s\"\n", input);

        if (s_agent_cfg.api_key || s_agent_cfg.provider == SEA_LLM_LOCAL) {
            /* Load conversation history from DB */
            SeaDbChatMsg db_msgs[20];
            i32 hist_count = 0;
            if (s_db) {
                hist_count = sea_db_chat_history(s_db, 0, db_msgs, 20,
                                                 &s_request_arena);
            }

            /* Convert DB messages to agent format */
            SeaChatMsg history[20];
            for (i32 i = 0; i < hist_count; i++) {
                if (strcmp(db_msgs[i].role, "assistant") == 0)
                    history[i].role = SEA_ROLE_ASSISTANT;
                else
                    history[i].role = SEA_ROLE_USER;
                history[i].content = db_msgs[i].content;
                history[i].tool_call_id = NULL;
                history[i].tool_name = NULL;
            }

            /* Route through LLM agent with history */
            SeaAgentResult ar = sea_agent_chat(&s_agent_cfg,
                                               history, (u32)hist_count,
                                               input, &s_request_arena);
            if (ar.error == SEA_OK && ar.text) {
                printf("\n  %s\n", ar.text);
                if (ar.tool_calls > 0) {
                    printf("  \033[33m(%u tool call%s)\033[0m\n",
                           ar.tool_calls, ar.tool_calls > 1 ? "s" : "");
                }
                /* Save to conversation memory */
                if (s_db) {
                    sea_db_chat_log(s_db, 0, "user", input);
                    sea_db_chat_log(s_db, 0, "assistant", ar.text);
                }
            } else {
                printf("  \033[31m[ERROR]\033[0m %s\n",
                       ar.text ? ar.text : "Agent failed");
            }
        } else {
            /* No LLM configured — echo as fallback */
            (void)input_slice;
            printf("  \033[33m[BRAIN]\033[0m No LLM configured. Set llm_api_key in config.json\n");
            printf("  \033[37m[ECHO]\033[0m %s\n", input);
        }
        u64 t1 = sea_log_elapsed_ms();
        printf("  \033[37m[CORE]\033[0m Arena reset. (%lums)\n\n", (unsigned long)(t1 - t0));
        sea_arena_reset(&s_request_arena);
    }
}

/* ── Telegram message handler ───────────────────────────── */

static SeaError telegram_handler(i64 chat_id, SeaSlice text,
                                  SeaArena* arena, SeaSlice* response) {
    /* Shield check */
    if (sea_shield_detect_injection(text)) {
        *response = SEA_SLICE_LIT("Rejected: injection detected.");
        return SEA_OK;
    }

    /* Check if it's a command */
    if (text.len > 0 && text.data[0] == '/') {
        /* Helper: extract rest of command after prefix */
        #define CMD_REST(prefix) \
            (SeaSlice){ .data = text.data + sizeof(prefix) - 1, \
                        .len  = text.len  - (u32)(sizeof(prefix) - 1) }

        if (sea_slice_eq_cstr(text, "/status")) {
            SeaSlice args = SEA_SLICE_EMPTY;
            return sea_tool_exec("system_status", args, arena, response);
        }

        /* /tools — list all registered tools */
        if (sea_slice_eq_cstr(text, "/tools")) {
            u32 tc = sea_tools_count();
            char buf[1024];
            int pos = snprintf(buf, sizeof(buf), "Tools (%u):\n", tc);
            for (u32 i = 0; i < tc && pos < (int)sizeof(buf) - 80; i++) {
                const SeaTool* t = sea_tool_by_id(i + 1);
                if (t) pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos,
                    "  /%s — %s\n", t->name, t->description);
            }
            u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)pos);
            if (dst) { response->data = dst; response->len = (u32)pos; }
            return SEA_OK;
        }

        /* /help — full command reference */
        if (sea_slice_eq_cstr(text, "/help")) {
            *response = SEA_SLICE_LIT(
                "Sea-Claw v" SEA_VERSION_STRING " Commands:\n\n"
                "/status — System status & memory\n"
                "/tools — List all tools\n"
                "/task list — List tasks\n"
                "/task create <title> — Create task\n"
                "/task done <id> — Complete task\n"
                "/file read <path> — Read a file\n"
                "/file write <path> — Write (next msg = content)\n"
                "/shell <command> — Run shell command\n"
                "/web <url> — Fetch URL content\n"
                "/session clear — Clear chat history\n"
                "/compact — Summarize & prune chat history\n"
                "/model — Show current LLM model\n"
                "/model set <name> — Hot-swap LLM model\n"
                "/think [off|low|medium|high] — Set thinking level\n"
                "/usage — Token spend dashboard\n"
                "/pii [on|off] — Toggle PII firewall\n"
                "/mesh — Show mesh network status\n"
                "/audit — View recent audit trail\n"
                "/delegate <url> <task> — Delegate to remote agent\n"
                "/exec <tool> <args> — Raw tool exec\n\n"
                "Or just type naturally — I'll use AI + tools.");
            return SEA_OK;
        }

        /* /task list | /task create <title> | /task done <id> */
        if (text.len >= 5 && memcmp(text.data, "/task", 5) == 0) {
            SeaSlice args;
            if (text.len == 5 || sea_slice_eq_cstr(text, "/task list")) {
                args = SEA_SLICE_LIT("list");
            } else if (text.len > 13 && memcmp(text.data, "/task create ", 13) == 0) {
                /* Build: create|<title> */
                const char* title = (const char*)text.data + 13;
                u32 tlen = text.len - 13;
                char tmp[512];
                int n = snprintf(tmp, sizeof(tmp), "create|%.*s", (int)tlen, title);
                args = (SeaSlice){ .data = (const u8*)sea_arena_push_bytes(arena, tmp, (u64)n),
                                   .len = (u32)n };
            } else if (text.len > 11 && memcmp(text.data, "/task done ", 11) == 0) {
                const char* id_str = (const char*)text.data + 11;
                u32 ilen = text.len - 11;
                char tmp[64];
                int n = snprintf(tmp, sizeof(tmp), "done|%.*s", (int)ilen, id_str);
                args = (SeaSlice){ .data = (const u8*)sea_arena_push_bytes(arena, tmp, (u64)n),
                                   .len = (u32)n };
            } else {
                *response = SEA_SLICE_LIT("Usage: /task list | /task create <title> | /task done <id>");
                return SEA_OK;
            }
            return sea_tool_exec("task_manage", args, arena, response);
        }

        /* /file read <path> | /file write <path>|<content> */
        if (text.len > 5 && memcmp(text.data, "/file", 5) == 0) {
            if (text.len > 11 && memcmp(text.data, "/file read ", 11) == 0) {
                SeaSlice args = { .data = text.data + 11, .len = text.len - 11 };
                return sea_tool_exec("file_read", args, arena, response);
            }
            if (text.len > 12 && memcmp(text.data, "/file write ", 12) == 0) {
                SeaSlice args = { .data = text.data + 12, .len = text.len - 12 };
                return sea_tool_exec("file_write", args, arena, response);
            }
            *response = SEA_SLICE_LIT("Usage: /file read <path> | /file write <path>|<content>");
            return SEA_OK;
        }

        /* /shell <command> */
        if (text.len > 7 && memcmp(text.data, "/shell ", 7) == 0) {
            SeaSlice args = { .data = text.data + 7, .len = text.len - 7 };
            return sea_tool_exec("shell_exec", args, arena, response);
        }

        /* /web <url> */
        if (text.len > 5 && memcmp(text.data, "/web ", 5) == 0) {
            SeaSlice args = { .data = text.data + 5, .len = text.len - 5 };
            return sea_tool_exec("web_fetch", args, arena, response);
        }

        /* /session clear */
        if (sea_slice_eq_cstr(text, "/session clear")) {
            if (s_db) {
                sea_db_chat_clear(s_db, chat_id);
                *response = SEA_SLICE_LIT("Chat history cleared.");
            } else {
                *response = SEA_SLICE_LIT("No database available.");
            }
            return SEA_OK;
        }

        /* /audit — show recent audit trail */
        if (sea_slice_eq_cstr(text, "/audit")) {
            if (!s_db) {
                *response = SEA_SLICE_LIT("No database available.");
                return SEA_OK;
            }
            SeaDbEvent events[10];
            i32 ec = sea_db_recent_events(s_db, events, 10, arena);
            if (ec == 0) {
                *response = SEA_SLICE_LIT("No audit events yet.");
                return SEA_OK;
            }
            char buf[2048];
            int pos = snprintf(buf, sizeof(buf), "Audit Trail (last %d):\n", ec);
            for (i32 i = 0; i < ec && pos < (int)sizeof(buf) - 120; i++) {
                pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos,
                    "\n[%s] %s: %s\n  %s",
                    events[i].created_at ? events[i].created_at : "?",
                    events[i].entry_type ? events[i].entry_type : "?",
                    events[i].title ? events[i].title : "?",
                    events[i].content ? events[i].content : "");
            }
            u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)pos);
            if (dst) { response->data = dst; response->len = (u32)pos; }
            return SEA_OK;
        }

        /* /model — show current model info */
        if (sea_slice_eq_cstr(text, "/model")) {
            char buf[256];
            int n = snprintf(buf, sizeof(buf),
                "Model: %s\nProvider: %s\nAPI URL: %s",
                s_agent_cfg.model ? s_agent_cfg.model : "(default)",
                s_agent_cfg.provider == SEA_LLM_LOCAL ? "local" :
                s_agent_cfg.provider == SEA_LLM_ANTHROPIC ? "anthropic" : "openai",
                s_agent_cfg.api_url ? s_agent_cfg.api_url : "(default)");
            u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)n);
            if (dst) { response->data = dst; response->len = (u32)n; }
            return SEA_OK;
        }

        /* /delegate <endpoint> <task> — delegate task to remote agent */
        if (text.len > 10 && memcmp(text.data, "/delegate ", 10) == 0) {
            const char* rest = (const char*)text.data + 10;
            u32 rlen = text.len - 10;
            /* Parse: endpoint task_description */
            u32 i = 0;
            while (i < rlen && rest[i] != ' ') i++;
            if (i == 0 || i >= rlen) {
                *response = SEA_SLICE_LIT("Usage: /delegate <endpoint> <task description>");
                return SEA_OK;
            }
            char* ep = (char*)sea_arena_alloc(arena, i + 1, 1);
            if (!ep) { *response = SEA_SLICE_LIT("Memory error."); return SEA_ERR_OOM; }
            memcpy(ep, rest, i); ep[i] = '\0';
            while (i < rlen && rest[i] == ' ') i++;
            char* task = (char*)sea_arena_alloc(arena, rlen - i + 1, 1);
            if (!task) { *response = SEA_SLICE_LIT("Memory error."); return SEA_ERR_OOM; }
            memcpy(task, rest + i, rlen - i); task[rlen - i] = '\0';

            SeaA2aPeer peer = { .name = ep, .endpoint = ep, .api_key = NULL,
                                .healthy = false, .last_seen = 0 };
            SeaA2aRequest req = { .task_id = NULL, .task_desc = task,
                                  .context = NULL, .timeout_ms = 30000 };
            SeaA2aResult ar = sea_a2a_delegate(&peer, &req, arena);

            char buf[2048];
            int n;
            if (ar.success && ar.output) {
                n = snprintf(buf, sizeof(buf),
                    "A2A Result from %s:\n%s\n[%ums, verified=%s]",
                    ar.agent_name ? ar.agent_name : "unknown",
                    ar.output,
                    ar.latency_ms,
                    ar.verified ? "yes" : "no");
            } else {
                n = snprintf(buf, sizeof(buf),
                    "A2A Delegation failed: %s",
                    ar.error ? ar.error : "unknown error");
            }
            u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)n);
            if (dst) { response->data = dst; response->len = (u32)n; }
            return SEA_OK;
        }

        /* /compact — summarize + prune chat history */
        if (sea_slice_eq_cstr(text, "/compact")) {
            if (!s_db) {
                *response = SEA_SLICE_LIT("No database available.");
                return SEA_OK;
            }
            SeaDbChatMsg db_msgs[50];
            i32 hist_count = sea_db_chat_history(s_db, chat_id, db_msgs, 50, arena);
            if (hist_count < 4) {
                *response = SEA_SLICE_LIT("Not enough history to compact (need 4+ messages).");
                return SEA_OK;
            }
            /* Convert to agent format */
            SeaChatMsg history[50];
            for (i32 i = 0; i < hist_count; i++) {
                history[i].role = strcmp(db_msgs[i].role, "assistant") == 0
                    ? SEA_ROLE_ASSISTANT : SEA_ROLE_USER;
                history[i].content = db_msgs[i].content;
                history[i].tool_call_id = NULL;
                history[i].tool_name = NULL;
            }
            const char* summary = sea_agent_compact(&s_agent_cfg, history,
                                                     (u32)hist_count, arena);
            if (summary) {
                sea_db_chat_clear(s_db, chat_id);
                sea_db_chat_log(s_db, chat_id, "system", summary);
                char buf[2048];
                int n = snprintf(buf, sizeof(buf),
                    "Compacted %d messages into summary.\n\n%s", hist_count, summary);
                u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)n);
                if (dst) { response->data = dst; response->len = (u32)n; }
            } else {
                *response = SEA_SLICE_LIT("Compaction failed — LLM unavailable.");
            }
            return SEA_OK;
        }

        /* /think [off|low|medium|high] — set thinking level */
        if (text.len >= 6 && memcmp(text.data, "/think", 6) == 0) {
            if (text.len == 6 || text.data[6] == ' ') {
                const char* level_str = (text.len > 7) ? (const char*)text.data + 7 : NULL;
                u32 llen = level_str ? text.len - 7 : 0;
                if (!level_str) {
                    char buf[128];
                    int n = snprintf(buf, sizeof(buf), "Think level: %s (temp=%.1f, max_tokens=%u)",
                        sea_agent_think_level_name(s_agent_cfg.think_level),
                        s_agent_cfg.temperature, s_agent_cfg.max_tokens);
                    u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)n);
                    if (dst) { response->data = dst; response->len = (u32)n; }
                } else if (llen == 3 && memcmp(level_str, "off", 3) == 0) {
                    sea_agent_set_think_level(&s_agent_cfg, SEA_THINK_OFF);
                    *response = SEA_SLICE_LIT("Think level: off (fast, temp=0.3)");
                } else if (llen == 3 && memcmp(level_str, "low", 3) == 0) {
                    sea_agent_set_think_level(&s_agent_cfg, SEA_THINK_LOW);
                    *response = SEA_SLICE_LIT("Think level: low (brief, temp=0.5)");
                } else if (llen == 6 && memcmp(level_str, "medium", 6) == 0) {
                    sea_agent_set_think_level(&s_agent_cfg, SEA_THINK_MEDIUM);
                    *response = SEA_SLICE_LIT("Think level: medium (balanced, temp=0.7)");
                } else if (llen == 4 && memcmp(level_str, "high", 4) == 0) {
                    sea_agent_set_think_level(&s_agent_cfg, SEA_THINK_HIGH);
                    *response = SEA_SLICE_LIT("Think level: high (deep, temp=0.9)");
                } else {
                    *response = SEA_SLICE_LIT("Usage: /think [off|low|medium|high]");
                }
                return SEA_OK;
            }
        }

        /* /model [set <name>] — show or hot-swap model */
        if (text.len >= 6 && memcmp(text.data, "/model", 6) == 0) {
            if (text.len > 11 && memcmp(text.data + 6, " set ", 5) == 0) {
                /* Hot-swap model */
                const char* new_model = (const char*)text.data + 11;
                u32 mlen = text.len - 11;
                char* model_copy = (char*)sea_arena_alloc(arena, mlen + 1, 1);
                if (model_copy) {
                    memcpy(model_copy, new_model, mlen);
                    model_copy[mlen] = '\0';
                    sea_agent_set_model(&s_agent_cfg, model_copy);
                    char buf[256];
                    int n = snprintf(buf, sizeof(buf), "Model switched to: %s", model_copy);
                    u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)n);
                    if (dst) { response->data = dst; response->len = (u32)n; }
                }
                return SEA_OK;
            }
            /* Show current model */
            char buf[512];
            int n = snprintf(buf, sizeof(buf),
                "Model: %s\nProvider: %s\nAPI URL: %s\nThink: %s\nPII: %s",
                s_agent_cfg.model ? s_agent_cfg.model : "(default)",
                s_agent_cfg.provider == SEA_LLM_LOCAL ? "local" :
                s_agent_cfg.provider == SEA_LLM_ANTHROPIC ? "anthropic" :
                s_agent_cfg.provider == SEA_LLM_GEMINI ? "gemini" :
                s_agent_cfg.provider == SEA_LLM_OPENROUTER ? "openrouter" : "openai",
                s_agent_cfg.api_url ? s_agent_cfg.api_url : "(default)",
                sea_agent_think_level_name(s_agent_cfg.think_level),
                s_agent_cfg.pii_categories ? "enabled" : "disabled");
            u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)n);
            if (dst) { response->data = dst; response->len = (u32)n; }
            return SEA_OK;
        }

        /* /usage — token spend dashboard */
        if (sea_slice_eq_cstr(text, "/usage")) {
            if (!s_usage) {
                *response = SEA_SLICE_LIT("Usage tracking not available.");
                return SEA_OK;
            }
            char buf[2048];
            u32 slen = sea_usage_summary(s_usage, buf, sizeof(buf));
            if (slen > 0) {
                u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)slen);
                if (dst) { response->data = dst; response->len = slen; }
            } else {
                *response = SEA_SLICE_LIT("No usage data yet.");
            }
            return SEA_OK;
        }

        /* /pii [on|off] — toggle PII firewall */
        if (text.len >= 4 && memcmp(text.data, "/pii", 4) == 0) {
            if (text.len > 5) {
                const char* arg = (const char*)text.data + 5;
                u32 alen = text.len - 5;
                if (alen == 2 && memcmp(arg, "on", 2) == 0) {
                    s_agent_cfg.pii_categories = SEA_PII_ALL;
                    *response = SEA_SLICE_LIT("PII Firewall: ENABLED (email, phone, SSN, CC, IP)");
                } else if (alen == 3 && memcmp(arg, "off", 3) == 0) {
                    s_agent_cfg.pii_categories = 0;
                    *response = SEA_SLICE_LIT("PII Firewall: DISABLED");
                } else {
                    *response = SEA_SLICE_LIT("Usage: /pii [on|off]");
                }
            } else {
                char buf[128];
                int n = snprintf(buf, sizeof(buf), "PII Firewall: %s",
                    s_agent_cfg.pii_categories ? "ENABLED" : "DISABLED");
                u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)n);
                if (dst) { response->data = dst; response->len = (u32)n; }
            }
            return SEA_OK;
        }

        /* /mesh — show mesh status */
        if (sea_slice_eq_cstr(text, "/mesh")) {
            if (!s_mesh) {
                *response = SEA_SLICE_LIT("Mesh not enabled. Use --mode captain|crew");
                return SEA_OK;
            }
            const char* status = sea_mesh_status(s_mesh, arena);
            response->data = (const u8*)status;
            response->len = (u32)strlen(status);
            return SEA_OK;
        }

        /* /exec <tool> <args> — raw tool execution */
        if (text.len > 6 && memcmp(text.data, "/exec ", 6) == 0) {
            const u8* rest = text.data + 6;
            u32 rlen = text.len - 6;
            char tool_name[SEA_MAX_TOOL_NAME];
            u32 i = 0;
            while (i < rlen && rest[i] != ' ' && i < SEA_MAX_TOOL_NAME - 1) {
                tool_name[i] = (char)rest[i];
                i++;
            }
            tool_name[i] = '\0';
            while (i < rlen && rest[i] == ' ') i++;
            SeaSlice args = { .data = rest + i, .len = rlen - i };
            return sea_tool_exec(tool_name, args, arena, response);
        }

        #undef CMD_REST
        *response = SEA_SLICE_LIT("Unknown command. Type /help");
        return SEA_OK;
    }

    /* Natural language — route through LLM agent */
    if (s_agent_cfg.api_key || s_agent_cfg.provider == SEA_LLM_LOCAL) {
        /* Null-terminate the input for the agent */
        char* input = (char*)sea_arena_alloc(arena, text.len + 1, 1);
        if (!input) {
            *response = SEA_SLICE_LIT("Memory error.");
            return SEA_ERR_OOM;
        }
        memcpy(input, text.data, text.len);
        input[text.len] = '\0';

        /* Load conversation history from DB */
        SeaDbChatMsg db_msgs[20];
        i32 hist_count = 0;
        if (s_db) {
            hist_count = sea_db_chat_history(s_db, chat_id, db_msgs, 20, arena);
        }

        /* Convert DB messages to agent format */
        SeaChatMsg history[20];
        for (i32 i = 0; i < hist_count; i++) {
            if (strcmp(db_msgs[i].role, "assistant") == 0)
                history[i].role = SEA_ROLE_ASSISTANT;
            else
                history[i].role = SEA_ROLE_USER;
            history[i].content = db_msgs[i].content;
            history[i].tool_call_id = NULL;
            history[i].tool_name = NULL;
        }

        SeaAgentResult ar = sea_agent_chat(&s_agent_cfg,
                                            history, (u32)hist_count,
                                            input, arena);
        if (ar.error == SEA_OK && ar.text) {
            response->data = (const u8*)ar.text;
            response->len  = (u32)strlen(ar.text);
            /* Save to conversation memory */
            if (s_db) {
                sea_db_chat_log(s_db, chat_id, "user", input);
                sea_db_chat_log(s_db, chat_id, "assistant", ar.text);
            }
        } else {
            const char* err_msg = ar.text ? ar.text : "Agent error.";
            response->data = (const u8*)err_msg;
            response->len  = (u32)strlen(err_msg);
        }
        return SEA_OK;
    }

    /* No LLM configured — echo fallback */
    return sea_tool_exec("echo", text, arena, response);
}

/* ── Telegram polling loop (legacy direct mode) ─────────── */

static int run_telegram(const char* token, i64 chat_id) {
    SeaError err = sea_telegram_init(&s_telegram, token, chat_id,
                                     telegram_handler, &s_request_arena);
    if (err != SEA_OK) {
        SEA_LOG_ERROR("TELEGRAM", "Init failed: %s", sea_error_str(err));
        return 1;
    }

    /* Test connection */
    err = sea_telegram_get_me(&s_telegram, &s_request_arena);
    sea_arena_reset(&s_request_arena);
    if (err != SEA_OK) {
        SEA_LOG_ERROR("TELEGRAM", "Connection failed: %s", sea_error_str(err));
        return 1;
    }

    SEA_LOG_INFO("STATUS", "Telegram polling started. Ctrl+C to stop.");

    while (s_running) {
        err = sea_telegram_poll(&s_telegram);
        if (err != SEA_OK && err != SEA_ERR_TIMEOUT) {
            SEA_LOG_WARN("TELEGRAM", "Poll error: %s (retrying in 5s)", sea_error_str(err));
            sleep(5);
        }
    }

    return 0;
}

/* ── Gateway: bus-based agent loop thread ─────────────────── */

/* Forward declaration for Telegram channel create */
extern SeaChannel* sea_channel_telegram_create(const char* bot_token, i64 allowed_chat_id);
extern void sea_channel_telegram_destroy(SeaChannel* ch);

static void* gateway_agent_thread(void* arg) {
    (void)arg;
    SEA_LOG_INFO("GATEWAY", "Agent loop started (bus consumer)");

    SeaArena agent_arena;
    if (sea_arena_create(&agent_arena, REQUEST_ARENA) != SEA_OK) {
        SEA_LOG_ERROR("GATEWAY", "Failed to create agent arena");
        return NULL;
    }

    while (s_running) {
        SeaBusMsg msg;
        SeaError err = sea_bus_consume_inbound(&s_bus, &msg, 500);
        if (err == SEA_ERR_TIMEOUT || err == SEA_ERR_EOF) continue;
        if (err != SEA_OK) continue;

        SEA_LOG_INFO("GATEWAY", "Processing [%s] chat=%lld: %.60s",
                     msg.channel ? msg.channel : "?",
                     (long long)msg.chat_id,
                     msg.content ? msg.content : "");

        /* Check if it's a command (starts with /) — handle via telegram_handler logic */
        SeaSlice text_slice = { .data = (const u8*)msg.content, .len = msg.content_len };

        /* Shield check */
        if (!sea_shield_check(text_slice, SEA_GRAMMAR_SAFE_TEXT)) {
            sea_bus_publish_outbound(&s_bus, msg.channel, msg.chat_id,
                                     "Rejected: invalid input.", 24);
            sea_arena_reset(&agent_arena);
            continue;
        }

        /* Route through LLM agent */
        if (s_agent_cfg.api_key || s_agent_cfg.provider == SEA_LLM_LOCAL) {
            /* Load conversation history from DB */
            SeaDbChatMsg db_msgs[20];
            i32 hist_count = 0;
            if (s_db) {
                hist_count = sea_db_chat_history(s_db, msg.chat_id, db_msgs, 20,
                                                 &agent_arena);
            }

            /* Convert DB messages to agent format */
            SeaChatMsg history[20];
            for (i32 i = 0; i < hist_count; i++) {
                if (strcmp(db_msgs[i].role, "assistant") == 0)
                    history[i].role = SEA_ROLE_ASSISTANT;
                else
                    history[i].role = SEA_ROLE_USER;
                history[i].content = db_msgs[i].content;
                history[i].tool_call_id = NULL;
                history[i].tool_name = NULL;
            }

            SeaAgentResult ar = sea_agent_chat(&s_agent_cfg,
                                               history, (u32)hist_count,
                                               msg.content, &agent_arena);
            if (ar.error == SEA_OK && ar.text) {
                u32 rlen = (u32)strlen(ar.text);
                sea_bus_publish_outbound(&s_bus, msg.channel, msg.chat_id,
                                         ar.text, rlen);
                /* Save to conversation memory */
                if (s_db) {
                    sea_db_chat_log(s_db, msg.chat_id, "user", msg.content);
                    sea_db_chat_log(s_db, msg.chat_id, "assistant", ar.text);
                }
            } else {
                const char* err_msg = ar.text ? ar.text : "Agent error.";
                sea_bus_publish_outbound(&s_bus, msg.channel, msg.chat_id,
                                         err_msg, (u32)strlen(err_msg));
            }
        } else {
            sea_bus_publish_outbound(&s_bus, msg.channel, msg.chat_id,
                                     "No LLM configured.", 19);
        }

        sea_arena_reset(&agent_arena);
    }

    sea_arena_destroy(&agent_arena);
    SEA_LOG_INFO("GATEWAY", "Agent loop stopped");
    return NULL;
}

static void* gateway_dispatch_thread(void* arg) {
    (void)arg;
    SEA_LOG_INFO("GATEWAY", "Outbound dispatcher started");

    while (s_running) {
        u32 dispatched = sea_channel_dispatch_outbound(&s_chan_mgr);
        if (dispatched == 0) {
            usleep(50000); /* 50ms idle sleep */
        }
    }

    SEA_LOG_INFO("GATEWAY", "Outbound dispatcher stopped");
    return NULL;
}

static SeaChannel* s_tg_channel_ptr = NULL;

static int run_gateway(const char* tg_token, i64 tg_chat_id) {
    /* Initialize message bus (2MB arena for message data) */
    SeaError err = sea_bus_init(&s_bus, 2 * 1024 * 1024);
    if (err != SEA_OK) {
        SEA_LOG_ERROR("GATEWAY", "Bus init failed: %s", sea_error_str(err));
        return 1;
    }
    s_bus_ptr = &s_bus;

    /* Initialize channel manager */
    sea_channel_manager_init(&s_chan_mgr, &s_bus);

    /* Register Telegram channel if configured */
    if (tg_token) {
        s_tg_channel_ptr = sea_channel_telegram_create(tg_token, tg_chat_id);
        if (s_tg_channel_ptr) {
            sea_channel_manager_register(&s_chan_mgr, s_tg_channel_ptr);
        }
    }

    /* Register WebSocket channel if enabled */
    {
        const char* ws_enabled = s_db ? sea_db_config_get(s_db, "ws_enabled", &s_request_arena) : NULL;
        if (ws_enabled && strcmp(ws_enabled, "true") == 0) {
            if (sea_ws_init(&s_ws_inst, SEA_WS_DEFAULT_PORT, &s_bus) == SEA_OK) {
                static SeaChannel s_ws_channel;
                if (sea_ws_channel_create(&s_ws_channel, &s_ws_inst) == SEA_OK) {
                    sea_channel_manager_register(&s_chan_mgr, &s_ws_channel);
                    SEA_LOG_INFO("GATEWAY", "WebSocket channel registered (port %u)",
                                 SEA_WS_DEFAULT_PORT);
                }
            }
        }
        sea_arena_reset(&s_request_arena);
    }

    /* Start all channels (each gets its own poll thread) */
    err = sea_channel_manager_start_all(&s_chan_mgr);
    if (err != SEA_OK) {
        SEA_LOG_ERROR("GATEWAY", "Channel start failed: %s", sea_error_str(err));
        sea_bus_destroy(&s_bus);
        return 1;
    }

    /* Print enabled channels */
    const char* names[SEA_MAX_CHANNELS];
    u32 nc = sea_channel_manager_enabled_names(&s_chan_mgr, names, SEA_MAX_CHANNELS);
    for (u32 i = 0; i < nc; i++) {
        SEA_LOG_INFO("GATEWAY", "Channel active: %s", names[i]);
    }

    /* Start agent loop thread (consumes inbound, publishes outbound) */
    pthread_t agent_tid;
    pthread_create(&agent_tid, NULL, gateway_agent_thread, NULL);

    /* Start outbound dispatch thread (routes outbound to channels) */
    pthread_t dispatch_tid;
    pthread_create(&dispatch_tid, NULL, gateway_dispatch_thread, NULL);

    SEA_LOG_INFO("GATEWAY", "Gateway running. %u channel(s). Ctrl+C to stop.", nc);

    /* Wait for shutdown signal, ticking heartbeat + cron */
    while (s_running) {
        sleep(1);
        if (s_heartbeat) sea_heartbeat_tick(s_heartbeat);
        if (s_cron) sea_cron_tick(s_cron);
    }

    /* Graceful shutdown */
    SEA_LOG_INFO("GATEWAY", "Shutting down...");
    sea_channel_manager_stop_all(&s_chan_mgr);
    sea_bus_destroy(&s_bus);

    pthread_join(agent_tid, NULL);
    pthread_join(dispatch_tid, NULL);

    if (s_tg_channel_ptr) {
        sea_channel_telegram_destroy(s_tg_channel_ptr);
        s_tg_channel_ptr = NULL;
    }

    return 0;
}

/* ── Main ─────────────────────────────────────────────────── */

int main(int argc, char** argv) {
    /* Initialize CLI subcommand dispatch and extension registry */
    sea_cli_init(&s_cli);
    sea_ext_init(&s_ext_reg);

    /* Try subcommand dispatch first (e.g. `sea_claw doctor`) */
    if (argc >= 2 && argv[1][0] != '-') {
        int ret = sea_cli_dispatch(&s_cli, argc, argv);
        if (ret >= 0) return ret;
        /* -1 means not a subcommand — fall through to legacy parsing */
    }

    /* Parse CLI args (legacy --flag mode) */
    const char* tg_token = NULL;
    i64 tg_chat_id = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--gateway") == 0) {
            s_gateway_mode = true;
        } else if (strcmp(argv[i], "--telegram") == 0 && i + 1 < argc) {
            tg_token = argv[++i];
            s_telegram_mode = true;
        } else if (strcmp(argv[i], "--chat") == 0 && i + 1 < argc) {
            tg_chat_id = atoll(argv[++i]);
        } else if (strcmp(argv[i], "--db") == 0 && i + 1 < argc) {
            s_db_path = argv[++i];
        } else if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            s_config_path = argv[++i];
        } else if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            s_mesh_role_str = argv[++i];
            s_mesh_mode = true;
        } else if (strcmp(argv[i], "--doctor") == 0) {
            /* ── Doctor: diagnose config, providers, channels ──── */
            load_dotenv(".env");
            SeaArena da; sea_arena_create(&da, 8192);
            sea_config_load(&s_config, s_config_path, &da);
            printf("\n\033[1m  Sea-Claw Doctor\033[0m\n");
            printf("  ════════════════════════════════════════\n\n");
            printf("  \033[1mBinary:\033[0m        %s\n", SEA_VERSION_STRING);
            printf("  \033[1mConfig file:\033[0m   %s\n", s_config_path);
            printf("  \033[1mDB path:\033[0m       %s\n", s_config.db_path ? s_config.db_path : s_db_path);
            printf("  \033[1mArena size:\033[0m    %u MB\n", s_config.arena_size_mb > 0 ? s_config.arena_size_mb : 16);
            printf("\n  \033[1mLLM Provider:\033[0m\n");
            printf("    provider:  %s %s\n", s_config.llm_provider ? s_config.llm_provider : "(not set)",
                   s_config.llm_provider ? "\033[32m✓\033[0m" : "\033[31m✗\033[0m");
            printf("    api_key:   %s\n", (s_config.llm_api_key && *s_config.llm_api_key) ? "\033[32m✓ set\033[0m" : "\033[31m✗ missing\033[0m");
            printf("    model:     %s\n", s_config.llm_model ? s_config.llm_model : "(default)");
            printf("    api_url:   %s\n", s_config.llm_api_url ? s_config.llm_api_url : "(default)");
            printf("\n  \033[1mTelegram:\033[0m\n");
            const char* tg_env = getenv("TELEGRAM_BOT_TOKEN");
            bool tg_ok = (s_config.telegram_token && *s_config.telegram_token) || (tg_env && *tg_env);
            printf("    token:     %s\n", tg_ok ? "\033[32m✓ set\033[0m" : "\033[33m○ not set (optional)\033[0m");
            printf("    chat_id:   %lld %s\n", (long long)s_config.telegram_chat_id,
                   s_config.telegram_chat_id ? "\033[32m✓\033[0m" : "\033[33m○\033[0m");
            printf("\n  \033[1mFallbacks:\033[0m     %u configured\n", s_config.llm_fallback_count);
            printf("\n  \033[1mEnvironment:\033[0m\n");
            const char* env_keys[] = {"OPENAI_API_KEY","ANTHROPIC_API_KEY","GEMINI_API_KEY","OPENROUTER_API_KEY","ZAI_API_KEY","TELEGRAM_BOT_TOKEN","EXA_API_KEY"};
            for (int e = 0; e < 7; e++) {
                const char* v = getenv(env_keys[e]);
                printf("    %-24s %s\n", env_keys[e], (v && *v) ? "\033[32m✓\033[0m" : "\033[90m-\033[0m");
            }
            printf("\n  \033[1mDatabase:\033[0m\n");
            SeaDb* ddb = NULL;
            const char* dbp = s_config.db_path ? s_config.db_path : s_db_path;
            if (sea_db_open(&ddb, dbp) == SEA_OK) {
                printf("    status:    \033[32m✓ OK\033[0m (%s)\n", dbp);
                sea_db_close(ddb);
            } else {
                printf("    status:    \033[31m✗ cannot open\033[0m (%s)\n", dbp);
            }
            printf("\n  \033[1mSkills dir:\033[0m\n");
            char skills_path[256];
            const char* home = getenv("HOME");
            snprintf(skills_path, sizeof(skills_path), "%s/.seaclaw/skills", home ? home : "/tmp");
            struct stat sst;
            if (stat(skills_path, &sst) == 0) {
                printf("    %s \033[32m✓\033[0m\n", skills_path);
            } else {
                printf("    %s \033[33m○ (will be created)\033[0m\n", skills_path);
            }
            printf("\n  \033[1mTools:\033[0m         57 compiled in\n");
            printf("\n  ════════════════════════════════════════\n\n");
            sea_arena_destroy(&da);
            return 0;
        } else if (strcmp(argv[i], "--onboard") == 0) {
            /* ── Onboard: interactive first-run setup ──────────── */
            printf("\n\033[1m  Sea-Claw Onboard Wizard\033[0m\n");
            printf("  ════════════════════════════════════════\n\n");

            /* ── Step 1: Personalization ──────────────────────── */
            printf("  \033[1mStep 1/3 — Who are you?\033[0m\n\n");
            char ob_name[128] = {0}, ob_work[256] = {0}, ob_tone[64] = {0};

            printf("  Your name (or press Enter to skip): ");
            fflush(stdout);
            if (fgets(ob_name, sizeof(ob_name), stdin)) {
                ob_name[strcspn(ob_name, "\n")] = '\0';
            }
            printf("  What do you do? (e.g. developer, student, business owner): ");
            fflush(stdout);
            if (fgets(ob_work, sizeof(ob_work), stdin)) {
                ob_work[strcspn(ob_work, "\n")] = '\0';
            }
            printf("  Preferred tone (casual/professional/technical) [professional]: ");
            fflush(stdout);
            if (fgets(ob_tone, sizeof(ob_tone), stdin)) {
                ob_tone[strcspn(ob_tone, "\n")] = '\0';
            }
            if (!ob_tone[0]) strncpy(ob_tone, "professional", sizeof(ob_tone) - 1);

            /* Write USER.md */
            {
                const char* home = getenv("HOME");
                if (!home) home = "/tmp";
                char user_dir[2048];
                snprintf(user_dir, sizeof(user_dir), "%s/.seaclaw", home);
                mkdir(user_dir, 0755);
                char user_path[4096];
                snprintf(user_path, sizeof(user_path), "%s/USER.md", user_dir);
                FILE* uf = fopen(user_path, "w");
                if (uf) {
                    fprintf(uf, "# User Profile\n\n");
                    if (ob_name[0]) fprintf(uf, "- **Name:** %s\n", ob_name);
                    if (ob_work[0]) fprintf(uf, "- **Role:** %s\n", ob_work);
                    fprintf(uf, "- **Preferred tone:** %s\n", ob_tone);
                    fprintf(uf, "\nThis profile was created during onboarding.\n");
                    fprintf(uf, "Edit this file anytime at: %s\n", user_path);
                    fclose(uf);
                    printf("  \033[32m✓\033[0m User profile saved to %s\n", user_path);
                }
                /* Update SOUL.md with tone preference */
                char soul_path[4096];
                snprintf(soul_path, sizeof(soul_path), "%s/SOUL.md", user_dir);
                FILE* sf = fopen(soul_path, "w");
                if (sf) {
                    fprintf(sf, "# Soul\n\n## Principles\n");
                    fprintf(sf, "- Be concise and direct\n");
                    fprintf(sf, "- Prefer action over explanation\n");
                    fprintf(sf, "- Use tools when they help\n");
                    fprintf(sf, "- Remember context from previous conversations\n");
                    fprintf(sf, "- Protect user data and privacy\n");
                    fprintf(sf, "- Admit uncertainty rather than guess\n\n");
                    fprintf(sf, "## Tone\n");
                    if (strcmp(ob_tone, "casual") == 0) {
                        fprintf(sf, "- Friendly and relaxed\n");
                        fprintf(sf, "- Use simple language, avoid jargon\n");
                        fprintf(sf, "- It's okay to be brief and informal\n");
                    } else if (strcmp(ob_tone, "technical") == 0) {
                        fprintf(sf, "- Precise and detailed\n");
                        fprintf(sf, "- Use technical terminology freely\n");
                        fprintf(sf, "- Include code examples when relevant\n");
                        fprintf(sf, "- Assume the user is technically proficient\n");
                    } else {
                        fprintf(sf, "- Professional but approachable\n");
                        fprintf(sf, "- Technical when needed, simple when possible\n");
                        fprintf(sf, "- No unnecessary filler or pleasantries\n");
                    }
                    if (ob_name[0]) {
                        fprintf(sf, "\n## User\n- Address the user as %s\n", ob_name);
                    }
                    fclose(sf);
                    printf("  \033[32m✓\033[0m Personality configured (%s tone)\n", ob_tone);
                }
            }

            /* Seed recall DB with user facts */
            {
                SeaDb* ob_db = NULL;
                if (sea_db_open(&ob_db, "seaclaw.db") == SEA_OK) {
                    SeaRecall ob_recall;
                    if (sea_recall_init(&ob_recall, ob_db, 800) == SEA_OK) {
                        if (ob_name[0]) {
                            char fact[256];
                            snprintf(fact, sizeof(fact), "The user's name is %.200s", ob_name);
                            sea_recall_store(&ob_recall, "user", fact, NULL, 9);
                        }
                        if (ob_work[0]) {
                            char fact[512];
                            snprintf(fact, sizeof(fact), "The user works as: %.400s", ob_work);
                            sea_recall_store(&ob_recall, "user", fact, NULL, 8);
                        }
                        {
                            char fact[128];
                            snprintf(fact, sizeof(fact), "The user prefers %s tone", ob_tone);
                            sea_recall_store(&ob_recall, "preference", fact, NULL, 8);
                        }
                        sea_recall_destroy(&ob_recall);
                        printf("  \033[32m✓\033[0m Memory seeded (%s)\n",
                               ob_name[0] ? ob_name : "anonymous");
                    }
                    sea_db_close(ob_db);
                }
            }

            /* ── Step 2: LLM Provider ────────────────────────── */
            printf("\n  \033[1mStep 2/3 — LLM Provider\033[0m\n\n");
            char ob_provider[64] = {0}, ob_key[256] = {0}, ob_model[128] = {0};

            printf("  Provider (openai/anthropic/gemini/openrouter/zai/local): ");
            fflush(stdout);
            if (fgets(ob_provider, sizeof(ob_provider), stdin)) {
                ob_provider[strcspn(ob_provider, "\n")] = '\0';
            }
            char ob_api_url[256] = {0};
            if (strcmp(ob_provider, "local") == 0) {
                printf("  No API key needed for local LLM.\n");
                printf("  Ollama URL [http://localhost:11434]: ");
                fflush(stdout);
                if (fgets(ob_api_url, sizeof(ob_api_url), stdin)) {
                    ob_api_url[strcspn(ob_api_url, "\n")] = '\0';
                }
            } else {
                printf("  API Key: ");
                fflush(stdout);
                if (fgets(ob_key, sizeof(ob_key), stdin)) {
                    ob_key[strcspn(ob_key, "\n")] = '\0';
                }
            }
            printf("  Model (or press Enter for default): ");
            fflush(stdout);
            if (fgets(ob_model, sizeof(ob_model), stdin)) {
                ob_model[strcspn(ob_model, "\n")] = '\0';
            }

            /* ── Step 3: Telegram (optional) ─────────────────── */
            printf("\n  \033[1mStep 3/3 — Telegram Bot (optional)\033[0m\n\n");
            char ob_tg_token[256] = {0}, ob_tg_chat[32] = {0};

            printf("  Bot Token (or press Enter to skip): ");
            fflush(stdout);
            if (fgets(ob_tg_token, sizeof(ob_tg_token), stdin)) {
                ob_tg_token[strcspn(ob_tg_token, "\n")] = '\0';
            }
            if (ob_tg_token[0]) {
                printf("  Chat ID: ");
                fflush(stdout);
                if (fgets(ob_tg_chat, sizeof(ob_tg_chat), stdin)) {
                    ob_tg_chat[strcspn(ob_tg_chat, "\n")] = '\0';
                }
            }

            /* Write config.json */
            FILE* cf = fopen("config.json", "w");
            if (cf) {
                fprintf(cf, "{\n");
                fprintf(cf, "  \"llm_provider\": \"%s\",\n", ob_provider);
                if (ob_key[0]) fprintf(cf, "  \"llm_api_key\": \"%s\",\n", ob_key);
                if (ob_api_url[0]) fprintf(cf, "  \"llm_api_url\": \"%s/v1/chat/completions\",\n", ob_api_url);
                if (ob_model[0]) fprintf(cf, "  \"llm_model\": \"%s\",\n", ob_model);
                if (ob_tg_token[0]) {
                    fprintf(cf, "  \"telegram_token\": \"%s\",\n", ob_tg_token);
                    if (ob_tg_chat[0]) fprintf(cf, "  \"telegram_chat_id\": %s,\n", ob_tg_chat);
                }
                fprintf(cf, "  \"arena_size_mb\": 16,\n");
                fprintf(cf, "  \"db_path\": \"seaclaw.db\"\n");
                fprintf(cf, "}\n");
                fclose(cf);
                printf("\n  \033[32m✓\033[0m Config written to config.json\n");
            } else {
                printf("\n  \033[31m✗\033[0m Failed to write config.json\n\n");
                return 1;
            }

            printf("\n  ════════════════════════════════════════\n");
            printf("  \033[32m\033[1m  Setup complete!\033[0m\n\n");
            printf("  Run \033[1m./sea_claw\033[0m to start chatting.\n");
            printf("  Run \033[1m./sea_claw --doctor\033[0m to verify.\n\n");
            if (ob_name[0]) {
                printf("  Welcome aboard, %s. The Vault remembers.\n\n", ob_name);
            }
            return 0;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: sea_claw [OPTIONS]\n");
            printf("  --config <path>     Config file (default: config.json)\n");
            printf("  --gateway           Run in gateway mode (bus-based, multi-channel)\n");
            printf("  --telegram <token>  Run as Telegram bot (legacy direct mode)\n");
            printf("  --chat <id>         Restrict to chat ID\n");
            printf("  --db <path>         Database file (default: seaclaw.db)\n");
            printf("  --doctor            Diagnose config, providers, channels\n");
            printf("  --onboard           Interactive first-run setup wizard\n");
            printf("  -h, --help          Show this help\n");
            return 0;
        }
    }

    /* Trap signals for clean shutdown */
    signal(SIGINT,  handle_signal);
    signal(SIGTERM, handle_signal);

    /* Initialize logging */
    sea_log_init(SEA_LOG_INFO);

    /* Print banner */
    printf("\033[2J\033[H");  /* Clear screen */
    printf("%s\n", BANNER);

    /* Load .env file (before config so env vars are available) */
    load_dotenv(".env");

    /* Load config (uses a small temporary arena for JSON parsing) */
    SeaArena cfg_arena;
    sea_arena_create(&cfg_arena, 8192);
    sea_config_load(&s_config, s_config_path, &cfg_arena);

    /* Environment variable overrides for secrets (non-empty only) */
    const char* env_val;
    /* Note: LLM API keys are resolved per-provider below in the agent init section */
    if ((env_val = getenv("TELEGRAM_BOT_TOKEN")) && *env_val &&
        (!s_config.telegram_token || !*s_config.telegram_token))
        s_config.telegram_token = env_val;
    if ((env_val = getenv("TELEGRAM_CHAT_ID")) && *env_val &&
        s_config.telegram_chat_id == 0)
        s_config.telegram_chat_id = atoll(env_val);

    /* Config values as fallbacks for CLI args */
    if (!tg_token && s_config.telegram_token) {
        tg_token = s_config.telegram_token;
        s_telegram_mode = true;
    }
    if (tg_chat_id == 0 && s_config.telegram_chat_id != 0) {
        tg_chat_id = s_config.telegram_chat_id;
    }
    if (strcmp(s_db_path, DEFAULT_DB_PATH) == 0 && s_config.db_path) {
        s_db_path = s_config.db_path;
    }

    /* Initialize arenas */
    u32 arena_mb = s_config.arena_size_mb > 0 ? s_config.arena_size_mb : 16;
    u64 arena_bytes = (u64)arena_mb * 1024 * 1024;
    SEA_LOG_INFO("SYSTEM", "Substrate initializing. Arena: %uMB (Fixed).", arena_mb);

    if (sea_arena_create(&s_session_arena, arena_bytes) != SEA_OK) {
        SEA_LOG_ERROR("SYSTEM", "Failed to create session arena");
        return 1;
    }
    if (sea_arena_create(&s_request_arena, REQUEST_ARENA) != SEA_OK) {
        SEA_LOG_ERROR("SYSTEM", "Failed to create request arena");
        sea_arena_destroy(&s_session_arena);
        return 1;
    }

    /* Initialize tools */
    sea_tools_init();

    /* Initialize database */
    if (sea_db_open(&s_db, s_db_path) == SEA_OK) {
        SEA_LOG_INFO("DB", "Ledger open: %s", s_db_path);
        sea_db_log_event(s_db, "startup", "Sea-Claw started", SEA_VERSION_STRING);
    } else {
        SEA_LOG_WARN("DB", "Running without database.");
    }

    /* Initialize agent */
    memset(&s_agent_cfg, 0, sizeof(s_agent_cfg));
    s_agent_cfg.provider = parse_provider(s_config.llm_provider);
    s_agent_cfg.api_key  = s_config.llm_api_key;
    s_agent_cfg.api_url  = s_config.llm_api_url;
    s_agent_cfg.model    = s_config.llm_model;

    /* Env var overrides per provider for primary (check NULL or empty) */
    #define NEEDS_KEY(k) (!(k) || !*(k))
    switch (s_agent_cfg.provider) {
        case SEA_LLM_OPENAI:
            if (NEEDS_KEY(s_agent_cfg.api_key) && (env_val = getenv("OPENAI_API_KEY")) && *env_val)
                s_agent_cfg.api_key = env_val;
            break;
        case SEA_LLM_ANTHROPIC:
            if (NEEDS_KEY(s_agent_cfg.api_key) && (env_val = getenv("ANTHROPIC_API_KEY")) && *env_val)
                s_agent_cfg.api_key = env_val;
            break;
        case SEA_LLM_GEMINI:
            if (NEEDS_KEY(s_agent_cfg.api_key) && (env_val = getenv("GEMINI_API_KEY")) && *env_val)
                s_agent_cfg.api_key = env_val;
            break;
        case SEA_LLM_OPENROUTER:
            if (NEEDS_KEY(s_agent_cfg.api_key) && (env_val = getenv("OPENROUTER_API_KEY")) && *env_val)
                s_agent_cfg.api_key = env_val;
            break;
        case SEA_LLM_LOCAL:
            break;
        case SEA_LLM_ZAI:
            if (NEEDS_KEY(s_agent_cfg.api_key) && (env_val = getenv("ZAI_API_KEY")) && *env_val)
                s_agent_cfg.api_key = env_val;
            break;
    }

    /* Wire fallback providers */
    s_agent_cfg.fallback_count = 0;
    for (u32 fi = 0; fi < s_config.llm_fallback_count && fi < SEA_MAX_FALLBACKS; fi++) {
        SeaLlmFallback* fb = &s_agent_cfg.fallbacks[fi];
        fb->provider = parse_provider(s_config.llm_fallbacks[fi].provider);
        fb->api_key  = s_config.llm_fallbacks[fi].api_key;
        fb->api_url  = s_config.llm_fallbacks[fi].api_url;
        fb->model    = s_config.llm_fallbacks[fi].model;
        /* Env var override for fallback API keys */
        if (NEEDS_KEY(fb->api_key)) {
            switch (fb->provider) {
                case SEA_LLM_OPENAI:     fb->api_key = getenv("OPENAI_API_KEY"); break;
                case SEA_LLM_ANTHROPIC:  fb->api_key = getenv("ANTHROPIC_API_KEY"); break;
                case SEA_LLM_GEMINI:     fb->api_key = getenv("GEMINI_API_KEY"); break;
                case SEA_LLM_OPENROUTER: fb->api_key = getenv("OPENROUTER_API_KEY"); break;
                case SEA_LLM_LOCAL:      break;
                case SEA_LLM_ZAI:        fb->api_key = getenv("ZAI_API_KEY"); break;
            }
        }
        s_agent_cfg.fallback_count++;
    }
    #undef NEEDS_KEY

    sea_agent_init(&s_agent_cfg);

    /* Initialize cron scheduler */
    if (sea_cron_init(&s_cron_inst, s_db, NULL) == SEA_OK) {
        s_cron = &s_cron_inst;
        sea_cron_load(s_cron);
        SEA_LOG_INFO("CRON", "Scheduler ready (%u jobs)", sea_cron_count(s_cron));
    }

    /* Initialize memory system */
    if (sea_memory_init(&s_memory_inst, NULL, 128 * 1024) == SEA_OK) {
        s_memory = &s_memory_inst;
        sea_memory_create_defaults(s_memory);
        SEA_LOG_INFO("MEMORY", "Memory system ready");
    }

    /* Initialize skills (DB-backed if available) */
    if (s_db && sea_skill_init_db(&s_skill_reg, NULL, s_db) == SEA_OK) {
        sea_skill_load_all(&s_skill_reg);
        SEA_LOG_INFO("SKILL", "Skills loaded: %u (DB-persisted)", sea_skill_count(&s_skill_reg));
    } else if (sea_skill_init(&s_skill_reg, NULL) == SEA_OK) {
        sea_skill_load_all(&s_skill_reg);
        SEA_LOG_INFO("SKILL", "Skills loaded: %u (in-memory)", sea_skill_count(&s_skill_reg));
    }

    /* Initialize usage tracker */
    if (sea_usage_init(&s_usage_inst, s_db) == SEA_OK) {
        s_usage = &s_usage_inst;
        sea_usage_load(s_usage);
    }

    /* Initialize recall engine (SQLite memory index) */
    if (s_db && sea_recall_init(&s_recall_inst, s_db, 800) == SEA_OK) {
        s_recall = &s_recall_inst;
        SEA_LOG_INFO("RECALL", "Memory index ready (%u facts)", sea_recall_count(s_recall));
    }

    /* Initialize auth (DB-backed if available) */
    if (s_db && sea_auth_init_db(&s_auth_inst, true, s_db) == SEA_OK) {
        s_auth = &s_auth_inst;
        SEA_LOG_INFO("AUTH", "Token auth ready (%u tokens from DB)", s_auth_inst.count);
    } else {
        sea_auth_init(&s_auth_inst, false);
        s_auth = &s_auth_inst;
    }

    /* Initialize heartbeat (DB-logged if available) */
    if (s_memory) {
        if (s_db) {
            sea_heartbeat_init_db(&s_heartbeat_inst, s_memory, s_bus_ptr, 1800, s_db);
        } else {
            sea_heartbeat_init(&s_heartbeat_inst, s_memory, s_bus_ptr, 1800);
        }
        s_heartbeat = &s_heartbeat_inst;
        SEA_LOG_INFO("HEARTBEAT", "Proactive agent ready (interval: 30m)");
    }

    /* Initialize knowledge graph */
    if (s_db && sea_graph_init(&s_graph_inst, s_db) == SEA_OK) {
        s_graph = &s_graph_inst;
        SEA_LOG_INFO("GRAPH", "Knowledge graph ready (%u entities)",
                     sea_graph_entity_count(s_graph));
    }

    /* Initialize SeaZero (Agent Zero integration — opt-in) */
    {
        SeaArena sz_arena;
        sea_arena_create(&sz_arena, 4096);
        const char* sz_enabled = s_db ? sea_db_config_get(s_db, "seazero_enabled", &sz_arena) : NULL;
        if (sz_enabled && strcmp(sz_enabled, "true") == 0) {
            const char* sz_token = sea_db_config_get(s_db, "seazero_internal_token", &sz_arena);
            const char* sz_url   = sea_db_config_get(s_db, "seazero_agent_url", &sz_arena);
            const char* sz_budget = sea_db_config_get(s_db, "seazero_token_budget", &sz_arena);

            /* Initialize bridge */
            SeaZeroConfig zcfg = {0};
            sea_zero_init(&zcfg, sz_url);
            sea_zero_register_tool(&zcfg);

            /* Start LLM proxy if we have an API key */
            if (s_agent_cfg.api_key && *s_agent_cfg.api_key) {
                SeaProxyConfig pcfg = {
                    .port            = 7432,
                    .internal_token  = sz_token,
                    .real_api_url    = s_agent_cfg.api_url ? s_agent_cfg.api_url : "https://openrouter.ai/api/v1/chat/completions",
                    .real_api_key    = s_agent_cfg.api_key,
                    .real_provider   = s_config.llm_provider,
                    .real_model      = s_config.llm_model,
                    .daily_token_budget = sz_budget ? atoll(sz_budget) : 100000,
                    .db              = s_db,
                    .enabled         = true
                };
                if (sea_proxy_start(&pcfg) == 0) {
                    SEA_LOG_INFO("SEAZERO", "LLM proxy active on 127.0.0.1:7432");
                }
            }

            /* Initialize shared workspace */
            SeaWorkspaceConfig wscfg = {0};
            wscfg.retention_days = 7;
            sea_workspace_init(&wscfg);

            SEA_LOG_INFO("SEAZERO", "Agent Zero integration enabled (tool #58)");
        }
        sea_arena_destroy(&sz_arena);
    }

    /* Initialize mesh engine if --mode specified */
    if (s_mesh_mode && s_mesh_role_str) {
        SeaMeshConfig mcfg;
        memset(&mcfg, 0, sizeof(mcfg));
        if (strcmp(s_mesh_role_str, "captain") == 0) {
            mcfg.role = SEA_MESH_CAPTAIN;
            mcfg.port = 9100;
        } else if (strcmp(s_mesh_role_str, "crew") == 0) {
            mcfg.role = SEA_MESH_CREW;
            mcfg.port = 9101;
        } else {
            SEA_LOG_ERROR("MESH", "Unknown mode: %s (use captain or crew)", s_mesh_role_str);
            s_mesh_mode = false;
        }
        if (s_mesh_mode) {
            char hostname[64] = "node";
            gethostname(hostname, sizeof(hostname));
            strncpy(mcfg.node_name, hostname, SEA_MESH_NODE_NAME_MAX - 1);
            if (sea_mesh_init(&s_mesh_inst, &mcfg, s_db) == SEA_OK) {
                s_mesh = &s_mesh_inst;
            }
        }
    }

    SEA_LOG_INFO("SHIELD", "Grammar Filter: ACTIVE.");

    int ret = 0;

    if (s_gateway_mode) {
        /* ── Gateway Mode (v2 bus-based) ─────────────────────── */
        SEA_LOG_INFO("STATUS", "Starting gateway mode (v2 bus architecture)");
        ret = run_gateway(tg_token, tg_chat_id);
    } else if (s_telegram_mode) {
        /* ── Telegram Mode (legacy direct) ────────────────────── */
        ret = run_telegram(tg_token, tg_chat_id);
    } else {
        /* ── Interactive TUI Mode ─────────────────────────────── */
        SEA_LOG_INFO("STATUS", "Waiting for command... (Type /help)");
        printf("\n");

#ifdef HAVE_READLINE
        /* Readline: arrow keys, history, Ctrl+R search */
        rl_bind_key('\t', rl_insert); /* Tab inserts literal tab (no file completion) */
        using_history();

        char* line;
        while (s_running && (line = readline("\001\033[1;32m\002> \001\033[0m\002")) != NULL) {
            /* Trim trailing whitespace */
            u32 len = (u32)strlen(line);
            while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r' ||
                               line[len - 1] == ' ')) {
                line[--len] = '\0';
            }
            if (len == 0) { free(line); continue; }

            add_history(line);
            dispatch_command(line);
            free(line);
        }
#else
        /* Fallback: plain fgets (no arrow keys) */
        char input[INPUT_BUF_SIZE];

        while (s_running) {
            printf("\033[1;32m> \033[0m");
            fflush(stdout);

            if (!fgets(input, sizeof(input), stdin)) {
                break;
            }

            u32 len = (u32)strlen(input);
            while (len > 0 && (input[len - 1] == '\n' || input[len - 1] == '\r')) {
                input[--len] = '\0';
            }

            if (len == 0) continue;

            dispatch_command(input);
        }
#endif
    }

    /* ── Cleanup ──────────────────────────────────────────── */

    printf("\n");
    SEA_LOG_INFO("SYSTEM", "Shutting down...");
    sea_proxy_stop(); /* Stop LLM proxy if running (no-op if not started) */
    if (s_graph) { sea_graph_destroy(s_graph); s_graph = NULL; }
    if (s_mesh) { sea_mesh_destroy(s_mesh); s_mesh = NULL; }
    if (s_usage) { sea_usage_save(s_usage); s_usage = NULL; }
    if (s_recall) { sea_recall_destroy(s_recall); s_recall = NULL; }
    if (s_cron) { sea_cron_destroy(s_cron); s_cron = NULL; }
    if (s_memory) { sea_memory_destroy(s_memory); s_memory = NULL; }
    sea_skill_destroy(&s_skill_reg);
    if (s_db) {
        sea_db_log_event(s_db, "shutdown", "Sea-Claw stopped", "clean");
        sea_db_close(s_db);
    }
    sea_arena_destroy(&s_request_arena);
    sea_arena_destroy(&s_session_arena);
    sea_arena_destroy(&cfg_arena);
    SEA_LOG_INFO("SYSTEM", "Goodbye. The Vault stands.");

    return ret;
}
