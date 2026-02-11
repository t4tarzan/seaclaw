/*
 * tool_web_search.c — Web search via Brave Search API
 *
 * Args: search query string
 * Returns: top results with title, URL, and description
 *
 * Requires BRAVE_API_KEY environment variable.
 * API docs: https://api.search.brave.com/app/documentation/web-search
 */

#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_http.h"
#include "seaclaw/sea_json.h"
#include "seaclaw/sea_shield.h"
#include "seaclaw/sea_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BRAVE_API_URL    "https://api.search.brave.com/res/v1/web/search"
#define BRAVE_MAX_RESULTS 5
#define BRAVE_OUTPUT_MAX  8192

SeaError tool_web_search(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Error: no search query provided. Usage: web_search <query>");
        return SEA_OK;
    }

    /* Get API key */
    const char* api_key = getenv("BRAVE_API_KEY");
    if (!api_key || !*api_key) {
        *output = SEA_SLICE_LIT("Error: BRAVE_API_KEY not set. Get one at https://api.search.brave.com/");
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

    /* URL-encode query for GET request */
    char encoded[1024];
    u32 ei = 0;
    for (const char* p = q; *p && ei < sizeof(encoded) - 4; p++) {
        unsigned char c = (unsigned char)*p;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded[ei++] = (char)c;
        } else {
            ei += (u32)snprintf(encoded + ei, sizeof(encoded) - ei, "%%%02X", c);
        }
    }
    encoded[ei] = '\0';

    /* Build URL */
    char url[2048];
    snprintf(url, sizeof(url), "%s?q=%s&count=%d", BRAVE_API_URL, encoded, BRAVE_MAX_RESULTS);

    /* Build auth header */
    char auth_hdr[256];
    snprintf(auth_hdr, sizeof(auth_hdr), "X-Subscription-Token: %s", api_key);

    /* Make HTTP GET request with auth header */
    SeaHttpResponse resp;
    SeaError err = sea_http_get_auth(url, auth_hdr, arena, &resp);

    if (err != SEA_OK) {
        *output = SEA_SLICE_LIT("Error: Brave Search API request failed");
        return SEA_OK;
    }

    if (resp.status_code != 200) {
        char msg[256];
        int mlen = snprintf(msg, sizeof(msg), "Brave API error (HTTP %d): %.*s",
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
        *output = SEA_SLICE_LIT("Error: failed to parse Brave response");
        return SEA_OK;
    }

    /* Extract web results */
    const SeaJsonValue* web = sea_json_get(&root, "web");
    const SeaJsonValue* results = web ? sea_json_get(web, "results") : NULL;

    if (!results || results->type != SEA_JSON_ARRAY || results->array.count == 0) {
        *output = SEA_SLICE_LIT("No results found.");
        return SEA_OK;
    }

    /* Format results */
    char* buf = (char*)sea_arena_alloc(arena, BRAVE_OUTPUT_MAX, 1);
    if (!buf) return SEA_ERR_ARENA_FULL;

    int pos = snprintf(buf, BRAVE_OUTPUT_MAX, "Web search: \"%s\" (%u results)\n",
                       q, results->array.count);

    for (u32 i = 0; i < results->array.count && pos < BRAVE_OUTPUT_MAX - 300; i++) {
        const SeaJsonValue* item = &results->array.items[i];

        SeaSlice title = sea_json_get_string(item, "title");
        SeaSlice url_s = sea_json_get_string(item, "url");
        SeaSlice desc  = sea_json_get_string(item, "description");

        pos += snprintf(buf + pos, (size_t)(BRAVE_OUTPUT_MAX - pos),
            "\n[%u] %.*s\n    %.*s\n",
            i + 1,
            (int)(title.len > 100 ? 100 : title.len),
            title.data ? (const char*)title.data : "?",
            (int)(url_s.len > 150 ? 150 : url_s.len),
            url_s.data ? (const char*)url_s.data : "?");

        if (desc.len > 0 && desc.data) {
            u32 snip = desc.len > 300 ? 300 : desc.len;
            pos += snprintf(buf + pos, (size_t)(BRAVE_OUTPUT_MAX - pos),
                "    %.*s%s\n",
                (int)snip, (const char*)desc.data,
                desc.len > 300 ? "..." : "");
        }
    }

    output->data = (const u8*)buf;
    output->len  = (u32)pos;

    SEA_LOG_INFO("HANDS", "Brave search: \"%s\" -> %u results", q, results->array.count);
    return SEA_OK;
}
