/*
 * sea_a2a.c — Agent-to-Agent Communication Protocol
 *
 * HTTP JSON-RPC based delegation to remote agents.
 * All results Shield-validated before returning.
 */

#include "seaclaw/sea_a2a.h"
#include "seaclaw/sea_http.h"
#include "seaclaw/sea_json.h"
#include "seaclaw/sea_shield.h"
#include "seaclaw/sea_log.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/* ── Simple UUID-like ID generator ───────────────────────── */

static void gen_task_id(char* buf, size_t len) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    snprintf(buf, len, "sea-%lx-%lx",
             (unsigned long)ts.tv_sec, (unsigned long)ts.tv_nsec);
}

/* ── Build delegation JSON payload ───────────────────────── */

static const char* build_delegate_json(const SeaA2aRequest* req,
                                        SeaArena* arena) {
    /* Estimate size */
    u32 est = 256;
    if (req->task_desc) est += (u32)strlen(req->task_desc) * 2;
    if (req->context)   est += (u32)strlen(req->context) * 2;

    char* buf = (char*)sea_arena_alloc(arena, est, 1);
    if (!buf) return NULL;

    int pos = 0;
    pos += snprintf(buf + pos, est - (u32)pos,
        "{\"jsonrpc\":\"2.0\",\"method\":\"delegate\","
        "\"id\":\"%s\",\"params\":{\"task\":\"",
        req->task_id ? req->task_id : "unknown");

    /* JSON-escape the task description */
    if (req->task_desc) {
        for (const char* p = req->task_desc; *p && pos < (int)est - 10; p++) {
            if (*p == '"')       { buf[pos++] = '\\'; buf[pos++] = '"'; }
            else if (*p == '\\') { buf[pos++] = '\\'; buf[pos++] = '\\'; }
            else if (*p == '\n') { buf[pos++] = '\\'; buf[pos++] = 'n'; }
            else                 { buf[pos++] = *p; }
        }
    }

    pos += snprintf(buf + pos, est - (u32)pos, "\"");

    if (req->context) {
        pos += snprintf(buf + pos, est - (u32)pos, ",\"context\":\"");
        for (const char* p = req->context; *p && pos < (int)est - 10; p++) {
            if (*p == '"')       { buf[pos++] = '\\'; buf[pos++] = '"'; }
            else if (*p == '\\') { buf[pos++] = '\\'; buf[pos++] = '\\'; }
            else if (*p == '\n') { buf[pos++] = '\\'; buf[pos++] = 'n'; }
            else                 { buf[pos++] = *p; }
        }
        pos += snprintf(buf + pos, est - (u32)pos, "\"");
    }

    pos += snprintf(buf + pos, est - (u32)pos,
        ",\"timeout_ms\":%u}}",
        req->timeout_ms > 0 ? req->timeout_ms : 30000);

    buf[pos] = '\0';
    return buf;
}

/* ── Delegate a task to a remote agent ───────────────────── */

SeaA2aResult sea_a2a_delegate(const SeaA2aPeer* peer,
                               const SeaA2aRequest* req,
                               SeaArena* arena) {
    SeaA2aResult result = {
        .task_id = req->task_id,
        .success = false,
        .output = NULL,
        .latency_ms = 0,
        .agent_name = peer->name,
        .verified = false,
        .error = NULL,
    };

    if (!peer || !peer->endpoint || !req) {
        result.error = "Invalid peer or request";
        return result;
    }

    /* Generate task ID if not provided */
    char id_buf[64];
    if (!req->task_id) {
        gen_task_id(id_buf, sizeof(id_buf));
        result.task_id = (const char*)sea_arena_push_bytes(arena, id_buf,
                                                            (u64)strlen(id_buf));
    }

    SEA_LOG_INFO("A2A", "Delegating to %s: %s",
                 peer->name ? peer->name : peer->endpoint,
                 req->task_desc ? req->task_desc : "(no desc)");

    /* Build JSON payload */
    const char* json = build_delegate_json(req, arena);
    if (!json) {
        result.error = "Failed to build request JSON";
        return result;
    }

    /* Build auth header if peer has API key */
    const char* auth_hdr = NULL;
    if (peer->api_key) {
        u32 klen = (u32)strlen(peer->api_key);
        char* hdr = (char*)sea_arena_alloc(arena, 24 + klen + 1, 1);
        if (hdr) {
            snprintf(hdr, 24 + klen + 1, "Authorization: Bearer %s", peer->api_key);
            auth_hdr = hdr;
        }
    }

    /* Send HTTP POST */
    u64 t0 = (u64)time(NULL) * 1000; /* rough ms */
    SeaSlice body = { .data = (const u8*)json, .len = (u32)strlen(json) };
    SeaHttpResponse resp;
    SeaError err = sea_http_post_json_auth(peer->endpoint, body, auth_hdr,
                                            arena, &resp);

    u64 t1 = (u64)time(NULL) * 1000;
    result.latency_ms = (u32)(t1 - t0);

    if (err != SEA_OK) {
        result.error = "HTTP request to peer failed";
        SEA_LOG_ERROR("A2A", "Delegation failed: HTTP error");
        return result;
    }

    if (resp.status_code != 200) {
        char* msg = (char*)sea_arena_alloc(arena, 64, 1);
        if (msg) {
            snprintf(msg, 64, "Peer returned HTTP %d", resp.status_code);
            result.error = msg;
        }
        return result;
    }

    /* Parse JSON-RPC response */
    SeaJsonValue root;
    if (sea_json_parse(resp.body, arena, &root) != SEA_OK) {
        result.error = "Failed to parse peer response JSON";
        return result;
    }

    /* Extract result */
    const SeaJsonValue* res = sea_json_get(&root, "result");
    if (res) {
        SeaSlice output = sea_json_get_string(res, "output");
        if (output.len > 0) {
            char* out = (char*)sea_arena_alloc(arena, output.len + 1, 1);
            if (out) {
                memcpy(out, output.data, output.len);
                out[output.len] = '\0';
                result.output = out;
            }
        }

        /* Check success flag */
        const SeaJsonValue* ok = sea_json_get(res, "success");
        if (ok && ok->type == SEA_JSON_BOOL) {
            result.success = ok->boolean;
        } else {
            result.success = (result.output != NULL);
        }
    }

    /* Check for JSON-RPC error */
    const SeaJsonValue* err_obj = sea_json_get(&root, "error");
    if (err_obj) {
        SeaSlice err_msg = sea_json_get_string(err_obj, "message");
        if (err_msg.len > 0) {
            char* emsg = (char*)sea_arena_alloc(arena, err_msg.len + 1, 1);
            if (emsg) {
                memcpy(emsg, err_msg.data, err_msg.len);
                emsg[err_msg.len] = '\0';
                result.error = emsg;
                result.success = false;
            }
        }
    }

    /* Shield-verify the output */
    if (result.output) {
        SeaSlice out_slice = { .data = (const u8*)result.output,
                               .len = (u32)strlen(result.output) };
        if (sea_shield_detect_injection(out_slice)) {
            SEA_LOG_WARN("A2A", "Shield REJECTED output from %s (injection detected)",
                         peer->name ? peer->name : "unknown");
            result.verified = false;
            result.output = "[Shield rejected: injection detected in agent output]";
        } else {
            result.verified = true;
        }
    }

    SEA_LOG_INFO("A2A", "Delegation %s: %s (%ums, verified=%s)",
                 result.success ? "OK" : "FAILED",
                 result.task_id,
                 result.latency_ms,
                 result.verified ? "yes" : "no");

    return result;
}

