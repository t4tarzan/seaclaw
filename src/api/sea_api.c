/*
 * sea_api.c — Lightweight HTTP API Server
 *
 * Minimal REST endpoint for external integrations.
 * Accepts POST /api/chat with JSON body {"message":"..."} and
 * returns {"response":"..."} after routing through the agent.
 *
 * Uses POSIX sockets + pthread. Runs in a background thread.
 * Port default: 8899 (configurable via SEA_API_PORT env var).
 */

#include "seaclaw/sea_api.h"
#include "seaclaw/sea_log.h"
#include "seaclaw/sea_agent.h"
#include "seaclaw/sea_shield.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

#define API_MAX_BODY    (64 * 1024)
#define API_MAX_HEADERS (4 * 1024)
#define API_BACKLOG     8
#define API_ARENA_SIZE  (512 * 1024)

/* ── Global State ──────────────────────────────────────────── */

static SeaApiConfig  s_api_cfg = {0};
static int           s_listen_fd = -1;
static pthread_t     s_api_tid;
static volatile bool s_api_running = false;

/* ── HTTP Helpers ──────────────────────────────────────────── */

static void send_http(int fd, int status, const char* status_text,
                      const char* body, int body_len) {
    char header[512];
    int hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
        "\r\n",
        status, status_text, body_len);

    ssize_t w = write(fd, header, (size_t)hlen);
    if (body_len > 0) w = write(fd, body, (size_t)body_len);
    (void)w;
}

static void send_json_error(int fd, int status, const char* msg) {
    char body[256];
    int blen = snprintf(body, sizeof(body), "{\"error\":\"%s\"}", msg);
    send_http(fd, status, status == 400 ? "Bad Request" : "Internal Server Error",
              body, blen);
}

/* ── Extract JSON string field (minimal, no full parser needed) ── */

static const char* extract_json_string(const char* json, const char* key,
                                        char* out, u32 out_size) {
    /* Try both "key":"val" and "key": "val" (with optional space) */
    char needle[64];
    snprintf(needle, sizeof(needle), "\"%s\":", key);
    const char* p = strstr(json, needle);
    if (!p) return NULL;
    p += strlen(needle);
    /* Skip whitespace between : and opening quote */
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '"') return NULL;
    p++; /* skip opening quote */

    u32 i = 0;
    while (*p && *p != '"' && i < out_size - 1) {
        if (*p == '\\' && *(p + 1)) {
            p++;
            if (*p == 'n') out[i++] = '\n';
            else if (*p == 't') out[i++] = '\t';
            else if (*p == '"') out[i++] = '"';
            else if (*p == '\\') out[i++] = '\\';
            else out[i++] = *p;
        } else {
            out[i++] = *p;
        }
        p++;
    }
    out[i] = '\0';
    return out;
}

/* ── Handle single connection ─────────────────────────────── */

