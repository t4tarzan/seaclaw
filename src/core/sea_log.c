/*
 * sea_log.c — Structured logging implementation
 *
 * Format: T+<ms> [<TAG>] <message>
 */

#include "seaclaw/sea_log.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

static SeaLogLevel s_min_level = SEA_LOG_INFO;
static struct timespec s_start_time;

void sea_log_init(SeaLogLevel min_level) {
    s_min_level = min_level;
    clock_gettime(CLOCK_MONOTONIC, &s_start_time);
}

u64 sea_log_elapsed_ms(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    u64 sec  = (u64)(now.tv_sec  - s_start_time.tv_sec);
    i64 nsec = now.tv_nsec - s_start_time.tv_nsec;
    if (nsec < 0) {
        sec--;
        nsec += 1000000000L;
    }
    return sec * 1000 + (u64)nsec / 1000000;
}

static const char* level_str(SeaLogLevel level) {
    switch (level) {
        case SEA_LOG_DEBUG: return "DBG";
        case SEA_LOG_INFO:  return "INF";
        case SEA_LOG_WARN:  return "WRN";
        case SEA_LOG_ERROR: return "ERR";
        default:            return "???";
    }
}

void sea_log(SeaLogLevel level, const char* tag, const char* fmt, ...) {
    if (level < s_min_level) return;

    u64 ms = sea_log_elapsed_ms();

    FILE* out = (level >= SEA_LOG_WARN) ? stderr : stdout;

    fprintf(out, "T+%lums [%s] %s: ", (unsigned long)ms, tag, level_str(level));

    va_list args;
    va_start(args, fmt);
    vfprintf(out, fmt, args);
    va_end(args);

    fputc('\n', out);
    fflush(out);
}
