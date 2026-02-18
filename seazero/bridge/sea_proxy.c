/*
 * sea_proxy.c — SeaZero LLM Proxy Server
 *
 * Lightweight HTTP server that proxies Agent Zero's LLM requests
 * through SeaClaw. Validates internal token, checks budget, forwards
 * to real LLM, logs usage. Pure POSIX sockets + pthread.
 *
 * Endpoint:
 *   POST /v1/chat/completions  — OpenAI-compatible proxy
 *   GET  /health               — Proxy health check
 *
 * Agent Zero config:
 *   OPENAI_API_BASE=http://host.docker.internal:7432
 *   OPENAI_API_KEY=<internal_token>
 */

#include "sea_proxy.h"
#include "seaclaw/sea_http.h"
#include "seaclaw/sea_json.h"
#include "seaclaw/sea_log.h"
#include "seaclaw/sea_shield.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

#define PROXY_MAX_BODY      (256 * 1024)  /* 256KB max request body */
#define PROXY_MAX_HEADERS   (8 * 1024)    /* 8KB max headers        */
#define PROXY_BACKLOG       8
#define PROXY_ARENA_SIZE    (512 * 1024)  /* 512KB per-request arena */

/* ── Global State ──────────────────────────────────────────── */

static SeaProxyConfig s_proxy_cfg = {0};
static int            s_listen_fd = -1;
static pthread_t      s_proxy_tid;
static volatile bool  s_proxy_running = false;

/* ── HTTP Response Helpers ─────────────────────────────────── */

static void send_response(int fd, int status, const char* status_text,
                           const char* content_type, const char* body, int body_len) {
    char header[512];
    int hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n",
        status, status_text, content_type, body_len);

    ssize_t written = write(fd, header, (size_t)hlen);
    if (written < 0) {
        SEA_LOG_ERROR("PROXY", "Failed to write response header: %s", strerror(errno));
        return;
    }
    if (body && body_len > 0) {
        written = write(fd, body, (size_t)body_len);
        if (written < 0) {
            SEA_LOG_ERROR("PROXY", "Failed to write response body: %s", strerror(errno));
        }
    }
}

static void send_json_error(int fd, int status, const char* message) {
    char body[256];
    int blen = snprintf(body, sizeof(body),
        "{\"error\":{\"message\":\"%s\",\"type\":\"proxy_error\",\"code\":%d}}",
        message, status);
    send_response(fd, status, "Error", "application/json", body, blen);
}

static void send_json_ok(int fd, const char* body, int body_len) {
    send_response(fd, 200, "OK", "application/json", body, body_len);
}

/* ── Request Parsing ───────────────────────────────────────── */

typedef struct {
    char method[8];         /* GET, POST */
    char path[128];         /* /v1/chat/completions */
    char auth_token[128];   /* Bearer token from Authorization header */
    char* body;             /* POST body (points into buf) */
    int body_len;
} ProxyRequest;

static bool parse_request(char* buf, int buf_len, ProxyRequest* req) {
    memset(req, 0, sizeof(*req));

    /* Parse request line: METHOD PATH HTTP/1.x */
    char* line_end = strstr(buf, "\r\n");
    if (!line_end) return false;

    if (sscanf(buf, "%7s %127s", req->method, req->path) != 2) return false;

    /* Find Authorization header */
    char* auth = strcasestr(buf, "\nAuthorization: Bearer ");
    if (auth) {
        auth += strlen("\nAuthorization: Bearer ");
        char* auth_end = strstr(auth, "\r\n");
        if (auth_end) {
            int len = (int)(auth_end - auth);
            if (len > 0 && len < (int)sizeof(req->auth_token)) {
                memcpy(req->auth_token, auth, (size_t)len);
                req->auth_token[len] = '\0';
            }
        }
    }

    /* Find body (after \r\n\r\n) */
    char* body_start = strstr(buf, "\r\n\r\n");
    if (body_start) {
        body_start += 4;
        req->body = body_start;
        req->body_len = buf_len - (int)(body_start - buf);
    }

    return true;
}

/* ── Token Validation ──────────────────────────────────────── */

static bool validate_token(const ProxyRequest* req) {
    if (!s_proxy_cfg.internal_token || !s_proxy_cfg.internal_token[0]) {
        return true; /* No token configured = allow all (dev mode) */
    }
    return req->auth_token[0] &&
           strcmp(req->auth_token, s_proxy_cfg.internal_token) == 0;
}

/* ── Budget Check ──────────────────────────────────────────── */

static bool check_budget(const char* caller) {
    if (s_proxy_cfg.daily_token_budget <= 0) return true; /* Unlimited */
    if (!s_proxy_cfg.db) return true; /* No DB = can't track */

    i64 used = sea_db_sz_llm_total_tokens(s_proxy_cfg.db, caller);
    if (used >= s_proxy_cfg.daily_token_budget) {
        SEA_LOG_WARN("PROXY", "Budget exceeded for %s: %lld/%lld tokens",
                     caller, (long long)used, (long long)s_proxy_cfg.daily_token_budget);
        return false;
    }
    return true;
}

