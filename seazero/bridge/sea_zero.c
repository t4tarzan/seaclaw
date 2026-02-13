/*
 * sea_zero.c — SeaZero Bridge: SeaClaw ↔ Agent Zero IPC
 *
 * Delegates tasks to Agent Zero containers via HTTP JSON-RPC.
 * Uses SeaClaw's existing sea_http client — no new dependencies.
 *
 * Agent Zero exposes:
 *   POST /api/v1/task    — execute a task
 *   GET  /health         — health check
 *
 * All responses are arena-allocated and Shield-validated before
 * reaching the user.
 */

#include "sea_zero.h"
#include "seaclaw/sea_http.h"
#include "seaclaw/sea_json.h"
#include "seaclaw/sea_log.h"
#include "seaclaw/sea_shield.h"
#include "seaclaw/sea_tools.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define SEAZERO_DEFAULT_URL     "http://localhost:8080"
#define SEAZERO_DEFAULT_TIMEOUT 120
#define SEAZERO_MAX_TASK_LEN    8192
#define SEAZERO_MAX_CONTEXT_LEN 16384

/* ── Config ────────────────────────────────────────────────── */

bool sea_zero_init(SeaZeroConfig* cfg, const char* agent_url) {
    if (!cfg) return false;

    cfg->agent_url   = agent_url ? agent_url : SEAZERO_DEFAULT_URL;
    cfg->agent_id    = "agent-0";
    cfg->timeout_sec = SEAZERO_DEFAULT_TIMEOUT;
    cfg->enabled     = true;

    SEA_LOG_INFO("SEAZERO", "Bridge initialized: %s (timeout=%us)",
                 cfg->agent_url, cfg->timeout_sec);
    return true;
}

/* ── JSON Escaping Helper ──────────────────────────────────── */

static int json_escape(char* dst, int dst_cap, const char* src, int src_len) {
    int w = 0;
    for (int i = 0; i < src_len && w < dst_cap - 2; i++) {
        char c = src[i];
        switch (c) {
            case '"':  if (w + 2 < dst_cap) { dst[w++] = '\\'; dst[w++] = '"'; }  break;
            case '\\': if (w + 2 < dst_cap) { dst[w++] = '\\'; dst[w++] = '\\'; } break;
            case '\n': if (w + 2 < dst_cap) { dst[w++] = '\\'; dst[w++] = 'n'; }  break;
            case '\r': if (w + 2 < dst_cap) { dst[w++] = '\\'; dst[w++] = 'r'; }  break;
            case '\t': if (w + 2 < dst_cap) { dst[w++] = '\\'; dst[w++] = 't'; }  break;
            default:   dst[w++] = c; break;
        }
    }
    dst[w] = '\0';
    return w;
}

/* ── Task Delegation ───────────────────────────────────────── */

