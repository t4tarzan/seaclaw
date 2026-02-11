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
#include "seaclaw/sea_a2a.h"

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
SeaDb*               s_db = NULL;
static const char*   s_db_path = DEFAULT_DB_PATH;
static SeaConfig     s_config;
static const char*   s_config_path = "config.json";
static SeaAgentConfig s_agent_cfg;

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
    return SEA_LLM_OPENAI;
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
                "/model — Show current LLM model\n"
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

    /* Load .env file (before config so env vars are available) */
    load_dotenv(".env");

    /* Load config (uses a small temporary arena for JSON parsing) */
    SeaArena cfg_arena;
    sea_arena_create(&cfg_arena, 8192);
    sea_config_load(&s_config, s_config_path, &cfg_arena);

    /* Environment variable overrides for secrets (non-empty only) */
    const char* env_val;
    if ((env_val = getenv("OPENAI_API_KEY")) && *env_val &&
        (!s_config.llm_api_key || !*s_config.llm_api_key))
        s_config.llm_api_key = env_val;
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

    /* Env var overrides per provider for primary */
    switch (s_agent_cfg.provider) {
        case SEA_LLM_OPENAI:
            if (!s_agent_cfg.api_key && (env_val = getenv("OPENAI_API_KEY")))
                s_agent_cfg.api_key = env_val;
            break;
        case SEA_LLM_ANTHROPIC:
            if (!s_agent_cfg.api_key && (env_val = getenv("ANTHROPIC_API_KEY")))
                s_agent_cfg.api_key = env_val;
            break;
        case SEA_LLM_GEMINI:
            if (!s_agent_cfg.api_key && (env_val = getenv("GEMINI_API_KEY")))
                s_agent_cfg.api_key = env_val;
            break;
        case SEA_LLM_OPENROUTER:
            if (!s_agent_cfg.api_key && (env_val = getenv("OPENROUTER_API_KEY")))
                s_agent_cfg.api_key = env_val;
            break;
        case SEA_LLM_LOCAL:
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
        if (!fb->api_key) {
            switch (fb->provider) {
                case SEA_LLM_OPENAI:     fb->api_key = getenv("OPENAI_API_KEY"); break;
                case SEA_LLM_ANTHROPIC:  fb->api_key = getenv("ANTHROPIC_API_KEY"); break;
                case SEA_LLM_GEMINI:     fb->api_key = getenv("GEMINI_API_KEY"); break;
                case SEA_LLM_OPENROUTER: fb->api_key = getenv("OPENROUTER_API_KEY"); break;
                case SEA_LLM_LOCAL:      break;
            }
        }
        s_agent_cfg.fallback_count++;
    }

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
