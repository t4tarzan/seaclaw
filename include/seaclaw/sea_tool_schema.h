/*
 * sea_tool_schema.h — Tool Argument Validation
 *
 * Lightweight argument validation for tools.
 * No external JSON schema library - simple built-in validation.
 */

#ifndef SEA_TOOL_SCHEMA_H
#define SEA_TOOL_SCHEMA_H

#include "sea_types.h"

/* Argument types */
typedef enum {
    SEA_ARG_STRING = 0,
    SEA_ARG_INTEGER,
    SEA_ARG_BOOLEAN,
    SEA_ARG_PATH,
    SEA_ARG_URL,
    SEA_ARG_ENUM,
} SeaArgType;

/* Argument definition */
typedef struct {
    const char*  name;
    SeaArgType   type;
    bool         required;
    const char*  description;
    const char** enum_values;  /* For ENUM type, NULL-terminated array */
} SeaToolArg;

/* Tool schema */
typedef struct {
    const char*      tool_name;
    const SeaToolArg* args;
    u32              arg_count;
} SeaToolSchema;

/* Validate tool arguments against schema */
bool sea_tool_validate_args(const char* tool_name, const char* args_str);

/* Register a tool schema */
void sea_tool_register_schema(const SeaToolSchema* schema);

/* Get validation error message */
const char* sea_tool_get_validation_error(void);

#endif /* SEA_TOOL_SCHEMA_H */
