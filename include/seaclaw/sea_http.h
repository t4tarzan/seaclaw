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

/* HTTP POST with JSON body + Authorization header */
SeaError sea_http_post_json_auth(const char* url, SeaSlice json_body,
                                 const char* auth_header,
                                 SeaArena* arena, SeaHttpResponse* resp);

#endif /* SEA_HTTP_H */
