/*
 * tool_google.c — Google Suite Tool Bridge
 *
 * Wraps the gogcli binary (github.com/steipete/gogcli) to provide
 * Gmail, Calendar, Drive, Contacts, and Tasks access as SeaBot tools.
 *
 * Requires `gog` binary in PATH. If not installed, returns a helpful
 * error message with install instructions.
 *
 * All commands run with --json output for machine-readable results.
 */

#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_log.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ── Helpers ──────────────────────────────────────────────── */

/* Check if gog binary is available */
static bool gog_available(void) {
    return system("command -v gog >/dev/null 2>&1") == 0;
}

/* Run a gog command and capture output into arena */
static SeaError gog_exec(const char* subcmd, SeaArena* arena, SeaSlice* output) {
    if (!gog_available()) {
        const char* msg =
            "{\"error\":\"gogcli not installed\","
            "\"install\":\"brew install steipete/tap/gogcli (macOS) or "
            "go install github.com/steipete/gogcli@latest (any)\","
            "\"url\":\"https://github.com/steipete/gogcli\"}";
        u32 len = (u32)strlen(msg);
        u8* buf = (u8*)sea_arena_push_bytes(arena, (const u8*)msg, len);
        if (!buf) return SEA_ERR_ARENA_FULL;
        output->data = buf;
        output->len = len;
        return SEA_OK;
    }

    /* Build command with JSON output and 30s timeout */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "timeout 30 gog %s --json 2>&1", subcmd);

    FILE* fp = popen(cmd, "r");
    if (!fp) return SEA_ERR_IO;

    /* Read output into arena */
    char tmp[4096];
    u32 total = 0;
    u8* start = (u8*)sea_arena_alloc(arena, 0, 1); /* Mark position */

    while (fgets(tmp, sizeof(tmp), fp)) {
        u32 len = (u32)strlen(tmp);
        u8* chunk = (u8*)sea_arena_push_bytes(arena, (const u8*)tmp, len);
        if (!chunk) { pclose(fp); return SEA_ERR_ARENA_FULL; }
        if (total == 0) start = chunk;
        total += len;
    }

    int rc = pclose(fp);
    if (total == 0) {
        const char* msg = rc == 0
            ? "{\"result\":\"ok\",\"output\":\"(empty)\"}"
            : "{\"error\":\"gog command failed\"}";
        u32 len = (u32)strlen(msg);
        u8* buf = (u8*)sea_arena_push_bytes(arena, (const u8*)msg, len);
        if (!buf) return SEA_ERR_ARENA_FULL;
        output->data = buf;
        output->len = len;
    } else {
        output->data = start;
        output->len = total;
    }

    return SEA_OK;
}

/* ── Tool: google_gmail_search ────────────────────────────── */

SeaError tool_google_gmail_search(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("{\"error\":\"usage: google_gmail_search <query>\"}");
        return SEA_OK;
    }

    char subcmd[512];
    snprintf(subcmd, sizeof(subcmd), "gmail search '%.400s' --max 10",
             (const char*)args.data);
    return gog_exec(subcmd, arena, output);
}

/* ── Tool: google_calendar_today ──────────────────────────── */

SeaError tool_google_calendar_today(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    (void)args;
    return gog_exec("calendar events --today", arena, output);
}

/* ── Tool: google_drive_search ────────────────────────────── */

SeaError tool_google_drive_search(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("{\"error\":\"usage: google_drive_search <query>\"}");
        return SEA_OK;
    }

    char subcmd[512];
    snprintf(subcmd, sizeof(subcmd), "drive list --search '%.400s' --max 10",
             (const char*)args.data);
    return gog_exec(subcmd, arena, output);
}

/* ── Tool: google_contacts_search ─────────────────────────── */

SeaError tool_google_contacts_search(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("{\"error\":\"usage: google_contacts_search <query>\"}");
        return SEA_OK;
    }

    char subcmd[512];
    snprintf(subcmd, sizeof(subcmd), "contacts search '%.400s'",
             (const char*)args.data);
    return gog_exec(subcmd, arena, output);
}

/* ── Tool: google_tasks_list ──────────────────────────────── */

SeaError tool_google_tasks_list(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    (void)args;
    return gog_exec("tasks list", arena, output);
}
