/*
 * tool_json_format.c — Validate and pretty-print JSON
 *
 * Args: JSON string
 * Returns: formatted JSON or validation error
 */

#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_json.h"
#include <stdio.h>
#include <string.h>

static void json_indent(char* buf, u32* pos, u32 cap, u32 depth) {
    for (u32 i = 0; i < depth * 2 && *pos < cap - 1; i++)
        buf[(*pos)++] = ' ';
}

static void json_pretty(const SeaJsonValue* v, char* buf, u32* pos, u32 cap, u32 depth) {
    if (*pos >= cap - 32) return;

    switch (v->type) {
        case SEA_JSON_NULL:
            *pos += (u32)snprintf(buf + *pos, cap - *pos, "null");
            break;
        case SEA_JSON_BOOL:
            *pos += (u32)snprintf(buf + *pos, cap - *pos, "%s", v->boolean ? "true" : "false");
            break;
        case SEA_JSON_NUMBER:
            if (v->number == (i64)v->number)
                *pos += (u32)snprintf(buf + *pos, cap - *pos, "%lld", (long long)(i64)v->number);
            else
                *pos += (u32)snprintf(buf + *pos, cap - *pos, "%g", v->number);
            break;
        case SEA_JSON_STRING:
            *pos += (u32)snprintf(buf + *pos, cap - *pos, "\"%.*s\"",
                                  (int)v->string.len, (const char*)v->string.data);
            break;
        case SEA_JSON_ARRAY:
            buf[(*pos)++] = '[';
            if (v->array.count > 0) {
                buf[(*pos)++] = '\n';
                for (u32 i = 0; i < v->array.count && *pos < cap - 32; i++) {
                    json_indent(buf, pos, cap, depth + 1);
                    json_pretty(&v->array.items[i], buf, pos, cap, depth + 1);
                    if (i + 1 < v->array.count) buf[(*pos)++] = ',';
                    buf[(*pos)++] = '\n';
                }
                json_indent(buf, pos, cap, depth);
            }
            buf[(*pos)++] = ']';
            break;
        case SEA_JSON_OBJECT:
            buf[(*pos)++] = '{';
            if (v->object.count > 0) {
                buf[(*pos)++] = '\n';
                for (u32 i = 0; i < v->object.count && *pos < cap - 32; i++) {
                    json_indent(buf, pos, cap, depth + 1);
                    *pos += (u32)snprintf(buf + *pos, cap - *pos, "\"%.*s\": ",
                                          (int)v->object.keys[i].len,
                                          (const char*)v->object.keys[i].data);
                    json_pretty(&v->object.values[i], buf, pos, cap, depth + 1);
                    if (i + 1 < v->object.count) buf[(*pos)++] = ',';
                    buf[(*pos)++] = '\n';
                }
                json_indent(buf, pos, cap, depth);
            }
            buf[(*pos)++] = '}';
            break;
    }
}

SeaError tool_json_format(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Error: no JSON provided");
        return SEA_OK;
    }

    SeaJsonValue root;
    SeaError err = sea_json_parse(args, arena, &root);
    if (err != SEA_OK) {
        *output = SEA_SLICE_LIT("Invalid JSON: parse error");
        return SEA_OK;
    }

    u32 cap = args.len * 3 + 1024;
    if (cap > 16384) cap = 16384;
    char* buf = (char*)sea_arena_alloc(arena, cap, 1);
    if (!buf) return SEA_ERR_ARENA_FULL;

    u32 pos = 0;
    json_pretty(&root, buf, &pos, cap, 0);
    buf[pos] = '\0';

    output->data = (const u8*)buf;
    output->len  = pos;
    return SEA_OK;
}
