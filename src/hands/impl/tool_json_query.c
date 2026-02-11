/*
 * tool_json_query.c — Query JSON data by key path
 *
 * Tool ID:    36
 * Category:   Data Processing
 * Args:       <key.path> <json_data>
 * Returns:    Value at the specified path
 *
 * Supports dot-notation for nested objects and [N] for arrays.
 *
 * Examples:
 *   /exec json_query name {"name":"Alice","age":30}
 *   /exec json_query users[0].name {"users":[{"name":"Bob"}]}
 *   /exec json_query config.db.host {"config":{"db":{"host":"localhost"}}}
 *
 * Security: Input validated by standard tool pipeline.
 */

#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_json.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

static const SeaJsonValue* walk_path(const SeaJsonValue* root, const char* path) {
    const SeaJsonValue* cur = root;
    char segment[128];

    while (*path && cur) {
        /* Parse next segment */
        u32 si = 0;
        while (*path && *path != '.' && *path != '[' && si < sizeof(segment) - 1)
            segment[si++] = *path++;
        segment[si] = '\0';
        if (*path == '.') path++;

        /* Object key lookup */
        if (si > 0 && cur->type == SEA_JSON_OBJECT) {
            cur = sea_json_get(cur, segment);
            if (!cur) return NULL;
        }

        /* Array index */
        if (*path == '[' || segment[0] == '[') {
            if (*path == '[') path++;
            u32 idx = 0;
            while (*path && isdigit((unsigned char)*path)) { idx = idx * 10 + (*path - '0'); path++; }
            if (*path == ']') path++;
            if (*path == '.') path++;

            if (cur->type != SEA_JSON_ARRAY || idx >= cur->array.count) return NULL;
            cur = &cur->array.items[idx];
        }
    }
    return cur;
}

static int format_value(const SeaJsonValue* v, char* buf, u32 cap) {
    if (!v) return snprintf(buf, cap, "null");
    switch (v->type) {
        case SEA_JSON_NULL:   return snprintf(buf, cap, "null");
        case SEA_JSON_BOOL:   return snprintf(buf, cap, "%s", v->boolean ? "true" : "false");
        case SEA_JSON_NUMBER:
            if (v->number == (i64)v->number)
                return snprintf(buf, cap, "%lld", (long long)(i64)v->number);
            return snprintf(buf, cap, "%g", v->number);
        case SEA_JSON_STRING:
            return snprintf(buf, cap, "%.*s", (int)v->string.len, (const char*)v->string.data);
        case SEA_JSON_ARRAY:
            return snprintf(buf, cap, "[array, %u items]", v->array.count);
        case SEA_JSON_OBJECT:
            return snprintf(buf, cap, "{object, %u keys}", v->object.count);
    }
    return 0;
}

SeaError tool_json_query(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Usage: <key.path> <json_data>\nExample: name {\"name\":\"Alice\"}");
        return SEA_OK;
    }

    char input[4096];
    u32 ilen = args.len < sizeof(input) - 1 ? args.len : (u32)(sizeof(input) - 1);
    memcpy(input, args.data, ilen);
    input[ilen] = '\0';

    /* Find where JSON starts (first { or [) */
    char* json_start = NULL;
    for (u32 i = 0; i < ilen; i++) {
        if (input[i] == '{' || input[i] == '[') { json_start = input + i; break; }
    }
    if (!json_start) {
        *output = SEA_SLICE_LIT("Error: no JSON found in input");
        return SEA_OK;
    }

    /* Extract path (everything before JSON) */
    char path[256];
    u32 plen = (u32)(json_start - input);
    while (plen > 0 && input[plen-1] == ' ') plen--;
    if (plen >= sizeof(path)) plen = sizeof(path) - 1;
    memcpy(path, input, plen);
    path[plen] = '\0';
    char* pp = path;
    while (*pp == ' ') pp++;

    /* Parse JSON */
    SeaSlice json_slice = { .data = (const u8*)json_start, .len = ilen - (u32)(json_start - input) };
    SeaJsonValue root;
    SeaError err = sea_json_parse(json_slice, arena, &root);
    if (err != SEA_OK) {
        *output = SEA_SLICE_LIT("Error: invalid JSON");
        return SEA_OK;
    }

    const SeaJsonValue* result = walk_path(&root, pp);
    char buf[2048];
    int len;
    if (!result) {
        len = snprintf(buf, sizeof(buf), "Path '%s': not found", pp);
    } else {
        len = format_value(result, buf, sizeof(buf));
    }

    u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)len);
    if (!dst) return SEA_ERR_ARENA_FULL;
    output->data = dst; output->len = (u32)len;
    return SEA_OK;
}
