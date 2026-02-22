/*
 * sea_ext.c — Extension Registry Implementation
 *
 * Manages registration, lifecycle, and health scoring
 * for all SeaClaw extensions (tools, channels, memory, providers).
 */

#include "seaclaw/sea_ext.h"
#include "seaclaw/sea_log.h"

#include <stdio.h>
#include <string.h>

/* ── Registry Init ────────────────────────────────────────── */

void sea_ext_init(SeaExtRegistry* reg) {
    if (!reg) return;
    memset(reg, 0, sizeof(*reg));
}

/* ── Register ─────────────────────────────────────────────── */

SeaError sea_ext_register(SeaExtRegistry* reg, SeaExtension* ext) {
    if (!reg || !ext) return SEA_ERR_INVALID_INPUT;
    if (reg->count >= SEA_EXT_MAX) return SEA_ERR_FULL;

    /* Check for duplicate name */
    for (u32 i = 0; i < reg->count; i++) {
        if (reg->extensions[i] &&
            strcmp(reg->extensions[i]->name, ext->name) == 0) {
            SEA_LOG_WARN("EXT", "Extension already registered: %s", ext->name);
            return SEA_ERR_ALREADY_EXISTS;
        }
    }

    reg->extensions[reg->count++] = ext;
    SEA_LOG_INFO("EXT", "Registered: %s v%s (type=%d)",
                 ext->name, ext->version, ext->type);
    return SEA_OK;
}

/* ── Find ─────────────────────────────────────────────────── */

SeaExtension* sea_ext_find(const SeaExtRegistry* reg, const char* name) {
    if (!reg || !name) return NULL;
    for (u32 i = 0; i < reg->count; i++) {
        if (reg->extensions[i] &&
            strcmp(reg->extensions[i]->name, name) == 0) {
            return reg->extensions[i];
        }
    }
    return NULL;
}

/* ── Init All ─────────────────────────────────────────────── */

SeaError sea_ext_init_all(SeaExtRegistry* reg, SeaArena* arena) {
    if (!reg) return SEA_ERR_INVALID_INPUT;

    u32 ok = 0, fail = 0;
    for (u32 i = 0; i < reg->count; i++) {
        SeaExtension* ext = reg->extensions[i];
        if (!ext || !ext->init) continue;

        SeaError err = ext->init(ext, arena);
        if (err == SEA_OK) {
            ext->enabled = true;
            ok++;
        } else {
            ext->enabled = false;
            fail++;
            SEA_LOG_WARN("EXT", "Init failed: %s (%s)",
                         ext->name, sea_error_str(err));
        }
    }

    SEA_LOG_INFO("EXT", "Extensions initialized: %u ok, %u failed", ok, fail);
    return fail > 0 ? SEA_ERR_PARTIAL : SEA_OK;
}

/* ── Destroy All ──────────────────────────────────────────── */

void sea_ext_destroy_all(SeaExtRegistry* reg) {
    if (!reg) return;
    for (u32 i = 0; i < reg->count; i++) {
        SeaExtension* ext = reg->extensions[i];
        if (ext && ext->destroy) {
            ext->destroy(ext);
            ext->enabled = false;
        }
    }
}

/* ── Count ────────────────────────────────────────────────── */

u32 sea_ext_count(const SeaExtRegistry* reg) {
    return reg ? reg->count : 0;
}

u32 sea_ext_count_by_type(const SeaExtRegistry* reg, SeaExtType type) {
    if (!reg) return 0;
    u32 n = 0;
    for (u32 i = 0; i < reg->count; i++) {
        if (reg->extensions[i] && reg->extensions[i]->type == type) n++;
    }
    return n;
}

/* ── Health Score ─────────────────────────────────────────── */

i32 sea_ext_health(const SeaExtRegistry* reg) {
    if (!reg || reg->count == 0) return 100; /* No extensions = healthy */

    i32 total = 0;
    u32 checked = 0;
    for (u32 i = 0; i < reg->count; i++) {
        SeaExtension* ext = reg->extensions[i];
        if (!ext) continue;
        if (ext->health) {
            total += ext->health(ext);
            checked++;
        } else if (ext->enabled) {
            total += 100; /* Enabled but no health check = assume healthy */
            checked++;
        }
    }

    return checked > 0 ? total / (i32)checked : 100;
}

/* ── List ─────────────────────────────────────────────────── */

static const char* ext_type_name(SeaExtType type) {
    switch (type) {
        case SEA_EXT_TYPE_TOOL:     return "tool";
        case SEA_EXT_TYPE_CHANNEL:  return "channel";
        case SEA_EXT_TYPE_MEMORY:   return "memory";
        case SEA_EXT_TYPE_PROVIDER: return "provider";
        default:                    return "unknown";
    }
}

void sea_ext_list(const SeaExtRegistry* reg) {
    if (!reg) return;
    printf("  Extensions (%u):\n", reg->count);
    for (u32 i = 0; i < reg->count; i++) {
        SeaExtension* ext = reg->extensions[i];
        if (!ext) continue;
        const char* icon = ext->enabled ? "\033[32m●\033[0m" : "\033[31m●\033[0m";
        printf("    %s %-20s v%-8s [%s]\n",
               icon, ext->name, ext->version, ext_type_name(ext->type));
    }
}
