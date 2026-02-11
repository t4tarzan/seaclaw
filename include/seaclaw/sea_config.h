/*
 * sea_config.h — JSON Configuration Loader
 *
 * Reads a JSON config file and provides typed accessors.
 * Config values are also persisted to the SQLite database
 * so they survive across sessions.
 *
 * Example config.json:
 * {
 *   "telegram_token": "123456:ABC...",
 *   "telegram_chat_id": 99887766,
 *   "db_path": "seaclaw.db",
 *   "log_level": "info",
 *   "arena_size_mb": 16
 * }
 */

#ifndef SEA_CONFIG_H
#define SEA_CONFIG_H

#include "sea_types.h"
#include "sea_arena.h"

typedef struct {
    /* Telegram */
    const char* telegram_token;
    i64         telegram_chat_id;

    /* Database */
    const char* db_path;

    /* System */
    const char* log_level;
    u32         arena_size_mb;

    /* State */
    bool        loaded;
} SeaConfig;

/* Load config from a JSON file. Arena used for string storage. */
SeaError sea_config_load(SeaConfig* cfg, const char* path, SeaArena* arena);

/* Apply defaults for any unset fields */
void sea_config_defaults(SeaConfig* cfg);

/* Print config summary to stdout */
void sea_config_print(const SeaConfig* cfg);

#endif /* SEA_CONFIG_H */
