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
#include "seaclaw/sea_db.h"
#include "seaclaw/sea_config.h"
#include "seaclaw/sea_agent.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

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
static SeaDb*        s_db = NULL;
static const char*   s_db_path = DEFAULT_DB_PATH;
static SeaConfig     s_config;
static const char*   s_config_path = "config.json";
static SeaAgentConfig s_agent_cfg;

/* ── Signal handler ───────────────────────────────────────── */

static void handle_signal(int sig) {
    (void)sig;
    s_running = false;
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
            /* Route through LLM agent */
            SeaAgentResult ar = sea_agent_chat(&s_agent_cfg, NULL, 0,
                                               input, &s_request_arena);
            if (ar.error == SEA_OK && ar.text) {
                printf("\n  %s\n", ar.text);
                if (ar.tool_calls > 0) {
                    printf("  \033[33m(%u tool call%s)\033[0m\n",
                           ar.tool_calls, ar.tool_calls > 1 ? "s" : "");
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
    (void)chat_id;

    /* Shield check */
    if (sea_shield_detect_injection(text)) {
        *response = SEA_SLICE_LIT("Rejected: injection detected.");
        return SEA_OK;
    }

    /* Check if it's a command */
    if (text.len > 0 && text.data[0] == '/') {
        if (sea_slice_eq_cstr(text, "/status")) {
            SeaSlice args = SEA_SLICE_EMPTY;
            return sea_tool_exec("system_status", args, arena, response);
        }
        if (sea_slice_eq_cstr(text, "/tools")) {
            *response = SEA_SLICE_LIT("Tools: echo, system_status");
            return SEA_OK;
        }
        if (sea_slice_eq_cstr(text, "/help")) {
            *response = SEA_SLICE_LIT(
                "Sea-Claw v" SEA_VERSION_STRING "\n"
                "/status - System status\n"
                "/tools - List tools\n"
                "/exec <tool> <args> - Execute tool");
            return SEA_OK;
        }
        /* /exec <tool> <args> */
        if (text.len > 6 && memcmp(text.data, "/exec ", 6) == 0) {
            const u8* rest = text.data + 6;
            u32 rlen = text.len - 6;
            /* Extract tool name */
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
        *response = SEA_SLICE_LIT("Unknown command. Type /help");
        return SEA_OK;
    }

    /* Natural language — echo for now */
    return sea_tool_exec("echo", text, arena, response);
}

/* ── Telegram polling loop ───────────────────────────────── */

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

/* ── Main ─────────────────────────────────────────────────── */

int main(int argc, char** argv) {
    /* Parse CLI args */
    const char* tg_token = NULL;
    i64 tg_chat_id = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--telegram") == 0 && i + 1 < argc) {
            tg_token = argv[++i];
            s_telegram_mode = true;
        } else if (strcmp(argv[i], "--chat") == 0 && i + 1 < argc) {
            tg_chat_id = atoll(argv[++i]);
        } else if (strcmp(argv[i], "--db") == 0 && i + 1 < argc) {
            s_db_path = argv[++i];
        } else if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            s_config_path = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: sea_claw [OPTIONS]\n");
            printf("  --config <path>     Config file (default: config.json)\n");
            printf("  --telegram <token>  Run as Telegram bot\n");
            printf("  --chat <id>         Restrict to chat ID\n");
            printf("  --db <path>         Database file (default: seaclaw.db)\n");
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

    /* Load config (uses a small temporary arena for JSON parsing) */
    SeaArena cfg_arena;
    sea_arena_create(&cfg_arena, 8192);
    sea_config_load(&s_config, s_config_path, &cfg_arena);

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
    if (s_config.llm_provider) {
        if (strcmp(s_config.llm_provider, "anthropic") == 0)
            s_agent_cfg.provider = SEA_LLM_ANTHROPIC;
        else if (strcmp(s_config.llm_provider, "local") == 0)
            s_agent_cfg.provider = SEA_LLM_LOCAL;
        else
            s_agent_cfg.provider = SEA_LLM_OPENAI;
    }
    s_agent_cfg.api_key = s_config.llm_api_key;
    s_agent_cfg.api_url = s_config.llm_api_url;
    s_agent_cfg.model   = s_config.llm_model;
    sea_agent_init(&s_agent_cfg);

    SEA_LOG_INFO("SHIELD", "Grammar Filter: ACTIVE.");

    int ret = 0;

    if (s_telegram_mode) {
        /* ── Telegram Mode ───────────────────────────────────── */
        ret = run_telegram(tg_token, tg_chat_id);
    } else {
        /* ── Interactive TUI Mode ─────────────────────────────── */
        SEA_LOG_INFO("STATUS", "Waiting for command... (Type /help)");
        printf("\n");

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
    }

    /* ── Cleanup ──────────────────────────────────────────── */

    printf("\n");
    SEA_LOG_INFO("SYSTEM", "Shutting down...");
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
