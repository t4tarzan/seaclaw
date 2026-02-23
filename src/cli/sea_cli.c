/*
 * sea_cli.c — CLI Subcommand Dispatch + Built-in Commands
 *
 * Table-driven routing: `sea_claw <subcommand> [args]`
 * Built-in subcommands: doctor, onboard, version, help
 */

#include "seaclaw/sea_cli.h"
#include "seaclaw/sea_log.h"
#include "seaclaw/sea_arena.h"
#include "seaclaw/sea_config.h"
#include "seaclaw/sea_db.h"
#include "seaclaw/sea_recall.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* ── Registry Init ────────────────────────────────────────── */

void sea_cli_init(SeaCli* cli) {
    if (!cli) return;
    memset(cli, 0, sizeof(*cli));

    /* Register built-in subcommands */
    sea_cli_register(cli, "doctor",  "Diagnose config, providers, channels",
                     "sea_claw doctor", sea_cmd_doctor);
    sea_cli_register(cli, "onboard", "Interactive first-run setup wizard",
                     "sea_claw onboard", sea_cmd_onboard);
    sea_cli_register(cli, "version", "Show version information",
                     "sea_claw version", sea_cmd_version);
    sea_cli_register(cli, "help",    "Show available subcommands",
                     "sea_claw help", sea_cmd_help);
}

/* ── Register ─────────────────────────────────────────────── */

SeaError sea_cli_register(SeaCli* cli, const char* name,
                           const char* description, const char* usage,
                           SeaCliFunc func) {
    if (!cli || !name || !func) return SEA_ERR_INVALID_INPUT;
    if (cli->count >= SEA_CLI_MAX) return SEA_ERR_FULL;

    SeaCliCmd* cmd = &cli->commands[cli->count];
    strncpy(cmd->name, name, SEA_CLI_NAME_MAX - 1);
    cmd->name[SEA_CLI_NAME_MAX - 1] = '\0';
    cmd->description = description;
    cmd->usage = usage;
    cmd->func = func;
    cli->count++;
    return SEA_OK;
}

/* ── Find ─────────────────────────────────────────────────── */

const SeaCliCmd* sea_cli_find(const SeaCli* cli, const char* name) {
    if (!cli || !name) return NULL;
    for (u32 i = 0; i < cli->count; i++) {
        if (strcmp(cli->commands[i].name, name) == 0) {
            return &cli->commands[i];
        }
    }
    return NULL;
}

/* ── Dispatch ─────────────────────────────────────────────── */

int sea_cli_dispatch(const SeaCli* cli, int argc, char** argv) {
    if (!cli || argc < 2) return -1;

    /* argv[1] is the subcommand name */
    const char* subcmd = argv[1];

    /* Skip if it starts with -- (legacy flag mode) */
    if (subcmd[0] == '-') return -1;

    const SeaCliCmd* cmd = sea_cli_find(cli, subcmd);
    if (!cmd) return -1;

    /* Shift argc/argv so the subcommand sees its own args */
    return cmd->func(argc - 1, argv + 1);
}

/* ── Help ─────────────────────────────────────────────────── */

void sea_cli_help(const SeaCli* cli) {
    if (!cli) return;
    printf("\n  \033[1mSubcommands:\033[0m\n");
    for (u32 i = 0; i < cli->count; i++) {
        printf("    %-12s %s\n",
               cli->commands[i].name,
               cli->commands[i].description);
    }
    printf("\n  \033[1mLegacy flags:\033[0m\n");
    printf("    --gateway           Run in gateway mode (bus-based, multi-channel)\n");
    printf("    --telegram <token>  Run as Telegram bot (legacy direct mode)\n");
    printf("    --chat <id>         Restrict to chat ID\n");
    printf("    --db <path>         Database file (default: seaclaw.db)\n");
    printf("    --config <path>     Config file (default: config.json)\n");
    printf("    --mode <role>       Mesh mode (captain|crew)\n");
    printf("    -h, --help          Show this help\n");
    printf("\n");
}

/* ── Built-in: version ────────────────────────────────────── */

int sea_cmd_version(int argc, char** argv) {
    (void)argc; (void)argv;
    printf("Sea-Claw %s\n", SEA_VERSION_STRING);
    printf("  C11 sovereign terminal agent\n");
    printf("  Tools: 58 | Providers: 6 | Dependencies: 2\n");
    printf("  License: Proprietary\n");
    return 0;
}