SeaZeroResult sea_zero_delegate(
    const SeaZeroConfig* cfg,
    const SeaZeroTask*   task,
    SeaArena*            arena)
{
    SeaZeroResult result = {
        .success     = false,
        .result      = NULL,
        .error       = NULL,
        .steps_taken = 0,
        .elapsed_sec = 0.0
    };

    if (!cfg || !cfg->enabled) {
        result.error = "SeaZero is disabled";
        return result;
    }

    if (!task || !task->task || !task->task[0]) {
        result.error = "Empty task";
        return result;
    }

    /* Build the URL */
    char url[512];
    snprintf(url, sizeof(url), "%s/api/v1/task", cfg->agent_url);

    /* Escape the task string for JSON */
    int task_len = (int)strlen(task->task);
    if (task_len > SEAZERO_MAX_TASK_LEN) {
        result.error = "Task too long (max 8192 chars)";
        return result;
    }

    char escaped_task[SEAZERO_MAX_TASK_LEN * 2];
    json_escape(escaped_task, sizeof(escaped_task), task->task, task_len);

    /* Build JSON request body */
    char req_json[SEAZERO_MAX_TASK_LEN * 2 + SEAZERO_MAX_CONTEXT_LEN + 256];
    int req_len;

    if (task->context && task->context[0]) {
        char escaped_ctx[SEAZERO_MAX_CONTEXT_LEN * 2];
        int ctx_len = (int)strlen(task->context);
        if (ctx_len > SEAZERO_MAX_CONTEXT_LEN) ctx_len = SEAZERO_MAX_CONTEXT_LEN;
        json_escape(escaped_ctx, sizeof(escaped_ctx), task->context, ctx_len);

        req_len = snprintf(req_json, sizeof(req_json),
            "{\"task\":\"%s\",\"context\":\"%s\",\"max_steps\":%u,\"timeout\":%u}",
            escaped_task, escaped_ctx,
            task->max_steps > 0 ? task->max_steps : 10,
            task->timeout_sec > 0 ? task->timeout_sec : cfg->timeout_sec);
    } else {
        req_len = snprintf(req_json, sizeof(req_json),
            "{\"task\":\"%s\",\"max_steps\":%u,\"timeout\":%u}",
            escaped_task,
            task->max_steps > 0 ? task->max_steps : 10,
            task->timeout_sec > 0 ? task->timeout_sec : cfg->timeout_sec);
    }

    SEA_LOG_INFO("SEAZERO", "Delegating task to %s: %.80s%s",
                 cfg->agent_id, task->task,
                 task_len > 80 ? "..." : "");

    /* Send HTTP POST */
    time_t t0 = time(NULL);
    SeaSlice body = { .data = (const u8*)req_json, .len = (u32)req_len };
    SeaHttpResponse resp;
    SeaError err = sea_http_post_json(url, body, arena, &resp);
    time_t t1 = time(NULL);

    result.elapsed_sec = difftime(t1, t0);

    if (err != SEA_OK) {
        result.error = (err == SEA_ERR_TIMEOUT)
            ? "Agent Zero timed out"
            : "Agent Zero unreachable";
        SEA_LOG_ERROR("SEAZERO", "HTTP request failed: %s", sea_error_str(err));
        return result;
    }

    if (resp.status_code != 200) {
        SEA_LOG_ERROR("SEAZERO", "Agent Zero returned HTTP %d", resp.status_code);
        result.error = "Agent Zero returned an error";
        return result;
    }

    /* Parse JSON response */
    SeaJsonValue root;
    if (sea_json_parse(resp.body, arena, &root) != SEA_OK) {
        result.error = "Invalid JSON from Agent Zero";
        return result;
    }

    /* Extract result */
    SeaSlice result_slice = sea_json_get_string(&root, "result");
    SeaSlice error_slice  = sea_json_get_string(&root, "error");

    if (error_slice.len > 0) {
        /* Agent Zero reported an error */
        char* err_str = (char*)sea_arena_push(arena, error_slice.len + 1);
        if (err_str) {
            memcpy(err_str, error_slice.data, error_slice.len);
            err_str[error_slice.len] = '\0';
            result.error = err_str;
        } else {
            result.error = "Agent Zero error (arena full)";
        }
        return result;
    }

    if (result_slice.len == 0) {
        result.error = "Empty response from Agent Zero";
        return result;
    }

    /* Validate output through Grammar Shield (output injection detection) */
    if (sea_shield_detect_output_injection(result_slice)) {
        SEA_LOG_WARN("SEAZERO", "Grammar Shield blocked Agent Zero output (injection detected)");
        result.error = "Agent Zero output blocked by Grammar Shield";
        return result;
    }

    /* Copy result to arena */
    char* res_str = (char*)sea_arena_push(arena, result_slice.len + 1);
    if (!res_str) {
        result.error = "Arena full";
        return result;
    }
    memcpy(res_str, result_slice.data, result_slice.len);
    res_str[result_slice.len] = '\0';

    result.success = true;
    result.result  = res_str;

    SEA_LOG_INFO("SEAZERO", "Task completed in %.0fs (%u bytes)",
                 result.elapsed_sec, result_slice.len);

    return result;
}

/* ── Health Check ──────────────────────────────────────────── */

