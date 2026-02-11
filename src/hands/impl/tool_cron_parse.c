/*
 * tool_cron_parse.c — Parse and explain cron expressions
 *
 * Tool ID:    33
 * Category:   System / Utility
 * Args:       <cron_expression>
 * Returns:    Human-readable explanation of the schedule
 *
 * Format: minute hour day_of_month month day_of_week
 *
 * Examples:
 *   /exec cron_parse "0 9 * * 1-5"       -> "At 09:00, Monday through Friday"
 *   /exec cron_parse "star/15 * * * *"    -> "Every 15 minutes"
 *   /exec cron_parse "0 0 1 * *"          -> "At midnight on the 1st of every month"
 *
 * Security: Input validated by standard tool pipeline.
 */

#include "seaclaw/sea_tools.h"
#include <stdio.h>
#include <string.h>

static int str_to_int(const char* s) {
    int v = 0, neg = 0;
    if (*s == '-') { neg = 1; s++; }
    while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
    return neg ? -v : v;
}

static const char* dow_names[] = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
static const char* mon_names[] = {"","January","February","March","April","May","June",
                                   "July","August","September","October","November","December"};

static void explain_field(const char* field, const char* unit, char* out, u32 omax) {
    if (strcmp(field, "*") == 0) {
        snprintf(out, omax, "every %s", unit);
    } else if (field[0] == '*' && field[1] == '/') {
        snprintf(out, omax, "every %s %ss", field + 2, unit);
    } else if (strchr(field, '-')) {
        snprintf(out, omax, "%s %s through %s", unit, field, strchr(field, '-') + 1);
    } else if (strchr(field, ',')) {
        snprintf(out, omax, "%ss %s", unit, field);
    } else {
        snprintf(out, omax, "%s %s", unit, field);
    }
}

SeaError tool_cron_parse(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Usage: <minute> <hour> <day> <month> <dow>\nExample: 0 9 * * 1-5");
        return SEA_OK;
    }

    char input[256];
    u32 ilen = args.len < sizeof(input) - 1 ? args.len : (u32)(sizeof(input) - 1);
    memcpy(input, args.data, ilen);
    input[ilen] = '\0';

    char minute[32] = "*", hour[32] = "*", dom[32] = "*", month[32] = "*", dow[32] = "*";
    sscanf(input, "%31s %31s %31s %31s %31s", minute, hour, dom, month, dow);

    char buf[1024];
    int pos = snprintf(buf, sizeof(buf), "Cron: %s %s %s %s %s\n\nSchedule: ", minute, hour, dom, month, dow);

    /* Build human-readable description */
    if (strcmp(minute, "0") == 0 && strcmp(hour, "*") != 0 && strcmp(dom, "*") == 0 &&
        strcmp(month, "*") == 0 && strcmp(dow, "*") == 0) {
        int h = str_to_int(hour);
        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "At %02d:00 every day", h);
    } else if (minute[0] == '*' && minute[1] == '/') {
        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "Every %s minutes", minute + 2);
    } else if (strcmp(minute, "*") == 0 && strcmp(hour, "*") == 0) {
        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "Every minute");
    } else {
        int m = str_to_int(minute), h = str_to_int(hour);
        if (strcmp(hour, "*") != 0 && strcmp(minute, "*") != 0)
            pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "At %02d:%02d", h, m);
        else {
            char desc[64];
            explain_field(minute, "minute", desc, sizeof(desc));
            pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "%s", desc);
        }
    }

    if (strcmp(dow, "*") != 0) {
        if (strcmp(dow, "1-5") == 0) {
            pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, ", Monday through Friday");
        } else if (strcmp(dow, "0,6") == 0 || strcmp(dow, "6,0") == 0) {
            pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, ", weekends only");
        } else {
            int d = str_to_int(dow);
            if (d >= 0 && d <= 6)
                pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, ", on %s", dow_names[d]);
            else
                pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, ", day-of-week %s", dow);
        }
    }

    if (strcmp(dom, "*") != 0) {
        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, ", on day %s of the month", dom);
    }

    if (strcmp(month, "*") != 0) {
        int mo = str_to_int(month);
        if (mo >= 1 && mo <= 12)
            pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, ", in %s", mon_names[mo]);
        else
            pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, ", month %s", month);
    }

    pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "\n\nFields:\n"
        "  Minute:       %s\n  Hour:         %s\n  Day of Month: %s\n"
        "  Month:        %s\n  Day of Week:  %s",
        minute, hour, dom, month, dow);

    u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)pos);
    if (!dst) return SEA_ERR_ARENA_FULL;
    output->data = dst; output->len = (u32)pos;
    return SEA_OK;
}