/* ── Built-in: help ───────────────────────────────────────── */

int sea_cmd_help(int argc, char** argv) {
    (void)argc; (void)argv;
    printf("\nUsage: sea_claw [subcommand] [options]\n\n");
    printf("  \033[1mSubcommands:\033[0m\n");
    printf("    doctor     Diagnose config, providers, channels\n");
    printf("    onboard    Interactive first-run setup wizard\n");
    printf("    version    Show version information\n");
    printf("    help       Show this help\n");
    printf("\n  \033[1mLegacy flags:\033[0m\n");
    printf("    --gateway           Run in gateway mode\n");
    printf("    --telegram <token>  Run as Telegram bot\n");
    printf("    --chat <id>         Restrict to chat ID\n");
    printf("    --db <path>         Database file\n");
    printf("    --config <path>     Config file\n");
    printf("    --mode <role>       Mesh mode (captain|crew)\n");
    printf("    -h, --help          Show this help\n\n");
    return 0;
}

/* ── .env loader (shared with main.c) ─────────────────────── */

static void cli_load_dotenv(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return;
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\0') continue;
        char* eq = strchr(p, '=');
        if (!eq) continue;
        *eq = '\0';
        char* key = p;
        char* val = eq + 1;
        char* ke = eq - 1;
        while (ke > key && (*ke == ' ' || *ke == '\t')) *ke-- = '\0';
        while (*val == ' ' || *val == '\t') val++;
        size_t vlen = strlen(val);
        while (vlen > 0 && (val[vlen-1] == '\n' || val[vlen-1] == '\r' ||
               val[vlen-1] == ' ' || val[vlen-1] == '\t')) val[--vlen] = '\0';
        if (vlen >= 2 && ((val[0] == '"' && val[vlen-1] == '"') ||
                          (val[0] == '\'' && val[vlen-1] == '\''))) {
            val[vlen-1] = '\0';
            val++;
        }
        setenv(key, val, 0);
    }
    fclose(f);
}

/* ── Built-in: doctor ─────────────────────────────────────── */

