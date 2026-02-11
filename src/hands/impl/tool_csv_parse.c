/*
 * tool_csv_parse.c — Parse CSV data and extract columns
 *
 * Tool ID:    26
 * Category:   Data Processing
 * Args:       <column_number|headers|count> <csv_data>
 * Returns:    Extracted column values, header list, or row count
 *
 * Supports comma and tab delimiters. Handles quoted fields.
 *
 * Examples:
 *   /exec csv_parse headers "name,age,city\nAlice,30,NYC"
 *   /exec csv_parse 2 "name,age,city\nAlice,30,NYC\nBob,25,LA"
 *   /exec csv_parse count "name,age\nAlice,30\nBob,25"
 *
 * Security: Input validated by standard tool pipeline.
 */

#include "seaclaw/sea_tools.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define MAX_OUTPUT 8192
#define MAX_COLS   64
#define MAX_ROWS   200

static u32 parse_csv_field(const char* line, u32 pos, u32 len, char* out, u32 omax) {
    u32 o = 0;
    if (pos < len && line[pos] == '"') {
        pos++;
        while (pos < len && o < omax - 1) {
            if (line[pos] == '"') {
                if (pos + 1 < len && line[pos+1] == '"') { out[o++] = '"'; pos += 2; }
                else { pos++; break; }
            } else { out[o++] = line[pos++]; }
        }
        if (pos < len && line[pos] == ',') pos++;
    } else {
        while (pos < len && line[pos] != ',' && line[pos] != '\t' && o < omax - 1)
            out[o++] = line[pos++];
        if (pos < len && (line[pos] == ',' || line[pos] == '\t')) pos++;
    }
    out[o] = '\0';
    return pos;
}

SeaError tool_csv_parse(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Usage: <column_num|headers|count> <csv_data>");
        return SEA_OK;
    }

    char input[8192];
    u32 ilen = args.len < sizeof(input) - 1 ? args.len : (u32)(sizeof(input) - 1);
    memcpy(input, args.data, ilen);
    input[ilen] = '\0';

    /* Parse operation */
    char op[16];
    u32 i = 0;
    while (i < ilen && i < sizeof(op) - 1 && input[i] != ' ') { op[i] = input[i]; i++; }
    op[i] = '\0';
    while (i < ilen && input[i] == ' ') i++;

    const char* csv = input + i;
    u32 csv_len = ilen - i;

    if (csv_len == 0) {
        *output = SEA_SLICE_LIT("Error: no CSV data provided");
        return SEA_OK;
    }

    /* Unescape literal \n to newlines */
    char* data = (char*)sea_arena_alloc(arena, csv_len + 1, 1);
    if (!data) return SEA_ERR_ARENA_FULL;
    u32 di = 0;
    for (u32 j = 0; j < csv_len; j++) {
        if (csv[j] == '\\' && j + 1 < csv_len && csv[j+1] == 'n') { data[di++] = '\n'; j++; }
        else data[di++] = csv[j];
    }
    data[di] = '\0';

    char* buf = (char*)sea_arena_alloc(arena, MAX_OUTPUT, 1);
    if (!buf) return SEA_ERR_ARENA_FULL;
    int pos = 0;

    /* Split into lines */
    char* lines[MAX_ROWS];
    u32 line_count = 0;
    char* tok = data;
    while (tok && line_count < MAX_ROWS) {
        lines[line_count] = tok;
        char* nl = strchr(tok, '\n');
        if (nl) { *nl = '\0'; tok = nl + 1; } else tok = NULL;
        if (strlen(lines[line_count]) > 0) line_count++;
    }

    if (strcmp(op, "count") == 0) {
        pos = snprintf(buf, MAX_OUTPUT, "Rows: %u (including header)", line_count);
    } else if (strcmp(op, "headers") == 0) {
        if (line_count == 0) { *output = SEA_SLICE_LIT("No data"); return SEA_OK; }
        pos = snprintf(buf, MAX_OUTPUT, "Headers:\n");
        char field[256];
        u32 fp = 0, col = 0;
        u32 hlen = (u32)strlen(lines[0]);
        while (fp < hlen && col < MAX_COLS) {
            fp = parse_csv_field(lines[0], fp, hlen, field, sizeof(field));
            pos += snprintf(buf + pos, (size_t)(MAX_OUTPUT - pos), "  [%u] %s\n", col + 1, field);
            col++;
        }
        pos += snprintf(buf + pos, (size_t)(MAX_OUTPUT - pos), "(%u columns)", col);
    } else {
        /* Extract column by number */
        int col_num = 0;
        for (u32 j = 0; op[j]; j++) { if (isdigit((unsigned char)op[j])) col_num = col_num * 10 + (op[j] - '0'); }
        if (col_num < 1) {
            pos = snprintf(buf, MAX_OUTPUT, "Error: invalid column '%s'. Use 1-based number, 'headers', or 'count'", op);
        } else {
            pos = snprintf(buf, MAX_OUTPUT, "Column %d:\n", col_num);
            for (u32 r = 0; r < line_count && pos < MAX_OUTPUT - 256; r++) {
                char field[256];
                u32 fp = 0, col = 0;
                u32 llen = (u32)strlen(lines[r]);
                while (fp < llen && col < (u32)col_num) {
                    fp = parse_csv_field(lines[r], fp, llen, field, sizeof(field));
                    col++;
                }
                if (col == (u32)col_num) {
                    pos += snprintf(buf + pos, (size_t)(MAX_OUTPUT - pos), "  [%u] %s\n", r + 1, field);
                }
            }
        }
    }

    output->data = (const u8*)buf;
    output->len  = (u32)pos;
    return SEA_OK;
}
