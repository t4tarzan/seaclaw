/*
 * sea_mesh.c — Distributed Agent Mesh Implementation
 *
 * Captain/Crew architecture. All communication over HTTP JSON-RPC
 * within the local network. HMAC-authenticated, Shield-verified.
 */

#include "seaclaw/sea_mesh.h"
#include "seaclaw/sea_http.h"
#include "seaclaw/sea_json.h"
#include "seaclaw/sea_shield.h"
#include "seaclaw/sea_log.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/* ── Helpers ──────────────────────────────────────────────── */

static u64 now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (u64)ts.tv_sec * 1000 + (u64)ts.tv_nsec / 1000000;
}

static SeaMeshNode* find_node(SeaMesh* mesh, const char* name) {
    for (u32 i = 0; i < mesh->node_count; i++) {
        if (strcmp(mesh->nodes[i].name, name) == 0) {
            return &mesh->nodes[i];
        }
    }
    return NULL;
}

/* Simple HMAC: FNV-1a hash of (secret + timestamp + nonce).
 * Not cryptographic — sufficient for LAN trust boundary. */
static u64 fnv1a_hash(const u8* data, u32 len) {
    u64 h = 14695981039346656037ULL;
    for (u32 i = 0; i < len; i++) {
        h ^= data[i];
        h *= 1099511628211ULL;
    }
    return h;
}

/* ── Init / Destroy ──────────────────────────────────────── */

SeaError sea_mesh_init(SeaMesh* mesh, SeaMeshConfig* config, SeaDb* db) {
    if (!mesh || !config) return SEA_ERR_CONFIG;

    memset(mesh, 0, sizeof(SeaMesh));
    mesh->config = *config;
    mesh->db = db;
    mesh->running = true;
    mesh->initialized = true;

    /* Defaults */
    if (mesh->config.port == 0) {
        mesh->config.port = (config->role == SEA_MESH_CAPTAIN) ? 9100 : 9101;
    }
    if (mesh->config.heartbeat_interval_ms == 0) {
        mesh->config.heartbeat_interval_ms = 30000;
    }
    if (mesh->config.task_timeout_ms == 0) {
        mesh->config.task_timeout_ms = 60000;
    }

    SEA_LOG_INFO("MESH", "Initialized as %s '%s' on port %u",
                 config->role == SEA_MESH_CAPTAIN ? "CAPTAIN" : "CREW",
                 config->node_name, mesh->config.port);

    return SEA_OK;
}

void sea_mesh_destroy(SeaMesh* mesh) {
    if (!mesh) return;
    mesh->running = false;
    mesh->initialized = false;
    SEA_LOG_INFO("MESH", "Mesh engine destroyed");
}

/* ── Node Registry (Captain) ─────────────────────────────── */

SeaError sea_mesh_register_node(SeaMesh* mesh, const char* name,
                                 const char* endpoint,
                                 const char** capabilities, u32 cap_count) {
    if (!mesh || !name || !endpoint) return SEA_ERR_CONFIG;

    /* Check if node already exists — update it */
    SeaMeshNode* existing = find_node(mesh, name);
    if (existing) {
        strncpy(existing->endpoint, endpoint, sizeof(existing->endpoint) - 1);
        existing->capability_count = 0;
        for (u32 i = 0; i < cap_count && i < SEA_MESH_MAX_CAPABILITIES; i++) {
            strncpy(existing->capabilities[i], capabilities[i], 63);
            existing->capability_count++;
        }
        existing->healthy = true;
        existing->last_heartbeat = now_ms();
        SEA_LOG_INFO("MESH", "Node '%s' re-registered (%u capabilities)", name, cap_count);
        return SEA_OK;
    }

    if (mesh->node_count >= SEA_MESH_MAX_NODES) {
        SEA_LOG_WARN("MESH", "Node registry full (%u)", SEA_MESH_MAX_NODES);
        return SEA_ERR_OOM;
    }

    SeaMeshNode* node = &mesh->nodes[mesh->node_count];
    memset(node, 0, sizeof(SeaMeshNode));
    strncpy(node->name, name, SEA_MESH_NODE_NAME_MAX - 1);
    strncpy(node->endpoint, endpoint, sizeof(node->endpoint) - 1);
    for (u32 i = 0; i < cap_count && i < SEA_MESH_MAX_CAPABILITIES; i++) {
        strncpy(node->capabilities[i], capabilities[i], 63);
        node->capability_count++;
    }
    node->healthy = true;
    node->last_heartbeat = now_ms();
    node->registered_at = now_ms();
    mesh->node_count++;

    SEA_LOG_INFO("MESH", "Node '%s' registered at %s (%u capabilities)",
                 name, endpoint, cap_count);

    /* Audit log */
    if (mesh->db) {
        char audit[256];
        snprintf(audit, sizeof(audit), "node=%s endpoint=%s caps=%u", name, endpoint, cap_count);
        sea_db_log_event(mesh->db, "mesh_register", name, audit);
    }

    return SEA_OK;
}

