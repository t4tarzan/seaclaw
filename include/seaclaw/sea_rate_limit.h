/*
 * sea_rate_limit.h — Tool Execution Rate Limiting
 *
 * Per-tool, per-hour rate limiting with SQLite backend.
 * Prevents abuse and controls costs.
 */

#ifndef SEA_RATE_LIMIT_H
#define SEA_RATE_LIMIT_H

#include "sea_types.h"

/* Rate limit configuration */
typedef struct {
    const char* tool_name;
    u32         max_per_hour;     /* Max executions per hour */
    u32         max_per_day;      /* Max executions per day (0 = unlimited) */
} SeaRateLimitConfig;

/* Initialize rate limiting system */
SeaError sea_rate_limit_init(const char* db_path);

/* Check if tool execution is allowed */
bool sea_rate_limit_check(const char* tool_name);

/* Record a tool execution */
SeaError sea_rate_limit_record(const char* tool_name);

/* Get current usage for a tool */
u32 sea_rate_limit_get_count(const char* tool_name, u32 window_hours);

/* Clean up old rate limit records */
void sea_rate_limit_cleanup(void);

/* Set custom rate limit for a tool */
void sea_rate_limit_set(const char* tool_name, u32 max_per_hour, u32 max_per_day);

/* Get rate limit info for a tool */
bool sea_rate_limit_get_info(const char* tool_name, u32* current_hour, u32* max_hour, u32* current_day, u32* max_day);

#endif /* SEA_RATE_LIMIT_H */
