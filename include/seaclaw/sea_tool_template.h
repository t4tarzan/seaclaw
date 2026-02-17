/*
 * sea_tool_template.h — Data-Driven Tool Templates
 *
 * Define custom HTTP-based tools via JSON templates.
 * No code execution - HTTP requests only.
 */

#ifndef SEA_TOOL_TEMPLATE_H
#define SEA_TOOL_TEMPLATE_H

#include "sea_types.h"
#include "sea_arena.h"

/* HTTP method */
typedef enum {
    SEA_HTTP_GET = 0,
    SEA_HTTP_POST,
    SEA_HTTP_PUT,
    SEA_HTTP_DELETE,
} SeaHttpMethod;

/* Tool template definition */
typedef struct {
    const char*    name;
    const char*    description;
    SeaHttpMethod  method;
    const char*    url_template;     /* URL with {{variable}} placeholders */
    const char*    body_template;    /* Body with {{variable}} placeholders */
    const char*    headers;          /* Newline-separated headers */
    const char**   required_vars;    /* NULL-terminated array of required variables */
} SeaToolTemplate;

/* Load tool templates from JSON file */
SeaError sea_tool_templates_load(const char* json_path);

/* Execute a template-based tool */
SeaError sea_tool_template_exec(const char* tool_name, const char* args, 
                                SeaArena* arena, SeaSlice* output);

/* Get template count */
u32 sea_tool_templates_count(void);

/* Get template by name */
const SeaToolTemplate* sea_tool_template_get(const char* name);

#endif /* SEA_TOOL_TEMPLATE_H */