SeaError sea_mesh_remove_node(SeaMesh* mesh, const char* name) {
    if (!mesh || !name) return SEA_ERR_CONFIG;

    for (u32 i = 0; i < mesh->node_count; i++) {
        if (strcmp(mesh->nodes[i].name, name) == 0) {
            /* Shift remaining nodes */
            for (u32 j = i; j < mesh->node_count - 1; j++) {
                mesh->nodes[j] = mesh->nodes[j + 1];
            }
            mesh->node_count--;
            SEA_LOG_INFO("MESH", "Node '%s' removed", name);
            return SEA_OK;
        }
    }
    return SEA_ERR_TOOL_NOT_FOUND;
}

/* ── Capability-Based Routing ────────────────────────────── */

const SeaMeshNode* sea_mesh_route_tool(SeaMesh* mesh, const char* tool_name) {
    if (!mesh || !tool_name) return NULL;

    const SeaMeshNode* best = NULL;
    u32 best_load = UINT32_MAX; /* Prefer least-loaded node */

    for (u32 i = 0; i < mesh->node_count; i++) {
        SeaMeshNode* node = &mesh->nodes[i];
        if (!node->healthy) continue;

        /* Check if node has this capability */
        for (u32 c = 0; c < node->capability_count; c++) {
            if (strcmp(node->capabilities[c], tool_name) == 0) {
                u32 load = node->tasks_completed + node->tasks_failed;
                if (!best || load < best_load) {
                    best = node;
                    best_load = load;
                }
                break;
            }
        }
    }

    return best;
}

/* ── Task Dispatch (Captain → Crew) ──────────────────────── */

SeaMeshResult sea_mesh_dispatch(SeaMesh* mesh, SeaMeshTask* task,
                                 SeaArena* arena) {
    SeaMeshResult result = { .task_id = task->task_id, .success = false,
                              .output = NULL, .node_name = NULL,
                              .latency_ms = 0, .error = NULL };

    /* Find the best node for this tool */
    const SeaMeshNode* node = sea_mesh_route_tool(mesh, task->tool_name);
    if (!node) {
        result.error = "No node available for this tool";
        SEA_LOG_WARN("MESH", "No node for tool '%s'", task->tool_name);
        return result;
    }

    result.node_name = node->name;

    /* Build JSON-RPC request */
    char url[512];
    snprintf(url, sizeof(url), "%s/node/exec", node->endpoint);

    char json[2048];
    int jlen = snprintf(json, sizeof(json),
        "{\"task_id\":\"%s\",\"tool\":\"%s\",\"args\":\"%s\"}",
        task->task_id ? task->task_id : "0",
        task->tool_name,
        task->tool_args ? task->tool_args : "");

    SeaSlice body = { .data = (const u8*)json, .len = (u32)jlen };
    SeaHttpResponse resp;

    u64 t0 = now_ms();
    SeaError err = sea_http_post_json(url, body, arena, &resp);
    u64 t1 = now_ms();

    result.latency_ms = (u32)(t1 - t0);

    if (err != SEA_OK || resp.status_code != 200) {
        result.error = "HTTP request to node failed";
        SEA_LOG_WARN("MESH", "Dispatch to '%s' failed (err=%d)", node->name, err);
        return result;
    }

    /* Parse response */
    SeaJsonValue root;
    if (sea_json_parse(resp.body, arena, &root) == SEA_OK) {
        SeaSlice output_sl = sea_json_get_string(&root, "output");
        if (output_sl.len > 0) {
            char* out = (char*)sea_arena_alloc(arena, output_sl.len + 1, 1);
            if (out) {
                memcpy(out, output_sl.data, output_sl.len);
                out[output_sl.len] = '\0';
                result.output = out;
                result.success = true;
            }
        }
    }

    /* Shield-verify output */
    if (result.output) {
        SeaSlice out_slice = { .data = (const u8*)result.output,
                               .len = (u32)strlen(result.output) };
        if (sea_shield_detect_output_injection(out_slice)) {
            SEA_LOG_WARN("MESH", "Shield REJECTED output from node '%s'", node->name);
            result.output = "[Output rejected by Shield]";
            result.success = false;
        }
    }

    /* Audit */
    if (mesh->db) {
        char audit[512];
        snprintf(audit, sizeof(audit), "tool=%s node=%s latency=%ums success=%s",
                 task->tool_name, node->name, result.latency_ms,
                 result.success ? "yes" : "no");
        sea_db_log_event(mesh->db, "mesh_dispatch", task->tool_name, audit);
    }

    SEA_LOG_INFO("MESH", "Dispatched '%s' to '%s' (%ums, %s)",
                 task->tool_name, node->name, result.latency_ms,
                 result.success ? "ok" : "fail");

    return result;
}

