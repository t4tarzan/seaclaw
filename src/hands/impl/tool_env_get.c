/*
 * tool_env_get.c — Get environment variable value
 *
 * Args: variable name
 * Returns: variable value or "not set"
 *
 * Security: only allows reading whitelisted env vars.
 */

#include "seaclaw/sea_tools.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Whitelist of safe env vars to expose */
static const char* s_whitelist[] = {
    "HOME", "USER", "SHELL", "LANG", "PATH", "PWD", "HOSTNAME",
    "TERM", "TZ", "LC_ALL", "LC_CTYPE",
    NULL
};

static bool is_whitelisted(const char* name) {
    for (int i = 0; s_whitelist[i]; i++) {
        if (strcmp(name, s_whitelist[i]) == 0) return true;
    }
    return false;
}

SeaError tool_env_get(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Usage: <VAR_NAME>\nAllowed: HOME, USER, SHELL, LANG, PATH, PWD, HOSTNAME, TERM, TZ");
        return SEA_OK;
    }

    char name[128];
    u32 nlen = args.len < sizeof(name) - 1 ? args.len : (u32)(sizeof(name) - 1);
    memcpy(name, args.data, nlen);
    name[nlen] = '\0';

    /* Trim whitespace */
    char* n = name;
    while (*n == ' ') n++;
    char* end = n + strlen(n) - 1;
    while (end > n && (*end == ' ' || *end == '\n')) *end-- = '\0';

    if (!is_whitelisted(n)) {
        char buf[256];
        int len = snprintf(buf, sizeof(buf), "Error: '%s' is not in the allowed whitelist", n);
        u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)len);
        if (!dst) return SEA_ERR_ARENA_FULL;
        output->data = dst; output->len = (u32)len;
        return SEA_OK;
    }

    const char* val = getenv(n);
    if (!val) {
        char buf[128];
        int len = snprintf(buf, sizeof(buf), "%s: (not set)", n);
        u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)len);
        if (!dst) return SEA_ERR_ARENA_FULL;
        output->data = dst; output->len = (u32)len;
        return SEA_OK;
    }

    char buf[2048];
    int len = snprintf(buf, sizeof(buf), "%s=%s", n, val);
    u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)len);
    if (!dst) return SEA_ERR_ARENA_FULL;
    output->data = dst; output->len = (u32)len;
    return SEA_OK;
}