int sea_cmd_doctor(int argc, char** argv) {
    (void)argc; (void)argv;

    cli_load_dotenv(".env");
    SeaArena da;
    sea_arena_create(&da, 8192);
    SeaConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    sea_config_load(&cfg, "config.json", &da);

    printf("\n\033[1m  Sea-Claw Doctor\033[0m\n");
    printf("  ════════════════════════════════════════\n\n");
    printf("  \033[1mBinary:\033[0m        %s\n", SEA_VERSION_STRING);
    printf("  \033[1mConfig file:\033[0m   config.json\n");
    printf("  \033[1mDB path:\033[0m       %s\n", cfg.db_path ? cfg.db_path : "seaclaw.db");
    printf("  \033[1mArena size:\033[0m    %u MB\n", cfg.arena_size_mb > 0 ? cfg.arena_size_mb : 16);

    printf("\n  \033[1mLLM Provider:\033[0m\n");
    printf("    provider:  %s %s\n", cfg.llm_provider ? cfg.llm_provider : "(not set)",
           cfg.llm_provider ? "\033[32m✓\033[0m" : "\033[31m✗\033[0m");
    printf("    api_key:   %s\n", (cfg.llm_api_key && *cfg.llm_api_key) ? "\033[32m✓ set\033[0m" : "\033[31m✗ missing\033[0m");
    printf("    model:     %s\n", cfg.llm_model ? cfg.llm_model : "(default)");
    printf("    api_url:   %s\n", cfg.llm_api_url ? cfg.llm_api_url : "(default)");

    printf("\n  \033[1mTelegram:\033[0m\n");
    const char* tg_env = getenv("TELEGRAM_BOT_TOKEN");
    bool tg_ok = (cfg.telegram_token && *cfg.telegram_token) || (tg_env && *tg_env);
    printf("    token:     %s\n", tg_ok ? "\033[32m✓ set\033[0m" : "\033[33m○ not set (optional)\033[0m");
    printf("    chat_id:   %lld %s\n", (long long)cfg.telegram_chat_id,
           cfg.telegram_chat_id ? "\033[32m✓\033[0m" : "\033[33m○\033[0m");

    printf("\n  \033[1mFallbacks:\033[0m     %u configured\n", cfg.llm_fallback_count);

    printf("\n  \033[1mEnvironment:\033[0m\n");
    const char* env_keys[] = {"OPENAI_API_KEY","ANTHROPIC_API_KEY","GEMINI_API_KEY","OPENROUTER_API_KEY","ZAI_API_KEY","TELEGRAM_BOT_TOKEN","EXA_API_KEY"};
    for (int e = 0; e < 7; e++) {
        const char* v = getenv(env_keys[e]);
        printf("    %-24s %s\n", env_keys[e], (v && *v) ? "\033[32m✓\033[0m" : "\033[90m-\033[0m");
    }

    printf("\n  \033[1mDatabase:\033[0m\n");
    SeaDb* ddb = NULL;
    const char* dbp = cfg.db_path ? cfg.db_path : "seaclaw.db";
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

    printf("\n  \033[1mTools:\033[0m         63 compiled in\n");

    /* API connectivity test */
    printf("\n  \033[1mAPI Connectivity:\033[0m\n");
    {
        const char* prov = cfg.llm_provider ? cfg.llm_provider : "openai";
        const char* key = cfg.llm_api_key;
        /* Check env var override */
        if (!key || !*key) {
            if (strcmp(prov, "openai") == 0)      key = getenv("OPENAI_API_KEY");
            else if (strcmp(prov, "anthropic") == 0)  key = getenv("ANTHROPIC_API_KEY");
            else if (strcmp(prov, "gemini") == 0)     key = getenv("GEMINI_API_KEY");
            else if (strcmp(prov, "openrouter") == 0) key = getenv("OPENROUTER_API_KEY");
            else if (strcmp(prov, "zai") == 0)        key = getenv("ZAI_API_KEY");
        }
        if (strcmp(prov, "local") == 0) {
            printf("    %-12s  \033[33m○ local (no key needed)\033[0m\n", prov);
        } else if (key && *key) {
            printf("    %-12s  \033[32m✓ key present\033[0m (%.8s...)\n", prov, key);
        } else {
            printf("    %-12s  \033[31m✗ no API key\033[0m\n", prov);
        }
        /* Check fallback keys */
        for (u32 fi = 0; fi < cfg.llm_fallback_count; fi++) {
            const char* fp = cfg.llm_fallbacks[fi].provider;
            const char* fk = cfg.llm_fallbacks[fi].api_key;
            if (!fk || !*fk) {
                if (fp && strcmp(fp, "openai") == 0)      fk = getenv("OPENAI_API_KEY");
                else if (fp && strcmp(fp, "anthropic") == 0)  fk = getenv("ANTHROPIC_API_KEY");
                else if (fp && strcmp(fp, "gemini") == 0)     fk = getenv("GEMINI_API_KEY");
                else if (fp && strcmp(fp, "openrouter") == 0) fk = getenv("OPENROUTER_API_KEY");
                else if (fp && strcmp(fp, "zai") == 0)        fk = getenv("ZAI_API_KEY");
            }
            if (fp && strcmp(fp, "local") == 0) {
                printf("    %-12s  \033[33m○ local fallback\033[0m\n", fp);
            } else if (fk && *fk) {
                printf("    %-12s  \033[32m✓ fallback key present\033[0m\n", fp ? fp : "?");
            } else {
                printf("    %-12s  \033[31m✗ fallback key missing\033[0m\n", fp ? fp : "?");
            }
        }
    }

    /* Readline status */
    printf("\n  \033[1mFeatures:\033[0m\n");
#ifdef HAVE_READLINE
    printf("    readline:  \033[32m✓ enabled\033[0m (arrow keys, history)\n");
#else
    printf("    readline:  \033[33m○ disabled\033[0m (install libreadline-dev)\n");
#endif
    printf("    streaming: available (/stream on)\n");
    printf("    think:     adjustable (/think off|low|medium|high)\n");

    printf("\n  ════════════════════════════════════════\n\n");
    sea_arena_destroy(&da);
    return 0;
}

/* ── Built-in: onboard ────────────────────────────────────── */