static void handle_connection(int client_fd) {
    char buf[API_MAX_HEADERS + API_MAX_BODY];
    ssize_t total = 0;

    /* Read headers first */
    while (total < (ssize_t)sizeof(buf) - 1) {
        ssize_t n = read(client_fd, buf + total, (size_t)(sizeof(buf) - 1 - (size_t)total));
        if (n <= 0) break;
        total += n;
        buf[total] = '\0';
        if (strstr(buf, "\r\n\r\n")) break;
    }

    if (total <= 0) { close(client_fd); return; }

    /* If POST, read remaining body based on Content-Length */
    char* hdr_end = strstr(buf, "\r\n\r\n");
    if (hdr_end) {
        const char* cl_hdr = strcasestr(buf, "content-length:");
        if (cl_hdr) {
            int content_len = atoi(cl_hdr + 15);
            if (content_len > 0 && content_len < API_MAX_BODY) {
                ssize_t body_received = total - (ssize_t)(hdr_end + 4 - buf);
                while (body_received < content_len &&
                       total < (ssize_t)sizeof(buf) - 1) {
                    ssize_t n = read(client_fd, buf + total,
                                     (size_t)(sizeof(buf) - 1 - (size_t)total));
                    if (n <= 0) break;
                    total += n;
                    body_received += n;
                    buf[total] = '\0';
                }
            }
        }
    }

    /* Parse method + path */
    char method[8] = {0}, path[128] = {0};
    sscanf(buf, "%7s %127s", method, path);

    /* CORS preflight */
    if (strcmp(method, "OPTIONS") == 0) {
        send_http(client_fd, 204, "No Content", "", 0);
        close(client_fd);
        return;
    }

    /* GET /api/health */
    if (strcmp(method, "GET") == 0 && strcmp(path, "/api/health") == 0) {
        const char* body = "{\"status\":\"ok\",\"version\":\"" SEA_VERSION_STRING "\"}";
        send_http(client_fd, 200, "OK", body, (int)strlen(body));
        close(client_fd);
        return;
    }

    /* POST /api/chat */
    if (strcmp(method, "POST") == 0 && strcmp(path, "/api/chat") == 0) {
        /* Find body (after \r\n\r\n) */
        const char* body_start = strstr(buf, "\r\n\r\n");
        if (!body_start) {
            send_json_error(client_fd, 400, "No body");
            close(client_fd);
            return;
        }
        body_start += 4;

        /* Extract message */
        char message[4096];
        if (!extract_json_string(body_start, "message", message, sizeof(message))) {
            send_json_error(client_fd, 400, "Missing 'message' field");
            close(client_fd);
            return;
        }

        /* Shield check */
        SeaSlice msg_slice = { .data = (const u8*)message, .len = (u32)strlen(message) };
        if (sea_shield_detect_injection(msg_slice)) {
            send_json_error(client_fd, 400, "Injection detected");
            close(client_fd);
            return;
        }

        SEA_LOG_INFO("API", "Chat request: %.60s%s", message,
                     strlen(message) > 60 ? "..." : "");

        /* Route through agent */
        SeaArena req_arena;
        if (sea_arena_create(&req_arena, API_ARENA_SIZE) != SEA_OK) {
            send_json_error(client_fd, 500, "Arena allocation failed");
            close(client_fd);
            return;
        }

        SeaAgentResult ar = sea_agent_chat(s_api_cfg.agent_cfg,
                                            NULL, 0, message, &req_arena);

        if (ar.error == SEA_OK && ar.text) {
            /* Build JSON response with escaped text */
            u32 tlen = (u32)strlen(ar.text);
            char* resp = malloc(tlen * 2 + 128);
            if (resp) {
                u32 ri = (u32)snprintf(resp, 64, "{\"response\":\"");
                for (u32 i = 0; i < tlen && ri < tlen * 2 + 100; i++) {
                    char c = ar.text[i];
                    if (c == '"')       { resp[ri++] = '\\'; resp[ri++] = '"'; }
                    else if (c == '\\') { resp[ri++] = '\\'; resp[ri++] = '\\'; }
                    else if (c == '\n') { resp[ri++] = '\\'; resp[ri++] = 'n'; }
                    else if (c == '\r') { /* skip */ }
                    else if (c == '\t') { resp[ri++] = '\\'; resp[ri++] = 't'; }
                    else                { resp[ri++] = c; }
                }
                ri += (u32)snprintf(resp + ri, 32, "\",\"tool_calls\":%u}", ar.tool_calls);
                send_http(client_fd, 200, "OK", resp, (int)ri);
                free(resp);
            } else {
                send_json_error(client_fd, 500, "OOM");
            }
        } else {
            send_json_error(client_fd, 500, ar.text ? ar.text : "Agent error");
        }

        sea_arena_destroy(&req_arena);
        close(client_fd);
        return;
    }

    /* 404 for anything else */
    const char* not_found = "{\"error\":\"Not found. Use POST /api/chat or GET /api/health\"}";
    send_http(client_fd, 404, "Not Found", not_found, (int)strlen(not_found));
    close(client_fd);
}

/* ── Server Thread ────────────────────────────────────────── */

static void* api_thread(void* arg) {
    (void)arg;
    SEA_LOG_INFO("API", "Server listening on port %u", s_api_cfg.port);

    while (s_api_running) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        int client_fd = accept(s_listen_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (client_fd < 0) {
            if (s_api_running && errno != EINTR) {
                SEA_LOG_WARN("API", "accept() failed: %s", strerror(errno));
            }
            continue;
        }

        /* Set read timeout */
        struct timeval tv = { .tv_sec = 30, .tv_usec = 0 };
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        handle_connection(client_fd);
    }

    return NULL;
}

/* ── Public API ───────────────────────────────────────────── */

int sea_api_start(const SeaApiConfig* cfg) {
    if (!cfg || !cfg->agent_cfg) return -1;

    s_api_cfg = *cfg;
    if (s_api_cfg.port == 0) s_api_cfg.port = 8899;

    /* Create listening socket */
    s_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (s_listen_fd < 0) {
        SEA_LOG_ERROR("API", "socket() failed: %s", strerror(errno));
        return -1;
    }

    int opt = 1;
    setsockopt(s_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* Bind to 0.0.0.0 if SEA_API_BIND_ALL is set (for containers),
     * otherwise loopback only for security */
    const char* bind_all = getenv("SEA_API_BIND_ALL");
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(s_api_cfg.port),
        .sin_addr.s_addr = (bind_all && *bind_all) ? htonl(INADDR_ANY)
                                                     : htonl(INADDR_LOOPBACK),
    };

    if (bind(s_listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        SEA_LOG_ERROR("API", "bind() port %u failed: %s", s_api_cfg.port, strerror(errno));
        close(s_listen_fd);
        s_listen_fd = -1;
        return -1;
    }

    if (listen(s_listen_fd, API_BACKLOG) < 0) {
        SEA_LOG_ERROR("API", "listen() failed: %s", strerror(errno));
        close(s_listen_fd);
        s_listen_fd = -1;
        return -1;
    }

    s_api_running = true;
    if (pthread_create(&s_api_tid, NULL, api_thread, NULL) != 0) {
        SEA_LOG_ERROR("API", "pthread_create failed");
        s_api_running = false;
        close(s_listen_fd);
        s_listen_fd = -1;
        return -1;
    }

    return 0;
}

void sea_api_stop(void) {
    if (!s_api_running) return;
    s_api_running = false;
    if (s_listen_fd >= 0) {
        shutdown(s_listen_fd, SHUT_RDWR);
        close(s_listen_fd);
        s_listen_fd = -1;
    }
    pthread_join(s_api_tid, NULL);
    SEA_LOG_INFO("API", "Server stopped");
}

bool sea_api_running(void) {
    return s_api_running;
}