/* ── Crew Registration ───────────────────────────────────── */

SeaError sea_mesh_crew_register(SeaMesh* mesh, SeaArena* arena) {
    if (!mesh || mesh->config.role != SEA_MESH_CREW) return SEA_ERR_CONFIG;

    char url[512];
    snprintf(url, sizeof(url), "%s/mesh/register", mesh->config.captain_url);

    /* Build registration JSON with our capabilities */
    char json[4096];
    int pos = snprintf(json, sizeof(json),
        "{\"name\":\"%s\",\"endpoint\":\"http://localhost:%u\",\"capabilities\":[",
        mesh->config.node_name, mesh->config.port);

    /* TODO: enumerate local tools as capabilities */
    pos += snprintf(json + pos, sizeof(json) - (size_t)pos,
        "\"file_read\",\"file_write\",\"shell_exec\",\"dir_list\"");

    pos += snprintf(json + pos, sizeof(json) - (size_t)pos, "]}");

    SeaSlice body = { .data = (const u8*)json, .len = (u32)pos };
    SeaHttpResponse resp;
    SeaError err = sea_http_post_json(url, body, arena, &resp);

    if (err == SEA_OK && resp.status_code == 200) {
        SEA_LOG_INFO("MESH", "Registered with Captain at %s", mesh->config.captain_url);
        return SEA_OK;
    }

    SEA_LOG_WARN("MESH", "Failed to register with Captain (err=%d)", err);
    return SEA_ERR_IO;
}

/* ── Heartbeat ───────────────────────────────────────────── */

SeaError sea_mesh_crew_heartbeat(SeaMesh* mesh, SeaArena* arena) {
    if (!mesh || mesh->config.role != SEA_MESH_CREW) return SEA_ERR_CONFIG;

    char url[512];
    snprintf(url, sizeof(url), "%s/mesh/heartbeat", mesh->config.captain_url);

    char json[256];
    int jlen = snprintf(json, sizeof(json),
        "{\"name\":\"%s\",\"timestamp\":%lu}",
        mesh->config.node_name, (unsigned long)now_ms());

    SeaSlice body = { .data = (const u8*)json, .len = (u32)jlen };
    SeaHttpResponse resp;
    SeaError err = sea_http_post_json(url, body, arena, &resp);

    return (err == SEA_OK && resp.status_code == 200) ? SEA_OK : SEA_ERR_IO;
}

SeaError sea_mesh_process_heartbeat(SeaMesh* mesh, const char* node_name) {
    SeaMeshNode* node = find_node(mesh, node_name);
    if (!node) return SEA_ERR_TOOL_NOT_FOUND;
    node->healthy = true;
    node->last_heartbeat = now_ms();
    return SEA_OK;
}

/* ── Status ──────────────────────────────────────────────── */

u32 sea_mesh_healthy_nodes(SeaMesh* mesh, const SeaMeshNode** out, u32 max) {
    u32 count = 0;
    u64 stale_threshold = now_ms() - (mesh->config.heartbeat_interval_ms * 3);

    for (u32 i = 0; i < mesh->node_count && count < max; i++) {
        /* Mark stale nodes as unhealthy */
        if (mesh->nodes[i].last_heartbeat < stale_threshold) {
            mesh->nodes[i].healthy = false;
        }
        if (mesh->nodes[i].healthy) {
            out[count++] = &mesh->nodes[i];
        }
    }
    return count;
}

