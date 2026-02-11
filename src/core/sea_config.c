/*
 * sea_config.c — JSON Configuration Loader
 *
 * Reads config.json using our own sea_json parser.
 * All string values are stored in the provided arena.
 */

#include "seaclaw/sea_config.h"
#include "seaclaw/sea_json.h"
#include "seaclaw/sea_log.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ── Defaults ─────────────────────────────────────────────── */

void sea_config_defaults(SeaConfig* cfg) {
    if (!cfg) return;
    if (!cfg->db_path)          cfg->db_path = "seaclaw.db";
    if (!cfg->log_level)        cfg->log_level = "info";
    if (cfg->arena_size_mb == 0) cfg->arena_size_mb = 16;
}

/* ── Load ─────────────────────────────────────────────────── */

SeaError sea_config_load(SeaConfig* cfg, const char* path, SeaArena* arena) {
    if (!cfg || !path || !arena) return SEA_ERR_CONFIG;

    memset(cfg, 0, sizeof(SeaConfig));

    /* Read file into arena */
    FILE* f = fopen(path, "rb");
    if (!f) {
        SEA_LOG_WARN("CONFIG", "Config file not found: %s (using defaults)", path);
        sea_config_defaults(cfg);
        return SEA_ERR_IO;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize <= 0 || fsize > 65536) {
        SEA_LOG_ERROR("CONFIG", "Config file invalid size: %ld", fsize);
        fclose(f);
        sea_config_defaults(cfg);
        return SEA_ERR_CONFIG;
    }

    char* buf = (char*)sea_arena_alloc(arena, (u64)fsize + 1, 1);
    if (!buf) {
        fclose(f);
        sea_config_defaults(cfg);
        return SEA_ERR_OOM;
    }

    size_t read = fread(buf, 1, (size_t)fsize, f);
    fclose(f);
    buf[read] = '\0';

    /* Parse JSON */
    SeaSlice input = { .data = (const u8*)buf, .len = (u32)read };
    SeaJsonValue root;
    SeaError jerr = sea_json_parse(input, arena, &root);
    if (jerr != SEA_OK || root.type != SEA_JSON_OBJECT) {
        SEA_LOG_ERROR("CONFIG", "Failed to parse config JSON");
        sea_config_defaults(cfg);
        return SEA_ERR_PARSE;
    }

    /* Helper: copy a SeaSlice into arena as null-terminated C string */
    #define SLICE_TO_CSTR(slice) do { \
        if ((slice).len > 0 && (slice).data) { \
            char* _s = (char*)sea_arena_alloc(arena, (slice).len + 1, 1); \
            if (_s) { memcpy(_s, (slice).data, (slice).len); _s[(slice).len] = '\0'; _dst = _s; } \
        } \
    } while(0)

    /* Extract fields using convenience accessors */
    SeaSlice sv;
    const char* _dst;

    _dst = NULL;
    sv = sea_json_get_string(&root, "telegram_token");
    SLICE_TO_CSTR(sv);
    if (_dst) cfg->telegram_token = _dst;

    cfg->telegram_chat_id = (i64)sea_json_get_number(&root, "telegram_chat_id", 0.0);

    _dst = NULL;
    sv = sea_json_get_string(&root, "db_path");
    SLICE_TO_CSTR(sv);
    if (_dst) cfg->db_path = _dst;

    _dst = NULL;
    sv = sea_json_get_string(&root, "log_level");
    SLICE_TO_CSTR(sv);
    if (_dst) cfg->log_level = _dst;

    cfg->arena_size_mb = (u32)sea_json_get_number(&root, "arena_size_mb", 0.0);

    _dst = NULL;
    sv = sea_json_get_string(&root, "llm_provider");
    SLICE_TO_CSTR(sv);
    if (_dst) cfg->llm_provider = _dst;

    _dst = NULL;
    sv = sea_json_get_string(&root, "llm_api_key");
    SLICE_TO_CSTR(sv);
    if (_dst) cfg->llm_api_key = _dst;

    _dst = NULL;
    sv = sea_json_get_string(&root, "llm_model");
    SLICE_TO_CSTR(sv);
    if (_dst) cfg->llm_model = _dst;

    _dst = NULL;
    sv = sea_json_get_string(&root, "llm_api_url");
    SLICE_TO_CSTR(sv);
    if (_dst) cfg->llm_api_url = _dst;

    /* Parse fallback providers array */
    cfg->llm_fallback_count = 0;
    const SeaJsonValue* fallbacks = sea_json_get(&root, "llm_fallbacks");
    if (fallbacks && fallbacks->type == SEA_JSON_ARRAY) {
        u32 max_fb = fallbacks->array.count;
        if (max_fb > 4) max_fb = 4;
        for (u32 fi = 0; fi < max_fb; fi++) {
            const SeaJsonValue* fb = &fallbacks->array.items[fi];
            if (fb->type != SEA_JSON_OBJECT) continue;

            _dst = NULL;
            sv = sea_json_get_string(fb, "provider");
            SLICE_TO_CSTR(sv);
            cfg->llm_fallbacks[cfg->llm_fallback_count].provider = _dst;

            _dst = NULL;
            sv = sea_json_get_string(fb, "api_key");
            SLICE_TO_CSTR(sv);
            cfg->llm_fallbacks[cfg->llm_fallback_count].api_key = _dst;

            _dst = NULL;
            sv = sea_json_get_string(fb, "model");
            SLICE_TO_CSTR(sv);
            cfg->llm_fallbacks[cfg->llm_fallback_count].model = _dst;

            _dst = NULL;
            sv = sea_json_get_string(fb, "api_url");
            SLICE_TO_CSTR(sv);
            cfg->llm_fallbacks[cfg->llm_fallback_count].api_url = _dst;

            cfg->llm_fallback_count++;
        }
    }

    #undef SLICE_TO_CSTR

    /* Fill defaults for anything not specified */
    sea_config_defaults(cfg);
    cfg->loaded = true;

    SEA_LOG_INFO("CONFIG", "Loaded config from %s", path);
    return SEA_OK;
}

/* ── Print ────────────────────────────────────────────────── */

void sea_config_print(const SeaConfig* cfg) {
    if (!cfg) return;
    printf("\n  \033[1mConfiguration:\033[0m\n");
    printf("    telegram_token:   %s\n", cfg->telegram_token ? "***set***" : "(not set)");
    printf("    telegram_chat_id: %lld\n", (long long)cfg->telegram_chat_id);
    printf("    db_path:          %s\n", cfg->db_path ? cfg->db_path : "(default)");
    printf("    log_level:        %s\n", cfg->log_level ? cfg->log_level : "info");
    printf("    arena_size_mb:    %u\n", cfg->arena_size_mb);
    printf("    llm_provider:     %s\n", cfg->llm_provider ? cfg->llm_provider : "(not set)");
    printf("    llm_api_key:      %s\n", cfg->llm_api_key ? "***set***" : "(not set)");
    printf("    llm_model:        %s\n", cfg->llm_model ? cfg->llm_model : "(default)");
    printf("    llm_api_url:      %s\n", cfg->llm_api_url ? cfg->llm_api_url : "(default)");
    printf("\n");
}
