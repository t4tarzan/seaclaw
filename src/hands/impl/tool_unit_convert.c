/*
 * tool_unit_convert.c — Unit conversion utility
 *
 * Tool ID:    48
 * Category:   Math / Utility
 * Args:       <value> <from_unit> <to_unit>
 * Returns:    Converted value
 *
 * Supported conversions:
 *   Length:  km, m, cm, mm, mi, ft, in, yd
 *   Weight:  kg, g, lb, oz
 *   Temp:    c, f, k (celsius, fahrenheit, kelvin)
 *   Data:    b, kb, mb, gb, tb (bytes)
 *   Time:    s, ms, min, hr, day
 *
 * Examples:
 *   /exec unit_convert 100 km mi
 *   /exec unit_convert 72 f c
 *   /exec unit_convert 1024 mb gb
 *   /exec unit_convert 3600 s hr
 *
 * Security: Input validated by standard tool pipeline.
 */

#include "seaclaw/sea_tools.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

typedef struct { const char* name; f64 to_base; } Unit;

static const Unit length_units[] = {
    {"km", 1000.0}, {"m", 1.0}, {"cm", 0.01}, {"mm", 0.001},
    {"mi", 1609.344}, {"ft", 0.3048}, {"in", 0.0254}, {"yd", 0.9144},
    {NULL, 0}
};
static const Unit weight_units[] = {
    {"kg", 1.0}, {"g", 0.001}, {"lb", 0.453592}, {"oz", 0.0283495},
    {NULL, 0}
};
static const Unit data_units[] = {
    {"b", 1.0}, {"kb", 1024.0}, {"mb", 1048576.0},
    {"gb", 1073741824.0}, {"tb", 1099511627776.0},
    {NULL, 0}
};
static const Unit time_units[] = {
    {"ms", 0.001}, {"s", 1.0}, {"min", 60.0}, {"hr", 3600.0}, {"day", 86400.0},
    {NULL, 0}
};

static const Unit* find_unit(const Unit* table, const char* name) {
    for (int i = 0; table[i].name; i++) {
        if (strcmp(table[i].name, name) == 0) return &table[i];
    }
    return NULL;
}

static bool try_convert(const Unit* table, const char* from, const char* to, f64 val, f64* result) {
    const Unit* uf = find_unit(table, from);
    const Unit* ut = find_unit(table, to);
    if (uf && ut) { *result = val * uf->to_base / ut->to_base; return true; }
    return false;
}

SeaError tool_unit_convert(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Usage: <value> <from_unit> <to_unit>\nExample: 100 km mi");
        return SEA_OK;
    }

    char input[256];
    u32 ilen = args.len < sizeof(input) - 1 ? args.len : (u32)(sizeof(input) - 1);
    memcpy(input, args.data, ilen);
    input[ilen] = '\0';

    f64 val = 0;
    char from[16] = "", to[16] = "";
    if (sscanf(input, "%lf %15s %15s", &val, from, to) != 3) {
        *output = SEA_SLICE_LIT("Error: need <value> <from> <to>");
        return SEA_OK;
    }

    /* Lowercase units */
    for (int i = 0; from[i]; i++) from[i] = (char)tolower((unsigned char)from[i]);
    for (int i = 0; to[i]; i++) to[i] = (char)tolower((unsigned char)to[i]);

    char buf[256];
    int len;
    f64 result;

    /* Temperature special case */
    if ((strcmp(from, "c") == 0 || strcmp(from, "f") == 0 || strcmp(from, "k") == 0) &&
        (strcmp(to, "c") == 0 || strcmp(to, "f") == 0 || strcmp(to, "k") == 0)) {
        /* Convert to Celsius first */
        f64 c;
        if (strcmp(from, "f") == 0) c = (val - 32) * 5.0 / 9.0;
        else if (strcmp(from, "k") == 0) c = val - 273.15;
        else c = val;
        /* Convert from Celsius */
        if (strcmp(to, "f") == 0) result = c * 9.0 / 5.0 + 32;
        else if (strcmp(to, "k") == 0) result = c + 273.15;
        else result = c;
        len = snprintf(buf, sizeof(buf), "%.4g %s = %.4g %s", val, from, result, to);
    } else if (try_convert(length_units, from, to, val, &result) ||
               try_convert(weight_units, from, to, val, &result) ||
               try_convert(data_units, from, to, val, &result) ||
               try_convert(time_units, from, to, val, &result)) {
        len = snprintf(buf, sizeof(buf), "%.4g %s = %.4g %s", val, from, result, to);
    } else {
        len = snprintf(buf, sizeof(buf),
            "Cannot convert '%s' to '%s'\n"
            "Length: km,m,cm,mm,mi,ft,in,yd\n"
            "Weight: kg,g,lb,oz\n"
            "Temp: c,f,k\n"
            "Data: b,kb,mb,gb,tb\n"
            "Time: ms,s,min,hr,day", from, to);
    }

    u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)len);
    if (!dst) return SEA_ERR_ARENA_FULL;
    output->data = dst; output->len = (u32)len;
    return SEA_OK;
}
