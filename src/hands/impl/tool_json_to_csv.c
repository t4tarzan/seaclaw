/*
 * tool_json_to_csv.c — Convert JSON array to CSV format
 *
 * Tool ID:    46
 * Category:   Data Processing
 * Args:       <json_array>
 * Returns:    CSV with headers from first object's keys
 *
 * Input must be a JSON array of objects with consistent keys.
 *
 * Examples:
 *   /exec json_to_csv [{"name":"Alice","age":30},{"name":"Bob","age":25}]
 *
 * Security: Input validated by standard tool pipeline.
 */

#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_json.h"
#include <stdio.h>
#include <string.h>

#define MAX_OUTPUT 8192

static int append_csv_field(char* buf, int pos, int cap, SeaSlice val) {
    bool needs_quote = false;
    for (u32 i = 0; i < val.len; i++) {
        if (val.data[i] == ',' || val.data[i] == '"' || val.data[i] == '\n') {
            needs_quote = true; break;
        }
    }
    if (needs_quote) {
        if (pos < cap - 1) buf[pos++] = '"';
        for (u32 i = 0; i < val.len && pos < cap - 2; i++) {
            if (val.data[i] == '"') { buf[pos++] = '"'; }
            buf[pos++] = (char)val.data[i];
        }
        if (pos < cap - 1) buf[pos++] = '"';
    } else {
        u32 clen = val.len < (u32)(cap - pos - 1) ? val.len : (u32)(cap - pos - 1);
        memcpy(buf + pos, val.data, clen);
        pos += (int)clen;
    }
    return pos;
}

SeaError tool_json_to_csv(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Usage: <json_array_of_objects>");
        return SEA_OK;
    }

    SeaJsonValue root;
    SeaError err = sea_json_parse(args, arena, &root);
    if (err != SEA_OK) {
        *output = SEA_SLICE_LIT("Error: invalid JSON");
        return SEA_OK;
    }

    if (root.type != SEA_JSON_ARRAY || root.array.count == 0) {
        *output = SEA_SLICE_LIT("Error: input must be a non-empty JSON array");
        return SEA_OK;
    }

    const SeaJsonValue* first = &root.array.items[0];
    if (first->type != SEA_JSON_OBJECT) {
        *output = SEA_SLICE_LIT("Error: array items must be objects");
        return SEA_OK;
    }

    char* buf = (char*)sea_arena_alloc(arena, MAX_OUTPUT, 1);
    if (!buf) return SEA_ERR_ARENA_FULL;
    int pos = 0;

    /* Header row from first object's keys */
    for (u32 k = 0; k < first->object.count && pos < MAX_OUTPUT - 64; k++) {
        if (k > 0) buf[pos++] = ',';
        pos = append_csv_field(buf, pos, MAX_OUTPUT, first->object.keys[k]);
    }
    buf[pos++] = '\n';

    /* Data rows */
    for (u32 r = 0; r < root.array.count && pos < MAX_OUTPUT - 256; r++) {
        const SeaJsonValue* row = &root.array.items[r];
        if (row->type != SEA_JSON_OBJECT) continue;

        for (u32 k = 0; k < first->object.count && pos < MAX_OUTPUT - 64; k++) {
            if (k > 0) buf[pos++] = ',';
            /* Find matching key in this row */
            const SeaJsonValue* val = NULL;
            for (u32 j = 0; j < row->object.count; j++) {
                if (row->object.keys[j].len == first->object.keys[k].len &&
                    memcmp(row->object.keys[j].data, first->object.keys[k].data,
                           first->object.keys[k].len) == 0) {
                    val = &row->object.values[j];
                    break;
                }
            }
            if (val) {
                char tmp[256];
                int tlen = 0;
                switch (val->type) {
                    case SEA_JSON_STRING:
                        pos = append_csv_field(buf, pos, MAX_OUTPUT, val->string);
                        continue;
                    case SEA_JSON_NUMBER:
                        if (val->number == (i64)val->number)
                            tlen = snprintf(tmp, sizeof(tmp), "%lld", (long long)(i64)val->number);
                        else
                            tlen = snprintf(tmp, sizeof(tmp), "%g", val->number);
                        break;
                    case SEA_JSON_BOOL:
                        tlen = snprintf(tmp, sizeof(tmp), "%s", val->boolean ? "true" : "false");
                        break;
                    case SEA_JSON_NULL:
                        tlen = 0; break;
                    default:
                        tlen = snprintf(tmp, sizeof(tmp), "[complex]"); break;
                }
                if (tlen > 0 && pos + tlen < MAX_OUTPUT) {
                    memcpy(buf + pos, tmp, (size_t)tlen);
                    pos += tlen;
                }
            }
        }
        buf[pos++] = '\n';
    }

    output->data = (const u8*)buf;
    output->len  = (u32)pos;
    return SEA_OK;
}
