/*
 * sea_auth.c — Authentication & Token Framework Implementation
 *
 * Token generation, validation, and permission checking.
 */

#include "seaclaw/sea_auth.h"
#include "seaclaw/sea_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ── Token Generation ─────────────────────────────────────── */

static void generate_token_string(char* out) {
    /* Generate a 64-char hex token from /dev/urandom */
    FILE* f = fopen("/dev/urandom", "rb");
    if (f) {
        u8 bytes[32];
        if (fread(bytes, 1, 32, f) == 32) {
            for (int i = 0; i < 32; i++) {
                snprintf(out + i * 2, 3, "%02x", bytes[i]);
            }
            fclose(f);
            return;
        }
        fclose(f);
    }
    /* Fallback: time-based (less secure, but functional) */
    srand((unsigned int)time(NULL));
    for (int i = 0; i < 64; i++) {
        const char hex[] = "0123456789abcdef";
        out[i] = hex[rand() % 16];
    }
    out[64] = '\0';
}

/* ── Init ─────────────────────────────────────────────────── */

void sea_auth_init(SeaAuth* auth, bool enabled) {
    if (!auth) return;
    memset(auth, 0, sizeof(*auth));
    auth->enabled = enabled;
    SEA_LOG_INFO("AUTH", "Token auth %s", enabled ? "enabled" : "disabled (dev mode)");
}

/* ── Create Token ─────────────────────────────────────────── */

SeaError sea_auth_create_token(SeaAuth* auth, const char* label,
                                u32 permissions, i64 expires_at,
                                char* out_token) {
    if (!auth || !out_token) return SEA_ERR_INVALID_INPUT;
    if (auth->count >= SEA_AUTH_MAX_TOKENS) return SEA_ERR_FULL;

    SeaAuthToken* t = &auth->tokens[auth->count];
    generate_token_string(t->token);
    t->token[SEA_TOKEN_LEN] = '\0';

    if (label) {
        strncpy(t->label, label, SEA_TOKEN_LABEL_MAX - 1);
        t->label[SEA_TOKEN_LABEL_MAX - 1] = '\0';
    }

    t->permissions = permissions;
    t->created_at = (i64)time(NULL);
    t->expires_at = expires_at;
    t->revoked = false;

    memcpy(out_token, t->token, SEA_TOKEN_LEN + 1);
    auth->count++;

    SEA_LOG_INFO("AUTH", "Token created: %s (perms=0x%02x, expires=%lld)",
                 t->label, permissions, (long long)expires_at);
    return SEA_OK;
}

/* ── Validate ─────────────────────────────────────────────── */

u32 sea_auth_validate(const SeaAuth* auth, const char* token) {
    if (!auth || !token) return 0;

    /* If auth is disabled, grant all permissions */
    if (!auth->enabled) return SEA_PERM_ALL;

    i64 now = (i64)time(NULL);

    for (u32 i = 0; i < auth->count; i++) {
        const SeaAuthToken* t = &auth->tokens[i];
        if (t->revoked) continue;
        if (t->expires_at > 0 && t->expires_at < now) continue;
        if (strcmp(t->token, token) == 0) {
            return t->permissions;
        }
    }
    return 0;
}

/* ── Has Permission ───────────────────────────────────────── */

bool sea_auth_has_perm(const SeaAuth* auth, const char* token, SeaPerm perm) {
    u32 perms = sea_auth_validate(auth, token);
    return (perms & (u32)perm) != 0;
}

/* ── Revoke ───────────────────────────────────────────────── */

SeaError sea_auth_revoke(SeaAuth* auth, const char* token) {
    if (!auth || !token) return SEA_ERR_INVALID_INPUT;

    for (u32 i = 0; i < auth->count; i++) {
        if (strcmp(auth->tokens[i].token, token) == 0) {
            auth->tokens[i].revoked = true;
            SEA_LOG_INFO("AUTH", "Token revoked: %s", auth->tokens[i].label);
            return SEA_OK;
        }
    }
    return SEA_ERR_NOT_FOUND;
}

/* ── List ─────────────────────────────────────────────────── */

u32 sea_auth_list(const SeaAuth* auth, SeaAuthToken* out, u32 max) {
    if (!auth || !out) return 0;
    u32 n = 0;
    for (u32 i = 0; i < auth->count && n < max; i++) {
        out[n] = auth->tokens[i];
        /* Mask the token string for security — only show first 8 chars */
        memset(out[n].token + 8, '*', SEA_TOKEN_LEN - 8);
        out[n].token[SEA_TOKEN_LEN] = '\0';
        n++;
    }
    return n;
}

/* ── Active Count ─────────────────────────────────────────── */

u32 sea_auth_active_count(const SeaAuth* auth) {
    if (!auth) return 0;
    u32 n = 0;
    i64 now = (i64)time(NULL);
    for (u32 i = 0; i < auth->count; i++) {
        if (!auth->tokens[i].revoked &&
            (auth->tokens[i].expires_at == 0 || auth->tokens[i].expires_at > now)) {
            n++;
        }
    }
    return n;
}

/* ── Tool Allowlist ───────────────────────────────────────── */

SeaError sea_auth_allow_tool(SeaAuth* auth, const char* token, const char* tool_name) {
    if (!auth || !token || !tool_name) return SEA_ERR_INVALID_INPUT;

    for (u32 i = 0; i < auth->count; i++) {
        if (strcmp(auth->tokens[i].token, token) == 0) {
            SeaAuthToken* t = &auth->tokens[i];
            if (t->allowed_tool_count >= SEA_AUTH_MAX_ALLOWED_TOOLS) {
                return SEA_ERR_FULL;
            }
            /* Check for duplicate */
            for (u32 j = 0; j < t->allowed_tool_count; j++) {
                if (strcmp(t->allowed_tools[j], tool_name) == 0) {
                    return SEA_ERR_ALREADY_EXISTS;
                }
            }
            strncpy(t->allowed_tools[t->allowed_tool_count], tool_name,
                    SEA_AUTH_TOOL_NAME_MAX - 1);
            t->allowed_tools[t->allowed_tool_count][SEA_AUTH_TOOL_NAME_MAX - 1] = '\0';
            t->allowed_tool_count++;
            SEA_LOG_INFO("AUTH", "Token '%s': allowed tool '%s' (%u/%u)",
                         t->label, tool_name, t->allowed_tool_count,
                         (u32)SEA_AUTH_MAX_ALLOWED_TOOLS);
            return SEA_OK;
        }
    }
    return SEA_ERR_NOT_FOUND;
}

bool sea_auth_can_call_tool(const SeaAuth* auth, const char* token,
                             const char* tool_name) {
    if (!auth || !token || !tool_name) return false;

    /* If auth disabled, allow everything */
    if (!auth->enabled) return true;

    /* Find the token */
    i64 now = (i64)time(NULL);
    for (u32 i = 0; i < auth->count; i++) {
        const SeaAuthToken* t = &auth->tokens[i];
        if (t->revoked) continue;
        if (t->expires_at > 0 && t->expires_at < now) continue;
        if (strcmp(t->token, token) != 0) continue;

        /* Must have TOOLS permission */
        if (!(t->permissions & SEA_PERM_TOOLS)) return false;

        /* Empty allowlist = all tools allowed */
        if (t->allowed_tool_count == 0) return true;

        /* Check allowlist */
        for (u32 j = 0; j < t->allowed_tool_count; j++) {
            if (strcmp(t->allowed_tools[j], tool_name) == 0) return true;
        }
        return false; /* Tool not in allowlist */
    }
    return false; /* Token not found */
}