/* ── Handle /v1/chat/completions ─────────────────────────── */

static void handle_chat_completions(int fd, ProxyRequest* req) {
    if (!req->body || req->body_len <= 0) {
        send_json_error(fd, 400, "Empty request body");
        return;
    }

    /* Validate token */
    if (!validate_token(req)) {
        send_json_error(fd, 401, "Invalid authorization token");
        if (s_proxy_cfg.db) {
            sea_db_sz_audit(s_proxy_cfg.db, "auth_failure", "proxy",
                            "agent-zero", "Invalid internal token", "warn");
        }
        return;
    }

    /* Check budget */
    if (!check_budget("agent-zero")) {
        send_json_error(fd, 429, "Daily token budget exceeded");
        if (s_proxy_cfg.db) {
            sea_db_sz_audit(s_proxy_cfg.db, "budget_exceeded", "proxy",
                            "agent-zero", NULL, "warn");
        }
        return;
    }

    /* Create per-request arena */
    SeaArena arena;
    if (sea_arena_create(&arena, PROXY_ARENA_SIZE) != SEA_OK) {
        send_json_error(fd, 500, "Internal arena allocation failed");
        return;
    }

    /* Forward to real LLM */
    char auth_hdr[256];
    if (s_proxy_cfg.real_provider &&
        strcmp(s_proxy_cfg.real_provider, "anthropic") == 0) {
        snprintf(auth_hdr, sizeof(auth_hdr), "x-api-key: %s",
                 s_proxy_cfg.real_api_key);
    } else {
        snprintf(auth_hdr, sizeof(auth_hdr), "Authorization: Bearer %s",
                 s_proxy_cfg.real_api_key);
    }

    SeaSlice body = { .data = (const u8*)req->body, .len = (u32)req->body_len };

    SEA_LOG_INFO("PROXY", "Forwarding %d bytes to %s",
                 req->body_len, s_proxy_cfg.real_api_url);

    time_t t0 = time(NULL);
    SeaHttpResponse resp;
    SeaError err = sea_http_post_json_auth(
        s_proxy_cfg.real_api_url, body, auth_hdr, &arena, &resp);
    time_t t1 = time(NULL);
    i32 latency_ms = (i32)(difftime(t1, t0) * 1000);

    if (err != SEA_OK) {
        SEA_LOG_ERROR("PROXY", "LLM request failed: %s", sea_error_str(err));
        send_json_error(fd, 502, "LLM provider unreachable");

        if (s_proxy_cfg.db) {
            sea_db_sz_llm_log(s_proxy_cfg.db, "agent-zero",
                s_proxy_cfg.real_provider ? s_proxy_cfg.real_provider : "unknown",
                s_proxy_cfg.real_model ? s_proxy_cfg.real_model : "unknown",
                0, 0, 0.0, latency_ms, "error", NULL);
        }

        sea_arena_destroy(&arena);
        return;
    }

    /* Log usage — try to extract token counts from response */
    i32 tokens_in = 0, tokens_out = 0;
    SeaJsonValue root;
    if (sea_json_parse(resp.body, &arena, &root) == SEA_OK) {
        const SeaJsonValue* usage = sea_json_get(&root, "usage");
        if (usage) {
            tokens_in  = (i32)sea_json_get_number(usage, "prompt_tokens", 0);
            tokens_out = (i32)sea_json_get_number(usage, "completion_tokens", 0);
        }
    }

    if (s_proxy_cfg.db) {
        sea_db_sz_llm_log(s_proxy_cfg.db, "agent-zero",
            s_proxy_cfg.real_provider ? s_proxy_cfg.real_provider : "unknown",
            s_proxy_cfg.real_model ? s_proxy_cfg.real_model : "unknown",
            tokens_in, tokens_out, 0.0, latency_ms, "ok", NULL);
    }

    SEA_LOG_INFO("PROXY", "LLM response: HTTP %d, %u bytes, %dms (in=%d, out=%d)",
                 resp.status_code, resp.body.len, latency_ms, tokens_in, tokens_out);

    /* Forward response back to Agent Zero */
    send_response(fd, resp.status_code,
                  resp.status_code == 200 ? "OK" : "Error",
                  "application/json",
                  (const char*)resp.body.data, (int)resp.body.len);

    sea_arena_destroy(&arena);
}

/* ── Handle /health ────────────────────────────────────────── */

static void handle_health(int fd) {
    const char* body = "{\"status\":\"ok\",\"service\":\"seazero-proxy\"}";
    send_json_ok(fd, body, (int)strlen(body));
}

/* ── Connection Handler ────────────────────────────────────── */

