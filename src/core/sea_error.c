/*
 * sea_error.c — Structured Error Types Implementation
 *
 * Enhanced error handling with structured messages.
 */

#include "seaclaw/sea_error.h"
#include "seaclaw/sea_log.h"
#include <stdio.h>
#include <string.h>

/* ── Format error result ──────────────────────────────────── */

const char* sea_error_format(SeaErrorResult result, char* buf, size_t buf_size) {
    if (!buf || buf_size == 0) return "";
    
    if (result.code == SEA_OK) {
        snprintf(buf, buf_size, "Success");
        return buf;
    }
    
    if (result.context && result.line > 0) {
        snprintf(buf, buf_size, "[%s:%d] %s: %s",
                result.context, result.line,
                sea_error_str(result.code),
                result.message);
    } else if (result.context) {
        snprintf(buf, buf_size, "[%s] %s: %s",
                result.context,
                sea_error_str(result.code),
                result.message);
    } else {
        snprintf(buf, buf_size, "%s: %s",
                sea_error_str(result.code),
                result.message);
    }
    
    return buf;
}

/* ── Log error result ─────────────────────────────────────── */

void sea_error_log(SeaErrorResult result) {
    if (result.code == SEA_OK) return;
    
    char formatted[512];
    sea_error_format(result, formatted, sizeof(formatted));
    
    /* Determine log level based on error severity */
    if (sea_is_security_error(result.code)) {
        SEA_LOG_WARN("ERROR", "%s", formatted);
    } else if (sea_is_memory_error(result.code)) {
        SEA_LOG_ERROR("ERROR", "%s", formatted);
    } else {
        SEA_LOG_INFO("ERROR", "%s", formatted);
    }
}
