/*
 * tool_spawn_worker.c — Spawn ephemeral worker pods via gateway API
 *
 * Three tools:
 *   swarm_spawn   Args: <task_description> [worker_name] [soul]
 *                       Calls POST /api/v1/agents/{coordinator}/workers
 *   swarm_relay   Args: <target_agent> <message>
 *                       Calls POST /api/v1/agents/{target}/relay
 *   swarm_workers Args: (none) — list active workers for this coordinator
 *
 * The coordinator identity comes from env var SEA_USERNAME.
 * The gateway URL comes from env var SEA_GATEWAY_URL (default: http://gateway-svc:8090).
 * The gateway is called via HTTP (reusing the existing sea_http module).
 */

#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_log.h"
#include "seaclaw/sea_http.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define SWARM_OUT_MAX (8 * 1024)

/* ── Helpers ────────────────────────────────────────────────── */

static const char* coordinator_name(void) {
    const char* u = getenv("SEA_USERNAME");
    return u ? u : "unknown";
}

static const char* gateway_url(void) {
    const char* u = getenv("SEA_GATEWAY_URL");
    return u ? u : "http://gateway-svc.seaclaw-platform.svc.cluster.local:8090";
}

/* Parse first token out of args (space-delimited) */
static const char* next_token(const char* s, char* tok, size_t tok_sz) {
    while (*s == ' ' || *s == '\t') s++;
    size_t i = 0;
    while (*s && *s != ' ' && *s != '\t' && i + 1 < tok_sz) tok[i++] = *s++;
    tok[i] = '\0';
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

/* Minimal JSON string escape — replaces " and \ */
static void json_escape(const char* in, char* out, size_t out_sz) {
    size_t j = 0;
    for (size_t i = 0; in[i] && j + 4 < out_sz; i++) {
        if (in[i] == '"' || in[i] == '\\') out[j++] = '\\';
        else if (in[i] == '\n') { out[j++] = '\\'; out[j++] = 'n'; continue; }
        else if (in[i] == '\r') continue;
        out[j++] = in[i];
    }
    out[j] = '\0';
}

/* Simple HTTP POST via curl (available in runtime image) */
static int http_post(const char* url, const char* body, char* resp, size_t resp_sz) {
    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
        "curl -s -X POST '%s' -H 'Content-Type: application/json' -d '%s' 2>&1",
        url, body);

    FILE* pipe = popen(cmd, "r");
    if (!pipe) return -1;

    size_t total = 0;
    while (total < resp_sz - 1) {
        size_t n = fread(resp + total, 1, resp_sz - 1 - total, pipe);
        if (n == 0) break;
        total += n;
    }
    resp[total] = '\0';
    int rc = pclose(pipe);
    return rc;
}

/* http_get is available but not called directly — workers use curl inline */

/* ── swarm_spawn ────────────────────────────────────────────── */

SeaError tool_swarm_spawn(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT(
            "swarm_spawn usage: <task_description> [worker_name] [soul]\n"
            "Example: swarm_spawn analyze the README file for security issues sec-worker alex");
        return SEA_OK;
    }

    char raw[2048];
    u32 len = args.len < sizeof(raw)-1 ? args.len : (u32)(sizeof(raw)-1);
    memcpy(raw, args.data, len); raw[len] = '\0';

    /* First token = task (could be multi-word — take up to 2nd space-delimited block
       if next tokens look like a worker name / soul keyword) */
    char worker_name[64] = {0};
    char soul[32] = "alex";

    /* Strategy: last token is soul if it matches known souls, second-to-last is worker name
       if it has no spaces, rest is task */
    const char* known_souls[] = {"alex","eva","tom","sarah","max", NULL};
    char* tokens[32];
    int ntok = 0;

    char copy[2048];
    strncpy(copy, raw, sizeof(copy)-1); copy[sizeof(copy)-1] = '\0';
    char* p = copy;
    while (ntok < 31 && *p) {
        while (*p == ' ') p++;
        if (!*p) break;
        tokens[ntok++] = p;
        while (*p && *p != ' ') p++;
        if (*p) *p++ = '\0';
    }
    tokens[ntok] = NULL;

    int task_end = ntok;

    /* Check if last token is a soul name */
    if (ntok >= 2) {
        for (int i = 0; known_souls[i]; i++) {
            if (strcmp(tokens[ntok-1], known_souls[i]) == 0) {
                strncpy(soul, tokens[ntok-1], sizeof(soul)-1);
                task_end--;
                break;
            }
        }
    }

    /* Check if now-last token looks like a worker name (no spaces, short) */
    if (task_end >= 2) {
        int last = task_end - 1;
        size_t wlen = strlen(tokens[last]);
        /* Heuristic: if it's a single word ≤20 chars and not a common English word, treat as worker name */
        if (wlen <= 20 && wlen >= 2 && strchr(tokens[last], '-') == NULL) {
            /* Only use as worker name if user provided exactly 3+ tokens */
            if (task_end >= 3) {
                strncpy(worker_name, tokens[last], sizeof(worker_name)-1);
                task_end--;
            }
        }
    }

    /* Rebuild task from remaining tokens */
    char task[1024] = {0};
    for (int i = 0; i < task_end; i++) {
        if (i > 0) strncat(task, " ", sizeof(task)-strlen(task)-1);
        strncat(task, tokens[i], sizeof(task)-strlen(task)-1);
    }

    const char* coord = coordinator_name();
    const char* gw    = gateway_url();

    char task_esc[1024], wname_esc[128], soul_esc[64];
    json_escape(task,        task_esc,  sizeof(task_esc));
    json_escape(worker_name, wname_esc, sizeof(wname_esc));
    json_escape(soul,        soul_esc,  sizeof(soul_esc));

    char body[2048];
    if (worker_name[0] != '\0') {
        snprintf(body, sizeof(body),
            "{\"task\":\"%s\",\"worker_name\":\"%s\",\"soul\":\"%s\",\"ttl_seconds\":300}",
            task_esc, wname_esc, soul_esc);
    } else {
        snprintf(body, sizeof(body),
            "{\"task\":\"%s\",\"soul\":\"%s\",\"ttl_seconds\":300}",
            task_esc, soul_esc);
    }

    char url[512];
    snprintf(url, sizeof(url), "%s/api/v1/agents/%s/workers", gw, coord);

    SEA_LOG_INFO("SWARM", "spawn worker: coord=%s task=%s", coord, task);

    char resp[SWARM_OUT_MAX];
    http_post(url, body, resp, sizeof(resp));

    /* Format output */
    char out[SWARM_OUT_MAX];
    int olen = snprintf(out, sizeof(out),
        "Spawn request sent to gateway.\nCoordinator: %s\nTask: %s\nSoul: %s\nResponse: %s",
        coord, task, soul, resp);

    u8* dst = (u8*)sea_arena_push_bytes(arena, out, (u64)olen);
    if (dst) { output->data = dst; output->len = (u32)olen; }
    return SEA_OK;
}

