/*
 * sea_error.h — Structured Error Types
 *
 * Enhanced error handling with structured messages and context.
 * Extends existing SeaError enum with detailed error information.
 */

#ifndef SEA_ERROR_H
#define SEA_ERROR_H

#include "sea_types.h"

/* Structured error result */
typedef struct {
    SeaError    code;           /* Error code from sea_types.h */
    char        message[256];   /* Human-readable error message */
    const char* context;        /* Additional context (function name, etc.) */
    i32         line;           /* Line number where error occurred */
} SeaErrorResult;

/* Create a structured error result */
#define SEA_ERROR_RESULT(err_code, msg) \
    ((SeaErrorResult){ \
        .code = (err_code), \
        .message = msg, \
        .context = __func__, \
        .line = __LINE__ \
    })

/* Create a success result */
#define SEA_SUCCESS_RESULT() \
    ((SeaErrorResult){ \
        .code = SEA_OK, \
        .message = "Success", \
        .context = NULL, \
        .line = 0 \
    })

/* Check if result is an error */
static inline bool sea_is_error(SeaErrorResult result) {
    return result.code != SEA_OK;
}

/* Get formatted error string */
const char* sea_error_format(SeaErrorResult result, char* buf, size_t buf_size);

/* Log an error result */
void sea_error_log(SeaErrorResult result);

/* Error category helpers */
static inline bool sea_is_memory_error(SeaError err) {
    return err == SEA_ERR_OOM || err == SEA_ERR_ARENA_FULL;
}

static inline bool sea_is_io_error(SeaError err) {
    return err == SEA_ERR_IO || err == SEA_ERR_EOF || 
           err == SEA_ERR_TIMEOUT || err == SEA_ERR_CONNECT;
}

static inline bool sea_is_parse_error(SeaError err) {
    return err == SEA_ERR_PARSE || err == SEA_ERR_INVALID_JSON || 
           err == SEA_ERR_UNEXPECTED_TOKEN;
}

static inline bool sea_is_security_error(SeaError err) {
    return err == SEA_ERR_INVALID_INPUT || err == SEA_ERR_GRAMMAR_REJECT || 
           err == SEA_ERR_SANDBOX_FAIL || err == SEA_ERR_PERMISSION;
}

#endif /* SEA_ERROR_H */
