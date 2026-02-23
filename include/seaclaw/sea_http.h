/*
 * sea_http.h — Minimal HTTP client
 *
 * HTTPS GET/POST using raw sockets + OpenSSL.
 * Used for Telegram Bot API and LLM provider calls.
 * Response body allocated in arena (zero-copy spirit).
 */

#ifndef SEA_HTTP_H
#define SEA_HTTP_H

#include "sea_types.h"
#include "sea_arena.h"

typedef struct {
    i32      status_code;
    SeaSlice body;
    SeaSlice headers;
} SeaHttpResponse;

/* HTTP GET — response body allocated in arena */
SeaError sea_http_get(const char* url, SeaArena* arena, SeaHttpResponse* resp);

/* HTTP POST with JSON body — response body allocated in arena */
SeaError sea_http_post_json(const char* url, SeaSlice json_body,
                            SeaArena* arena, SeaHttpResponse* resp);

/* HTTP GET with custom auth header */
SeaError sea_http_get_auth(const char* url, const char* auth_header,
                            SeaArena* arena, SeaHttpResponse* resp);

/* HTTP POST with JSON body + Authorization header */
SeaError sea_http_post_json_auth(const char* url, SeaSlice json_body,
                                 const char* auth_header,
                                 SeaArena* arena, SeaHttpResponse* resp);

/* HTTP POST with JSON body + multiple custom headers (NULL-terminated array) */
SeaError sea_http_post_json_headers(const char* url, SeaSlice json_body,
                                    const char** extra_headers,
                                    SeaArena* arena, SeaHttpResponse* resp);

/* SSE streaming callback: called for each data line. Return false to abort. */
typedef bool (*SeaHttpStreamCb)(const char* data, u32 data_len, void* user_data);

/* HTTP POST with SSE streaming — calls stream_cb for each `data:` line.
 * Also accumulates full response body in resp for final parsing. */
SeaError sea_http_post_stream(const char* url, SeaSlice json_body,
                               const char** extra_headers,
                               SeaHttpStreamCb stream_cb, void* cb_data,
                               SeaArena* arena, SeaHttpResponse* resp);

#endif /* SEA_HTTP_H */