int sea_cmd_onboard(int argc, char** argv) {
    (void)argc; (void)argv;

    printf("\n\033[1m  Sea-Claw Onboard Wizard\033[0m\n");
    printf("  ════════════════════════════════════════\n\n");

    /* Step 1: Personalization */
    printf("  \033[1mStep 1/3 — Who are you?\033[0m\n\n");
    char ob_name[128] = {0}, ob_work[256] = {0}, ob_tone[64] = {0};

    printf("  Your name (or press Enter to skip): ");
    fflush(stdout);
    if (fgets(ob_name, sizeof(ob_name), stdin))
        ob_name[strcspn(ob_name, "\n")] = '\0';
    printf("  What do you do? (e.g. developer, student, business owner): ");
    fflush(stdout);
    if (fgets(ob_work, sizeof(ob_work), stdin))
        ob_work[strcspn(ob_work, "\n")] = '\0';
    printf("  Preferred tone (casual/professional/technical) [professional]: ");
    fflush(stdout);
    if (fgets(ob_tone, sizeof(ob_tone), stdin))
        ob_tone[strcspn(ob_tone, "\n")] = '\0';
    if (!ob_tone[0]) strncpy(ob_tone, "professional", sizeof(ob_tone) - 1);

    /* Write USER.md + SOUL.md */
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
                fprintf(sf, "- Friendly and relaxed\n- Use simple language\n- Brief and informal\n");
            } else if (strcmp(ob_tone, "technical") == 0) {
                fprintf(sf, "- Precise and detailed\n- Technical terminology freely\n- Code examples when relevant\n");
            } else {
                fprintf(sf, "- Professional but approachable\n- Technical when needed, simple when possible\n");
            }
            if (ob_name[0]) fprintf(sf, "\n## User\n- Address the user as %s\n", ob_name);
            fclose(sf);
            printf("  \033[32m✓\033[0m Personality configured (%s tone)\n", ob_tone);
        }
    }

    /* Seed recall DB */
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
                printf("  \033[32m✓\033[0m Memory seeded (%s)\n", ob_name[0] ? ob_name : "anonymous");
            }
            sea_db_close(ob_db);
        }
    }

    /* Step 2: LLM Provider */
    printf("\n  \033[1mStep 2/3 — LLM Provider\033[0m\n\n");
    char ob_provider[64] = {0}, ob_key[256] = {0}, ob_model[128] = {0};

    printf("  Provider (openai/anthropic/gemini/openrouter/zai/local): ");
    fflush(stdout);
    if (fgets(ob_provider, sizeof(ob_provider), stdin))
        ob_provider[strcspn(ob_provider, "\n")] = '\0';
    char ob_api_url[256] = {0};
    if (strcmp(ob_provider, "local") == 0) {
        printf("  No API key needed for local LLM.\n");
        printf("  Ollama URL [http://localhost:11434]: ");
        fflush(stdout);
        if (fgets(ob_api_url, sizeof(ob_api_url), stdin))
            ob_api_url[strcspn(ob_api_url, "\n")] = '\0';
    } else {
        printf("  API Key: ");
        fflush(stdout);
        if (fgets(ob_key, sizeof(ob_key), stdin))
            ob_key[strcspn(ob_key, "\n")] = '\0';
    }
    printf("  Model (or press Enter for default): ");
    fflush(stdout);
    if (fgets(ob_model, sizeof(ob_model), stdin))
        ob_model[strcspn(ob_model, "\n")] = '\0';

    /* Step 3: Telegram */
    printf("\n  \033[1mStep 3/3 — Telegram Bot (optional)\033[0m\n\n");
    char ob_tg_token[256] = {0}, ob_tg_chat[32] = {0};

    printf("  Bot Token (or press Enter to skip): ");
    fflush(stdout);
    if (fgets(ob_tg_token, sizeof(ob_tg_token), stdin))
        ob_tg_token[strcspn(ob_tg_token, "\n")] = '\0';
    if (ob_tg_token[0]) {
        printf("  Chat ID: ");
        fflush(stdout);
        if (fgets(ob_tg_chat, sizeof(ob_tg_chat), stdin))
            ob_tg_chat[strcspn(ob_tg_chat, "\n")] = '\0';
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
    printf("  Run \033[1m./sea_claw doctor\033[0m to verify.\n\n");
    if (ob_name[0]) printf("  Welcome aboard, %s. The Vault remembers.\n\n", ob_name);
    return 0;
}