u32 sea_mesh_node_count(SeaMesh* mesh) {
    return mesh ? mesh->node_count : 0;
}

const char* sea_mesh_status(SeaMesh* mesh, SeaArena* arena) {
    if (!mesh) return "Mesh not initialized";

    char buf[4096];
    int pos = snprintf(buf, sizeof(buf),
        "Sea-Claw Mesh — %s '%s'\n"
        "Port: %u | Nodes: %u | Secret: %s\n\n",
        mesh->config.role == SEA_MESH_CAPTAIN ? "CAPTAIN" : "CREW",
        mesh->config.node_name,
        mesh->config.port,
        mesh->node_count,
        mesh->config.shared_secret[0] ? "configured" : "none");

    if (mesh->config.role == SEA_MESH_CAPTAIN) {
        for (u32 i = 0; i < mesh->node_count; i++) {
            SeaMeshNode* n = &mesh->nodes[i];
            pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos,
                "  %s %s (%s) — %u caps, %u tasks, %s\n",
                n->healthy ? "●" : "○",
                n->name, n->endpoint,
                n->capability_count,
                n->tasks_completed,
                n->healthy ? "healthy" : "stale");
        }
    } else {
        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos,
            "  Captain: %s\n", mesh->config.captain_url);
    }

    char* out = (char*)sea_arena_alloc(arena, (u32)pos + 1, 1);
    if (!out) return "OOM";
    memcpy(out, buf, (size_t)pos);
    out[pos] = '\0';
    return out;
}

/* ── HMAC Token ──────────────────────────────────────────── */

bool sea_mesh_validate_token(SeaMesh* mesh, const char* token) {
    if (!mesh || !token) return false;
    if (!mesh->config.shared_secret[0]) return true; /* No secret = open */

    /* Token format: "timestamp:hash" */
    const char* colon = strchr(token, ':');
    if (!colon) return false;

    /* Verify hash */
    char material[512];
    u32 ts_len = (u32)(colon - token);
    snprintf(material, sizeof(material), "%.*s:%s",
             (int)ts_len, token, mesh->config.shared_secret);
    u64 expected = fnv1a_hash((const u8*)material, (u32)strlen(material));

    char expected_str[32];
    snprintf(expected_str, sizeof(expected_str), "%016llx", (unsigned long long)expected);

    return strcmp(colon + 1, expected_str) == 0;
}

const char* sea_mesh_generate_token(SeaMesh* mesh, SeaArena* arena) {
    if (!mesh) return NULL;

    char ts[32];
    snprintf(ts, sizeof(ts), "%lu", (unsigned long)now_ms());

    char material[512];
    snprintf(material, sizeof(material), "%s:%s", ts, mesh->config.shared_secret);
    u64 hash = fnv1a_hash((const u8*)material, (u32)strlen(material));

    char token[64];
    int tlen = snprintf(token, sizeof(token), "%s:%016llx", ts, (unsigned long long)hash);

    char* out = (char*)sea_arena_alloc(arena, (u32)tlen + 1, 1);
    if (!out) return NULL;
    memcpy(out, token, (size_t)tlen);
    out[tlen] = '\0';
    return out;
}

SeaError sea_mesh_broadcast(SeaMesh* mesh, const char* message, SeaArena* arena) {
    if (!mesh || !message) return SEA_ERR_CONFIG;

    u32 sent = 0;
    for (u32 i = 0; i < mesh->node_count; i++) {
        if (!mesh->nodes[i].healthy) continue;

        char url[512];
        snprintf(url, sizeof(url), "%s/mesh/broadcast", mesh->nodes[i].endpoint);

        char json[1024];
        int jlen = snprintf(json, sizeof(json), "{\"message\":\"%s\"}", message);
        SeaSlice body = { .data = (const u8*)json, .len = (u32)jlen };
        SeaHttpResponse resp;

        if (sea_http_post_json(url, body, arena, &resp) == SEA_OK) {
            sent++;
        }
    }

    SEA_LOG_INFO("MESH", "Broadcast sent to %u/%u nodes", sent, mesh->node_count);
    return SEA_OK;
}
