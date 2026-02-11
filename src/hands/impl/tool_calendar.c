/*
 * tool_calendar.c — Calendar and date utilities
 *
 * Tool ID:    39
 * Category:   Utility
 * Args:       [month year] | [weekday YYYY-MM-DD] | [diff YYYY-MM-DD YYYY-MM-DD]
 * Returns:    Calendar view, day of week, or date difference
 *
 * Examples:
 *   /exec calendar                    → current month calendar
 *   /exec calendar 3 2026             → March 2026 calendar
 *   /exec calendar weekday 2026-02-11 → "Wednesday"
 *   /exec calendar diff 2026-01-01 2026-12-31 → "364 days"
 *
 * Security: Input validated by standard tool pipeline.
 */

#include "seaclaw/sea_tools.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char* dow_short[] = {"Su","Mo","Tu","We","Th","Fr","Sa"};
static const char* month_names[] = {"","January","February","March","April","May","June",
                                     "July","August","September","October","November","December"};

static int days_in_month(int m, int y) {
    int d[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
    if (m == 2 && ((y%4==0 && y%100!=0) || y%400==0)) return 29;
    return d[m];
}

static int day_of_week(int y, int m, int d) {
    /* Tomohiko Sakamoto's algorithm */
    static int t[] = {0,3,2,5,0,3,5,1,4,6,2,4};
    if (m < 3) y--;
    return (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7;
}

SeaError tool_calendar(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    char input[128] = "";
    if (args.len > 0) {
        u32 ilen = args.len < sizeof(input) - 1 ? args.len : (u32)(sizeof(input) - 1);
        memcpy(input, args.data, ilen);
        input[ilen] = '\0';
    }

    char* p = input;
    while (*p == ' ') p++;

    char buf[2048];
    int pos = 0;

    if (strncmp(p, "weekday", 7) == 0) {
        p += 7; while (*p == ' ') p++;
        int y, m, d;
        if (sscanf(p, "%d-%d-%d", &y, &m, &d) != 3) {
            *output = SEA_SLICE_LIT("Error: use format YYYY-MM-DD");
            return SEA_OK;
        }
        int w = day_of_week(y, m, d);
        static const char* full_dow[] = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
        pos = snprintf(buf, sizeof(buf), "%04d-%02d-%02d is a %s", y, m, d, full_dow[w]);
    } else if (strncmp(p, "diff", 4) == 0) {
        p += 4; while (*p == ' ') p++;
        int y1, m1, d1, y2, m2, d2;
        if (sscanf(p, "%d-%d-%d %d-%d-%d", &y1, &m1, &d1, &y2, &m2, &d2) != 6) {
            *output = SEA_SLICE_LIT("Error: use format YYYY-MM-DD YYYY-MM-DD");
            return SEA_OK;
        }
        struct tm t1 = {0}, t2 = {0};
        t1.tm_year = y1 - 1900; t1.tm_mon = m1 - 1; t1.tm_mday = d1;
        t2.tm_year = y2 - 1900; t2.tm_mon = m2 - 1; t2.tm_mday = d2;
        time_t tt1 = mktime(&t1), tt2 = mktime(&t2);
        long diff = (long)difftime(tt2, tt1) / 86400;
        pos = snprintf(buf, sizeof(buf), "%04d-%02d-%02d to %04d-%02d-%02d: %ld days",
                       y1, m1, d1, y2, m2, d2, diff);
    } else {
        /* Calendar view */
        int month, year;
        time_t now = time(NULL);
        struct tm* tm = localtime(&now);

        if (sscanf(p, "%d %d", &month, &year) == 2) {
            if (month < 1 || month > 12) month = tm->tm_mon + 1;
            if (year < 1970 || year > 2100) year = tm->tm_year + 1900;
        } else {
            month = tm->tm_mon + 1;
            year = tm->tm_year + 1900;
        }

        pos = snprintf(buf, sizeof(buf), "    %s %d\n", month_names[month], year);
        for (int i = 0; i < 7; i++)
            pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, " %s", dow_short[i]);
        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "\n");

        int first = day_of_week(year, month, 1);
        int dim = days_in_month(month, year);

        for (int i = 0; i < first; i++)
            pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "   ");

        for (int d = 1; d <= dim; d++) {
            if (d == tm->tm_mday && month == tm->tm_mon + 1 && year == tm->tm_year + 1900)
                pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "[%2d]", d);
            else
                pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, " %2d ", d);
            if ((first + d) % 7 == 0) pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "\n");
        }
    }

    u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)pos);
    if (!dst) return SEA_ERR_ARENA_FULL;
    output->data = dst; output->len = (u32)pos;
    return SEA_OK;
}
