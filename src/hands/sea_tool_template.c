/*
 * sea_tool_template.c — Data-Driven Tool Templates
 *
 * HTTP-only tool templates with variable substitution.
 */

#include "seaclaw/sea_tool_template.h"
#include "seaclaw/sea_log.h"
#include "seaclaw/sea_json.h"
#include "seaclaw/sea_http.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Global state */
static SeaToolTemplate s_templates[32];
static u32 s_template_count = 0;

/* ── Variable substitution ────────────────────────────────── */

static void substitute_variables(const char* template_str, const char* args,
                                 char* output, size_t output_size) {
    if (!template_str || !output || output_size == 0) return;
    
    size_t out_pos = 0;
    const char* p = template_str;
    
    while (*p && out_pos < output_size - 1) {
        if (*p == '{' && *(p + 1) == '{') {
            /* Found variable start */
            p += 2;
            const char* var_start = p;
            
            /* Find variable end */
            while (*p && !(*p == '}' && *(p + 1) == '}')) p++;
            
            if (*p == '}') {
                size_t var_len = (size_t)(p - var_start);
                
                /* For simple case, just use the entire args as the value */
                /* More complex parsing would extract specific variables */
                if (args && var_len > 0) {
                    size_t args_len = strlen(args);
                    size_t copy_len = args_len < (output_size - out_pos - 1) ? 
                                     args_len : (output_size - out_pos - 1);
                    memcpy(output + out_pos, args, copy_len);
                    out_pos += copy_len;
                }
                
                p += 2;  /* Skip }} */
            }
        } else {
            output[out_pos++] = *p++;
        }
    }
    
    output[out_pos] = '\0';
}

/* ── Template loading ─────────────────────────────────────── */

SeaError sea_tool_templates_load(const char* json_path) {
    /* For now, just log that templates would be loaded */
    /* Full implementation would parse JSON file */
    SEA_LOG_INFO("TEMPLATE", "Tool templates loading from: %s", json_path);
    
    /* Example: Add a hardcoded weather template for demonstration */
    if (s_template_count < 32) {
        SeaToolTemplate* t = &s_templates[s_template_count++];
        t->name = "weather_api";
        t->description = "Get weather for a city via API";
        t->method = SEA_HTTP_GET;
        t->url_template = "https://api.weatherapi.com/v1/current.json?key=demo&q={{city}}";
        t->body_template = NULL;
        t->headers = "User-Agent: SeaClaw/1.0";
        t->required_vars = NULL;
        
        SEA_LOG_INFO("TEMPLATE", "Registered template: %s", t->name);
    }
    
    return SEA_OK;
}

/* ── Template execution ───────────────────────────────────── */

SeaError sea_tool_template_exec(const char* tool_name, const char* args,
                                SeaArena* arena, SeaSlice* output) {
    if (!tool_name || !arena || !output) return SEA_ERR_INVALID_INPUT;
    
    /* Find template */
    const SeaToolTemplate* tmpl = NULL;
    for (u32 i = 0; i < s_template_count; i++) {
        if (strcmp(s_templates[i].name, tool_name) == 0) {
            tmpl = &s_templates[i];
            break;
        }
    }
    
    if (!tmpl) {
        *output = SEA_SLICE_LIT("Error: Template not found");
        return SEA_ERR_NOT_FOUND;
    }
    
    /* Substitute variables in URL */
    char url[2048];
    substitute_variables(tmpl->url_template, args, url, sizeof(url));
    
    SEA_LOG_INFO("TEMPLATE", "Executing %s: %s", tool_name, url);
    
    /* Execute HTTP request */
    SeaHttpResponse resp;
    
    SeaError err = sea_http_get(url, arena, &resp);
    if (err != SEA_OK) {
        *output = SEA_SLICE_LIT("Error: HTTP request failed");
        return err;
    }
    
    if (resp.status_code != 200) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Error: HTTP %d", resp.status_code);
        *output = (SeaSlice){ .data = (const u8*)sea_arena_push_cstr(arena, error_msg).data,
                             .len = (u32)strlen(error_msg) };
        return SEA_ERR_IO;
    }
    
    *output = resp.body;
    return SEA_OK;
}

/* ── Template queries ─────────────────────────────────────── */

u32 sea_tool_templates_count(void) {
    return s_template_count;
}

const SeaToolTemplate* sea_tool_template_get(const char* name) {
    if (!name) return NULL;
    
    for (u32 i = 0; i < s_template_count; i++) {
        if (strcmp(s_templates[i].name, name) == 0) {
            return &s_templates[i];
        }
    }
    
    return NULL;
}