/* ── Heartbeat ───────────────────────────────────────────── */

bool sea_a2a_heartbeat(const SeaA2aPeer* peer, SeaArena* arena) {
    if (!peer || !peer->endpoint) return false;

    /* Build heartbeat URL: endpoint + /heartbeat */
    u32 elen = (u32)strlen(peer->endpoint);
    char* url = (char*)sea_arena_alloc(arena, elen + 16, 1);
    if (!url) return false;
    snprintf(url, elen + 16, "%s/heartbeat", peer->endpoint);

    SeaHttpResponse resp;
    SeaError err = sea_http_get(url, arena, &resp);

    if (err != SEA_OK || resp.status_code != 200) {
        SEA_LOG_WARN("A2A", "Heartbeat failed for %s",
                     peer->name ? peer->name : peer->endpoint);
        return false;
    }

    SEA_LOG_DEBUG("A2A", "Heartbeat OK: %s",
                  peer->name ? peer->name : peer->endpoint);
    return true;
}

/* ── Discovery ───────────────────────────────────────────── */

i32 sea_a2a_discover(const char* discovery_url,
                     SeaA2aPeer* out, i32 max_peers,
                     SeaArena* arena) {
    if (!discovery_url || !out || max_peers <= 0) return 0;

    SeaHttpResponse resp;
    SeaError err = sea_http_get(discovery_url, arena, &resp);
    if (err != SEA_OK || resp.status_code != 200) return 0;

    SeaJsonValue root;
    if (sea_json_parse(resp.body, arena, &root) != SEA_OK) return 0;

    /* Expect: {"agents": [{"name":"...", "endpoint":"...", "api_key":"..."}]} */
    const SeaJsonValue* agents = sea_json_get(&root, "agents");
    if (!agents || agents->type != SEA_JSON_ARRAY) return 0;

    i32 count = 0;
    for (u32 i = 0; i < agents->array.count && count < max_peers; i++) {
        const SeaJsonValue* agent = &agents->array.items[i];
        SeaSlice name = sea_json_get_string(agent, "name");
        SeaSlice endpoint = sea_json_get_string(agent, "endpoint");

        if (endpoint.len == 0) continue;

        /* Copy strings into arena */
        char* n = (char*)sea_arena_alloc(arena, name.len + 1, 1);
        char* e = (char*)sea_arena_alloc(arena, endpoint.len + 1, 1);
        if (!n || !e) break;

        memcpy(n, name.data, name.len); n[name.len] = '\0';
        memcpy(e, endpoint.data, endpoint.len); e[endpoint.len] = '\0';

        out[count].name = n;
        out[count].endpoint = e;
        out[count].healthy = false;
        out[count].last_seen = 0;

        SeaSlice key = sea_json_get_string(agent, "api_key");
        if (key.len > 0) {
            char* k = (char*)sea_arena_alloc(arena, key.len + 1, 1);
            if (k) { memcpy(k, key.data, key.len); k[key.len] = '\0'; }
            out[count].api_key = k;
        } else {
            out[count].api_key = NULL;
        }

        count++;
    }

    SEA_LOG_INFO("A2A", "Discovered %d agents from %s", count, discovery_url);
    return count;
}