static void handle_connection(int fd) {
    /* Read request (headers + body) */
    char buf[PROXY_MAX_HEADERS + PROXY_MAX_BODY];
    int total = 0;
    int n;

    while (total < (int)sizeof(buf) - 1) {
        n = (int)read(fd, buf + total, (size_t)(sizeof(buf) - 1 - (size_t)total));
        if (n <= 0) break;
        total += n;

        /* Check if we have the full request (headers end with \r\n\r\n) */
        buf[total] = '\0';
        char* header_end = strstr(buf, "\r\n\r\n");
        if (header_end) {
            /* Check Content-Length to know if we have the full body */
            char* cl = strcasestr(buf, "\nContent-Length: ");
            if (cl) {
                int content_len = atoi(cl + strlen("\nContent-Length: "));
                int header_size = (int)(header_end + 4 - buf);
                if (total >= header_size + content_len) break;
            } else {
                break; /* No Content-Length = no body expected */
            }
        }
    }

    if (total <= 0) {
        close(fd);
        return;
    }
    buf[total] = '\0';

    /* Parse request */
    ProxyRequest req;
    if (!parse_request(buf, total, &req)) {
        send_json_error(fd, 400, "Malformed request");
        close(fd);
        return;
    }

    SEA_LOG_DEBUG("PROXY", "%s %s", req.method, req.path);

    /* Route */
    if (strcmp(req.method, "POST") == 0 &&
        (strcmp(req.path, "/v1/chat/completions") == 0 ||
         strcmp(req.path, "/chat/completions") == 0)) {
        handle_chat_completions(fd, &req);
    } else if (strcmp(req.method, "GET") == 0 &&
               strcmp(req.path, "/health") == 0) {
        handle_health(fd);
    } else if (strcmp(req.method, "OPTIONS") == 0) {
        /* CORS preflight */
        send_response(fd, 204, "No Content", "text/plain", NULL, 0);
    } else {
        send_json_error(fd, 404, "Not found");
    }

    close(fd);
}

/* ── Server Thread ─────────────────────────────────────────── */

static void* proxy_thread(void* arg) {
    (void)arg;

    SEA_LOG_INFO("PROXY", "LLM proxy listening on port %u", s_proxy_cfg.port);

    while (s_proxy_running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(s_listen_fd,
                               (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (s_proxy_running && errno != EINTR) {
                SEA_LOG_ERROR("PROXY", "accept() failed: %s", strerror(errno));
            }
            continue;
        }

        /* Set read timeout on client socket */
        struct timeval tv = { .tv_sec = 30, .tv_usec = 0 };
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        handle_connection(client_fd);
    }

    SEA_LOG_INFO("PROXY", "Proxy thread exiting");
    return NULL;
}

/* ── Public API ────────────────────────────────────────────── */

int sea_proxy_start(const SeaProxyConfig* cfg) {
    if (!cfg || !cfg->enabled) return -1;
    if (s_proxy_running) return 0; /* Already running */

    s_proxy_cfg = *cfg;
    if (s_proxy_cfg.port == 0) s_proxy_cfg.port = 7432;

    if (!s_proxy_cfg.real_api_url || !s_proxy_cfg.real_api_key) {
        SEA_LOG_ERROR("PROXY", "Cannot start: no LLM API URL or key configured");
        return -1;
    }

    /* Create socket */
    s_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (s_listen_fd < 0) {
        SEA_LOG_ERROR("PROXY", "socket() failed: %s", strerror(errno));
        return -1;
    }

    /* Allow port reuse */
    int opt = 1;
    setsockopt(s_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* Bind */
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(s_proxy_cfg.port),
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK) /* 127.0.0.1 only */
    };

    if (bind(s_listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        SEA_LOG_ERROR("PROXY", "bind() port %u failed: %s",
                      s_proxy_cfg.port, strerror(errno));
        close(s_listen_fd);
        s_listen_fd = -1;
        return -1;
    }

    if (listen(s_listen_fd, PROXY_BACKLOG) < 0) {
        SEA_LOG_ERROR("PROXY", "listen() failed: %s", strerror(errno));
        close(s_listen_fd);
        s_listen_fd = -1;
        return -1;
    }

    /* Start background thread */
    s_proxy_running = true;
    if (pthread_create(&s_proxy_tid, NULL, proxy_thread, NULL) != 0) {
        SEA_LOG_ERROR("PROXY", "pthread_create() failed: %s", strerror(errno));
        s_proxy_running = false;
        close(s_listen_fd);
        s_listen_fd = -1;
        return -1;
    }

    SEA_LOG_INFO("PROXY", "LLM proxy started on 127.0.0.1:%u → %s",
                 s_proxy_cfg.port, s_proxy_cfg.real_api_url);

    if (s_proxy_cfg.db) {
        sea_db_sz_audit(s_proxy_cfg.db, "proxy_start", "seaclaw", NULL,
                        NULL, "info");
    }

    return 0;
}

void sea_proxy_stop(void) {
    if (!s_proxy_running) return;

    s_proxy_running = false;

    /* Close listen socket to unblock accept() */
    if (s_listen_fd >= 0) {
        close(s_listen_fd);
        s_listen_fd = -1;
    }

    pthread_join(s_proxy_tid, NULL);

    SEA_LOG_INFO("PROXY", "LLM proxy stopped");

    if (s_proxy_cfg.db) {
        sea_db_sz_audit(s_proxy_cfg.db, "proxy_stop", "seaclaw", NULL,
                        NULL, "info");
    }
}

bool sea_proxy_running(void) {
    return s_proxy_running;
}
