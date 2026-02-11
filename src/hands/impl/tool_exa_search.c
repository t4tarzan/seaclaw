/*
 * tool_exa_search.c — Real-time web search via Exa API
 *
 * Args: search query string
 * Returns: top results with title, URL, and text snippet
 *
 * Requires EXA_API_KEY environment variable.
 * API docs: https://docs.exa.ai/reference/search
 */

#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_http.h"
#include "seaclaw/sea_json.h"
#include "seaclaw/sea_shield.h"
#include "seaclaw/sea_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXA_API_URL   "https://api.exa.ai/search"
#define EXA_MAX_RESULTS 5
#define EXA_MAX_CHARS   2000
#define EXA_OUTPUT_MAX  8192

SeaError tool_exa_search(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Error: no search query provided");
        return SEA_OK;
    }

    /* Get API key from environment */
    const char* api_key = getenv("EXA_API_KEY");
    if (!api_key || !*api_key) {
        *output = SEA_SLICE_LIT("Error: EXA_API_KEY not set");
        return SEA_OK;
    }

    /* Null-terminate and trim query */
    char query[512];
    u32 qlen = args.len < sizeof(query) - 1 ? args.len : (u32)(sizeof(query) - 1);
    memcpy(query, args.data, qlen);
    query[qlen] = '\0';
    char* q = query;
    while (*q == ' ' || *q == '\t') q++;
    char* end = q + strlen(q) - 1;
    while (end > q && (*end == ' ' || *end == '\t' || *end == '\n')) *end-- = '\0';

    if (strlen(q) == 0) {
        *output = SEA_SLICE_LIT("Error: empty search query");
        return SEA_OK;
    }

    /* Shield: validate query */
    SeaSlice q_slice = { .data = (const u8*)q, .len = (u32)strlen(q) };
    if (sea_shield_detect_injection(q_slice)) {
        *output = SEA_SLICE_LIT("Error: query rejected by Shield");
        return SEA_OK;
    }

    /* Build JSON request body with escaped query */
    char req_body[1024];
    /* Escape query for JSON */
    char escaped[512];
    u32 ei = 0;
    for (const char* p = q; *p && ei < sizeof(escaped) - 6; p++) {
        switch (*p) {
            case '"':  escaped[ei++] = '\\'; escaped[ei++] = '"'; break;
            case '\\': escaped[ei++] = '\\'; escaped[ei++] = '\\'; break;
            case '\n': escaped[ei++] = '\\'; escaped[ei++] = 'n'; break;
            case '\r': escaped[ei++] = '\\'; escaped[ei++] = 'r'; break;
            case '\t': escaped[ei++] = '\\'; escaped[ei++] = 't'; break;
            default:   escaped[ei++] = *p; break;
        }
    }
    escaped[ei] = '\0';

    int blen = snprintf(req_body, sizeof(req_body),
        "{\"query\":\"%s\",\"type\":\"auto\",\"num_results\":%d,"
        "\"contents\":{\"text\":{\"max_characters\":%d}}}",
        escaped, EXA_MAX_RESULTS, EXA_MAX_CHARS);

    /* Build auth header */
    char auth_hdr[128];
    snprintf(auth_hdr, sizeof(auth_hdr), "x-api-key: %s", api_key);

    /* Make HTTP request */
    SeaSlice body = { .data = (const u8*)req_body, .len = (u32)blen };
    SeaHttpResponse resp;
    SeaError err = sea_http_post_json_auth(EXA_API_URL, body, auth_hdr, arena, &resp);

    if (err != SEA_OK) {
        *output = SEA_SLICE_LIT("Error: Exa API request failed");
        return SEA_OK;
    }

    if (resp.status_code != 200) {
        char msg[256];
        int mlen = snprintf(msg, sizeof(msg), "Exa API error (HTTP %d): %.*s",
                            resp.status_code,
                            (int)(resp.body.len > 150 ? 150 : resp.body.len),
                            (const char*)resp.body.data);
        u8* dst = (u8*)sea_arena_push_bytes(arena, msg, (u64)mlen);
        if (dst) { output->data = dst; output->len = (u32)mlen; }
        return SEA_OK;
    }

    /* Parse JSON response */
    SeaJsonValue root;
    err = sea_json_parse(resp.body, arena, &root);
    if (err != SEA_OK) {
        *output = SEA_SLICE_LIT("Error: failed to parse Exa response");
        return SEA_OK;
    }

    const SeaJsonValue* results = sea_json_get(&root, "results");
    if (!results || results->type != SEA_JSON_ARRAY || results->array.count == 0) {
        *output = SEA_SLICE_LIT("No results found.");
        return SEA_OK;
    }

    /* Format results */
    char* buf = (char*)sea_arena_alloc(arena, EXA_OUTPUT_MAX, 1);
    if (!buf) {
        *output = SEA_SLICE_LIT("Error: out of memory");
        return SEA_OK;
    }

    int pos = snprintf(buf, EXA_OUTPUT_MAX, "Web search: \"%s\" (%u results)\n",
                       q, results->array.count);

    for (u32 i = 0; i < results->array.count && pos < EXA_OUTPUT_MAX - 200; i++) {
        const SeaJsonValue* item = &results->array.items[i];

        SeaSlice title = sea_json_get_string(item, "title");
        SeaSlice url   = sea_json_get_string(item, "url");
        SeaSlice text  = sea_json_get_string(item, "text");

        pos += snprintf(buf + pos, (size_t)(EXA_OUTPUT_MAX - pos),
            "\n[%u] %.*s\n    %.*s\n",
            i + 1,
            (int)(title.len > 80 ? 80 : title.len),
            title.data ? (const char*)title.data : "?",
            (int)(url.len > 120 ? 120 : url.len),
            url.data ? (const char*)url.data : "?");

        /* Add text snippet (first 300 chars) */
        if (text.len > 0 && text.data) {
            u32 snip = text.len > 300 ? 300 : text.len;
            pos += snprintf(buf + pos, (size_t)(EXA_OUTPUT_MAX - pos),
                "    %.*s%s\n",
                (int)snip, (const char*)text.data,
                text.len > 300 ? "..." : "");
        }
    }

    output->data = (const u8*)buf;
    output->len  = (u32)pos;

    SEA_LOG_INFO("HANDS", "Exa search: \"%s\" → %u results", q, results->array.count);
    return SEA_OK;
}
