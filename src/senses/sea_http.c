/*
 * sea_http.c — Minimal HTTP client using libcurl
 *
 * Wraps curl for HTTPS GET/POST. Response body goes into arena.
 */

#include "seaclaw/sea_http.h"
#include "seaclaw/sea_log.h"
#include <curl/curl.h>
#include <string.h>

/* ── Write callback: append to arena ──────────────────────── */

typedef struct {
    SeaArena* arena;
    u8*       buf;
    u64       len;
    u64       cap;
} WriteCtx;

static size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    WriteCtx* ctx = (WriteCtx*)userdata;
    u64 bytes = size * nmemb;

    if (ctx->len + bytes > ctx->cap) {
        /* Grow buffer in arena */
        u64 new_cap = (ctx->cap == 0) ? 4096 : ctx->cap * 2;
        while (new_cap < ctx->len + bytes) new_cap *= 2;

        u8* new_buf = (u8*)sea_arena_alloc(ctx->arena, new_cap, 1);
        if (!new_buf) return 0; /* Signal error to curl */

        if (ctx->buf && ctx->len > 0) {
            memcpy(new_buf, ctx->buf, ctx->len);
        }
        ctx->buf = new_buf;
        ctx->cap = new_cap;
    }

    memcpy(ctx->buf + ctx->len, ptr, bytes);
    ctx->len += bytes;
    return bytes;
}

/* ── Internal request ─────────────────────────────────────── */

static SeaError do_request(const char* url, const char* method,
                           SeaSlice* post_body, const char* auth_header,
                           SeaArena* arena, SeaHttpResponse* resp) {
    CURL* curl = curl_easy_init();
    if (!curl) return SEA_ERR_CONNECT;

    WriteCtx ctx = { .arena = arena, .buf = NULL, .len = 0, .cap = 0 };

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Sea-Claw/" SEA_VERSION_STRING);

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Accept-Language: en-US,en");

    if (auth_header) {
        headers = curl_slist_append(headers, auth_header);
    }

    if (strcmp(method, "POST") == 0 && post_body) {
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, (const char*)post_body->data);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)post_body->len);
    }

    if (headers) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        SEA_LOG_ERROR("HTTP", "%s %s failed: %s", method, url, curl_easy_strerror(res));
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        if (res == CURLE_OPERATION_TIMEDOUT) return SEA_ERR_TIMEOUT;
        return SEA_ERR_CONNECT;
    }

    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);

    resp->status_code = (i32)status;
    resp->body.data   = ctx.buf;
    resp->body.len    = (u32)ctx.len;
    resp->headers     = SEA_SLICE_EMPTY;

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return SEA_OK;
}

/* ── Public API ───────────────────────────────────────────── */

SeaError sea_http_get(const char* url, SeaArena* arena, SeaHttpResponse* resp) {
    if (!url || !arena || !resp) return SEA_ERR_IO;
    SEA_LOG_DEBUG("HTTP", "GET %s", url);
    return do_request(url, "GET", NULL, NULL, arena, resp);
}

SeaError sea_http_get_auth(const char* url, const char* auth_header,
                            SeaArena* arena, SeaHttpResponse* resp) {
    if (!url || !arena || !resp) return SEA_ERR_IO;
    SEA_LOG_DEBUG("HTTP", "GET %s (auth)", url);
    return do_request(url, "GET", NULL, auth_header, arena, resp);
}

SeaError sea_http_post_json(const char* url, SeaSlice json_body,
                            SeaArena* arena, SeaHttpResponse* resp) {
    if (!url || !arena || !resp) return SEA_ERR_IO;
    SEA_LOG_DEBUG("HTTP", "POST %s (%u bytes)", url, json_body.len);
    return do_request(url, "POST", &json_body, NULL, arena, resp);
}

SeaError sea_http_post_json_auth(const char* url, SeaSlice json_body,
                                 const char* auth_header,
                                 SeaArena* arena, SeaHttpResponse* resp) {
    if (!url || !arena || !resp) return SEA_ERR_IO;
    SEA_LOG_DEBUG("HTTP", "POST %s (%u bytes, auth)", url, json_body.len);
    return do_request(url, "POST", &json_body, auth_header, arena, resp);
}
