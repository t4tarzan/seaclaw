/*
 * sea_tool_schema.c — Tool Argument Validation
 *
 * Lightweight validation without external dependencies.
 */

#include "seaclaw/sea_tool_schema.h"
#include "seaclaw/sea_log.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

/* Global state */
static SeaToolSchema s_schemas[64];
static u32 s_schema_count = 0;
static char s_validation_error[256] = {0};

/* ── Schema registration ──────────────────────────────────── */

void sea_tool_register_schema(const SeaToolSchema* schema) {
    if (!schema || s_schema_count >= 64) return;
    
    s_schemas[s_schema_count++] = *schema;
    SEA_LOG_DEBUG("SCHEMA", "Registered schema for tool: %s (%u args)", 
                  schema->tool_name, schema->arg_count);
}

/* ── Validation helpers ───────────────────────────────────── */

static bool is_valid_integer(const char* str) {
    if (!str || !*str) return false;
    if (*str == '-' || *str == '+') str++;
    if (!*str) return false;
    while (*str) {
        if (!isdigit(*str)) return false;
        str++;
    }
    return true;
}

static bool is_valid_boolean(const char* str) {
    if (!str) return false;
    return (strcmp(str, "true") == 0 || strcmp(str, "false") == 0 ||
            strcmp(str, "1") == 0 || strcmp(str, "0") == 0);
}

static bool is_valid_path(const char* str) {
    if (!str || !*str) return false;
    /* Basic path validation - no null bytes, reasonable length */
    size_t len = strlen(str);
    if (len > 4096) return false;
    for (size_t i = 0; i < len; i++) {
        if (str[i] == '\0') return false;
    }
    return true;
}

static bool is_valid_url(const char* str) {
    if (!str || !*str) return false;
    /* Basic URL validation */
    return (strncmp(str, "http://", 7) == 0 || 
            strncmp(str, "https://", 8) == 0);
}

static bool is_valid_enum(const char* str, const char** enum_values) {
    if (!str || !enum_values) return false;
    for (int i = 0; enum_values[i]; i++) {
        if (strcmp(str, enum_values[i]) == 0) return true;
    }
    return false;
}

/* ── Argument validation ──────────────────────────────────── */

bool sea_tool_validate_args(const char* tool_name, const char* args_str) {
    if (!tool_name) {
        snprintf(s_validation_error, sizeof(s_validation_error), "Tool name is NULL");
        return false;
    }

    /* Find schema for this tool */
    const SeaToolSchema* schema = NULL;
    for (u32 i = 0; i < s_schema_count; i++) {
        if (strcmp(s_schemas[i].tool_name, tool_name) == 0) {
            schema = &s_schemas[i];
            break;
        }
    }

    /* No schema registered = allow (backward compatibility) */
    if (!schema) {
        SEA_LOG_DEBUG("SCHEMA", "No schema for tool %s, allowing", tool_name);
        return true;
    }

    /* If no args provided but required args exist */
    if (!args_str || !*args_str) {
        for (u32 i = 0; i < schema->arg_count; i++) {
            if (schema->args[i].required) {
                snprintf(s_validation_error, sizeof(s_validation_error),
                        "Missing required argument: %s", schema->args[i].name);
                return false;
            }
        }
        return true;
    }

    /* Simple validation: check if args string is reasonable */
    size_t args_len = strlen(args_str);
    if (args_len > 65536) {
        snprintf(s_validation_error, sizeof(s_validation_error),
                "Arguments too long: %zu bytes", args_len);
        return false;
    }

    /* For tools with single argument, validate type */
    if (schema->arg_count == 1) {
        const SeaToolArg* arg = &schema->args[0];
        bool valid = false;

        switch (arg->type) {
            case SEA_ARG_STRING:
                valid = true;  /* Any string is valid */
                break;
            case SEA_ARG_INTEGER:
                valid = is_valid_integer(args_str);
                if (!valid) {
                    snprintf(s_validation_error, sizeof(s_validation_error),
                            "Argument '%s' must be an integer", arg->name);
                }
                break;
            case SEA_ARG_BOOLEAN:
                valid = is_valid_boolean(args_str);
                if (!valid) {
                    snprintf(s_validation_error, sizeof(s_validation_error),
                            "Argument '%s' must be true/false", arg->name);
                }
                break;
            case SEA_ARG_PATH:
                valid = is_valid_path(args_str);
                if (!valid) {
                    snprintf(s_validation_error, sizeof(s_validation_error),
                            "Argument '%s' is not a valid path", arg->name);
                }
                break;
            case SEA_ARG_URL:
                valid = is_valid_url(args_str);
                if (!valid) {
                    snprintf(s_validation_error, sizeof(s_validation_error),
                            "Argument '%s' must be a valid URL (http:// or https://)", arg->name);
                }
                break;
            case SEA_ARG_ENUM:
                valid = is_valid_enum(args_str, arg->enum_values);
                if (!valid) {
                    snprintf(s_validation_error, sizeof(s_validation_error),
                            "Argument '%s' must be one of the allowed values", arg->name);
                }
                break;
        }

        if (!valid) {
            SEA_LOG_WARN("SCHEMA", "Validation failed for %s: %s", tool_name, s_validation_error);
        }
        return valid;
    }

    /* For multi-argument tools, just check basic sanity */
    /* Full parsing would require more complex logic */
    return true;
}

/* ── Get validation error ─────────────────────────────────── */

const char* sea_tool_get_validation_error(void) {
    return s_validation_error[0] ? s_validation_error : "Unknown validation error";
}
