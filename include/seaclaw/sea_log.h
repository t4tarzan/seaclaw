/*
 * sea_log.h — Structured logging with [TAG] format
 *
 * Every subsystem logs with a bracketed tag and millisecond timestamps.
 * Matches the TUI status line format: T+0ms [SENSES] Parsing...
 */

#ifndef SEA_LOG_H
#define SEA_LOG_H

#include "sea_types.h"

typedef enum {
    SEA_LOG_DEBUG = 0,
    SEA_LOG_INFO,
    SEA_LOG_WARN,
    SEA_LOG_ERROR,
} SeaLogLevel;

/* Initialize logging. Call once at startup. */
void sea_log_init(SeaLogLevel min_level);

/* Get elapsed milliseconds since sea_log_init() */
u64 sea_log_elapsed_ms(void);

/* Core log function — prefer the macros below */
void sea_log(SeaLogLevel level, const char* tag, const char* fmt, ...)
    __attribute__((format(printf, 3, 4)));

/* Convenience macros — C11 compatible (fmt folded into __VA_ARGS__) */
#define SEA_LOG_DEBUG(tag, ...) sea_log(SEA_LOG_DEBUG, tag, __VA_ARGS__)
#define SEA_LOG_INFO(tag, ...)  sea_log(SEA_LOG_INFO,  tag, __VA_ARGS__)
#define SEA_LOG_WARN(tag, ...)  sea_log(SEA_LOG_WARN,  tag, __VA_ARGS__)
#define SEA_LOG_ERROR(tag, ...) sea_log(SEA_LOG_ERROR, tag, __VA_ARGS__)

#endif /* SEA_LOG_H */