/* ── swarm_relay ────────────────────────────────────────────── */

SeaError tool_swarm_relay(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("swarm_relay usage: <target_agent> <message>\nExample: swarm_relay alec-worker1 Here is my analysis result: ...");
        return SEA_OK;
    }

    char raw[4096];
    u32 len = args.len < sizeof(raw)-1 ? args.len : (u32)(sizeof(raw)-1);
    memcpy(raw, args.data, len); raw[len] = '\0';

    char target[64];
    const char* msg = next_token(raw, target, sizeof(target));

    if (target[0] == '\0' || msg[0] == '\0') {
        *output = SEA_SLICE_LIT("Usage: swarm_relay <target_agent> <message>");
        return SEA_OK;
    }

    const char* coord = coordinator_name();
    const char* gw    = gateway_url();

    char msg_esc[4096], coord_esc[128];
    json_escape(msg,   msg_esc,   sizeof(msg_esc));
    json_escape(coord, coord_esc, sizeof(coord_esc));

    char body[4096 + 256];
    snprintf(body, sizeof(body),
        "{\"from_agent\":\"%s\",\"message\":\"%s\"}", coord_esc, msg_esc);

    char url[512];
    snprintf(url, sizeof(url), "%s/api/v1/agents/%s/relay", gw, target);

    SEA_LOG_INFO("SWARM", "relay from=%s to=%s", coord, target);

    char resp[SWARM_OUT_MAX];
    http_post(url, body, resp, sizeof(resp));

    char out[SWARM_OUT_MAX];
    int olen = snprintf(out, sizeof(out),
        "Relay to '%s':\nResponse: %s", target, resp);

    u8* dst = (u8*)sea_arena_push_bytes(arena, out, (u64)olen);
    if (dst) { output->data = dst; output->len = (u32)olen; }
    return SEA_OK;
}

/* ── swarm_workers ──────────────────────────────────────────── */

SeaError tool_swarm_workers(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    (void)args;

    const char* coord = coordinator_name();
    const char* gw    = gateway_url();

    char url[512];
    snprintf(url, sizeof(url), "%s/api/v1/agents/%s/workers", gw, coord);

    char resp[SWARM_OUT_MAX];
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "curl -s '%s' 2>&1", url);
    FILE* pipe = popen(cmd, "r");
    if (!pipe) {
        *output = SEA_SLICE_LIT("Error: could not contact gateway");
        return SEA_OK;
    }
    size_t total = 0;
    while (total < sizeof(resp)-1) {
        size_t n = fread(resp + total, 1, sizeof(resp)-1-total, pipe);
        if (n == 0) break;
        total += n;
    }
    resp[total] = '\0';
    pclose(pipe);

    char out[SWARM_OUT_MAX];
    int olen = snprintf(out, sizeof(out), "Workers for '%s':\n%s", coord, resp);
    u8* dst = (u8*)sea_arena_push_bytes(arena, out, (u64)olen);
    if (dst) { output->data = dst; output->len = (u32)olen; }
    return SEA_OK;
}