SeaZeroHealth sea_zero_health(const SeaZeroConfig* cfg, SeaArena* arena) {
    SeaZeroHealth health = {
        .reachable    = false,
        .agent_id     = NULL,
        .status       = "unreachable",
        .active_tasks = 0
    };

    if (!cfg || !cfg->enabled) {
        health.status = "disabled";
        return health;
    }

    char url[512];
    snprintf(url, sizeof(url), "%s/health", cfg->agent_url);

    SeaHttpResponse resp;
    SeaError err = sea_http_get(url, arena, &resp);

    if (err != SEA_OK || resp.status_code != 200) {
        return health;
    }

    health.reachable = true;

    /* Parse health response */
    SeaJsonValue root;
    if (sea_json_parse(resp.body, arena, &root) == SEA_OK) {
        SeaSlice status_slice = sea_json_get_string(&root, "status");
        if (status_slice.len > 0) {
            char* s = (char*)sea_arena_push(arena, status_slice.len + 1);
            if (s) {
                memcpy(s, status_slice.data, status_slice.len);
                s[status_slice.len] = '\0';
                health.status = s;
            }
        }

        SeaSlice id_slice = sea_json_get_string(&root, "agent_id");
        if (id_slice.len > 0) {
            char* id = (char*)sea_arena_push(arena, id_slice.len + 1);
            if (id) {
                memcpy(id, id_slice.data, id_slice.len);
                id[id_slice.len] = '\0';
                health.agent_id = id;
            }
        }
    }

    return health;
}

/* ── Tool Integration ──────────────────────────────────────── */

/* Global config — set once at startup */
static SeaZeroConfig s_zero_cfg = { 0 };

/*
 * tool_agent_zero — SeaClaw tool #58
 *
 * Called by the LLM when it decides a task needs Agent Zero.
 * Args: natural language task description
 *
 * Example:
 *   {"tool": "agent_zero", "params": {"task": "scan network for open ports"}}
 */
SeaError tool_agent_zero(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (!s_zero_cfg.enabled) {
        *output = SEA_SLICE_LIT("SeaZero is not enabled. Start Agent Zero with: cd seazero && docker compose up -d");
        return SEA_OK;
    }

    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Usage: agent_zero <task description>");
        return SEA_OK;
    }

    /* Build task from args */
    char task_str[SEAZERO_MAX_TASK_LEN + 1];
    u32 copy_len = args.len < SEAZERO_MAX_TASK_LEN ? args.len : SEAZERO_MAX_TASK_LEN;
    memcpy(task_str, args.data, copy_len);
    task_str[copy_len] = '\0';

    SeaZeroTask task = {
        .task        = task_str,
        .context     = NULL,
        .max_steps   = 10,
        .timeout_sec = s_zero_cfg.timeout_sec
    };

    SeaZeroResult res = sea_zero_delegate(&s_zero_cfg, &task, arena);

    if (res.success && res.result) {
        /* Copy result to output slice */
        u32 len = (u32)strlen(res.result);
        u8* buf = (u8*)sea_arena_push_bytes(arena, (const u8*)res.result, len);
        if (!buf) {
            *output = SEA_SLICE_LIT("Error: arena full");
            return SEA_OK;
        }
        output->data = buf;
        output->len  = len;
    } else {
        /* Format error message */
        const char* err_msg = res.error ? res.error : "Unknown error";
        char msg[512];
        int mlen = snprintf(msg, sizeof(msg),
            "Agent Zero failed: %s (%.0fs elapsed)", err_msg, res.elapsed_sec);
        u8* buf = (u8*)sea_arena_push_bytes(arena, (const u8*)msg, (u32)mlen);
        if (!buf) {
            *output = SEA_SLICE_LIT("Agent Zero failed (arena full)");
            return SEA_OK;
        }
        output->data = buf;
        output->len  = (u32)mlen;
    }

    return SEA_OK;
}

/* Register the agent_zero tool and initialize the bridge config */
int sea_zero_register_tool(const SeaZeroConfig* cfg) {
    if (cfg) {
        s_zero_cfg = *cfg;
    }
    /* Tool registration happens in sea_tools.c static registry.
     * This function just stores the config for the tool to use.
     * The tool itself (tool_agent_zero) is added to s_registry[]
     * in sea_tools.c as tool #58. */
    SEA_LOG_INFO("SEAZERO", "Tool 'agent_zero' registered (agent=%s, url=%s)",
                 s_zero_cfg.agent_id, s_zero_cfg.agent_url);
    return 0;
}
