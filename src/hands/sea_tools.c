/*
 * sea_tools.c — Static tool registry implementation
 *
 * All tools are compiled in. AI cannot invent new tools at runtime.
 */

#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_log.h"
#include <stdio.h>
#include <string.h>

/* ── Forward declarations for tool implementations ────────── */

extern SeaError tool_echo(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_system_status(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_file_read(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_file_write(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_shell_exec(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_web_fetch(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_task_manage(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_db_query(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_exa_search(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_text_summarize(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_text_transform(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_json_format(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_hash_compute(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_env_get(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_dir_list(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_file_info(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_process_list(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_dns_lookup(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_timestamp(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_math_eval(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_uuid_gen(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_random_gen(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_url_parse(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_encode_decode(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_regex_match(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_csv_parse(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_diff_text(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_grep_text(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_wc(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_head_tail(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_sort_text(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_net_info(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_cron_parse(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_disk_usage(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_syslog_read(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_json_query(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_http_request(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_string_replace(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_calendar(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_checksum_file(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_file_search(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_uptime(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_ip_info(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_whois_lookup(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_ssl_check(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_json_to_csv(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_weather(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_unit_convert(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_password_gen(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_count_lines(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_edit_file(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_cron_manage(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_memory_manage(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_web_search(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_spawn(SeaSlice args, SeaArena* arena, SeaSlice* output);
extern SeaError tool_message(SeaSlice args, SeaArena* arena, SeaSlice* output);

/* ── The Static Registry ──────────────────────────────────── */

static const SeaTool s_registry[] = {
    { 1, "echo",          "Echo text back",                              tool_echo },
    { 2, "system_status", "Report memory usage and uptime",              tool_system_status },
    { 3, "file_read",     "Read a file. Args: file_path",                tool_file_read },
    { 4, "file_write",    "Write a file. Args: path|content",            tool_file_write },
    { 5, "shell_exec",    "Run a shell command. Args: command",          tool_shell_exec },
    { 6, "web_fetch",     "Fetch a URL. Args: url",                      tool_web_fetch },
    { 7, "task_manage",   "Manage tasks. Args: list|create|title|desc|done|id", tool_task_manage },
    { 8, "db_query",      "Query database (read-only). Args: SELECT SQL",        tool_db_query },
    { 9, "exa_search",    "Web search via Exa. Args: search query",              tool_exa_search },
    {10, "text_summarize","Analyze text stats. Args: text",                       tool_text_summarize },
    {11, "text_transform","Transform text. Args: <upper|lower|reverse|base64enc|base64dec> text", tool_text_transform },
    {12, "json_format",   "Pretty-print/validate JSON. Args: json string",        tool_json_format },
    {13, "hash_compute",  "Hash text. Args: <crc32|djb2|fnv1a> text",             tool_hash_compute },
    {14, "env_get",       "Get env variable (whitelisted). Args: VAR_NAME",       tool_env_get },
    {15, "dir_list",      "List directory contents. Args: path",                  tool_dir_list },
    {16, "file_info",     "File metadata. Args: file_path",                       tool_file_info },
    {17, "process_list",  "List processes. Args: optional filter",                tool_process_list },
    {18, "dns_lookup",    "DNS resolve hostname. Args: hostname",                 tool_dns_lookup },
    {19, "timestamp",     "Current time. Args: optional unix|iso|utc|date",       tool_timestamp },
    {20, "math_eval",     "Evaluate math. Args: expression (e.g. 2+3*4)",         tool_math_eval },
    {21, "uuid_gen",      "Generate UUID v4. Args: optional count (1-10)",         tool_uuid_gen },
    {22, "random_gen",    "Random values. Args: <number|string|hex|coin|dice>",   tool_random_gen },
    {23, "url_parse",     "Parse URL components. Args: url",                      tool_url_parse },
    {24, "encode_decode", "Encode/decode. Args: <urlencode|urldecode|htmlencode|htmldecode> text", tool_encode_decode },
    {25, "regex_match",   "Regex match. Args: <pattern> <text>",                  tool_regex_match },
    {26, "csv_parse",     "Parse CSV. Args: <headers|count|col_num> <csv>",        tool_csv_parse },
    {27, "diff_text",     "Compare texts. Args: <text1>|||<text2>",                tool_diff_text },
    {28, "grep_text",     "Search text/file. Args: <pattern> <text_or_path>",      tool_grep_text },
    {29, "wc",            "Word count. Args: <filepath_or_text>",                  tool_wc },
    {30, "head_tail",     "First/last lines. Args: <head|tail> [N] <path_or_text>",tool_head_tail },
    {31, "sort_text",     "Sort lines. Args: [-r] [-n] [-u] <text>",               tool_sort_text },
    {32, "net_info",      "Network info. Args: <interfaces|ip|ping|ports>",        tool_net_info },
    {33, "cron_parse",    "Explain cron. Args: <min hour dom mon dow>",            tool_cron_parse },
    {34, "disk_usage",    "Disk usage. Args: [path]",                              tool_disk_usage },
    {35, "syslog_read",   "Read system logs. Args: [lines] [filter]",              tool_syslog_read },
    {36, "json_query",    "Query JSON by path. Args: <key.path> <json>",           tool_json_query },
    {37, "http_request",  "HTTP request. Args: <GET|POST|HEAD> <url> [body]",      tool_http_request },
    {38, "string_replace","Find/replace. Args: <find>|||<replace>|||<text>",        tool_string_replace },
    {39, "calendar",      "Calendar/dates. Args: [month year|weekday|diff]",        tool_calendar },
    {40, "checksum_file", "File checksum. Args: <filepath>",                       tool_checksum_file },
    {41, "file_search",   "Find files by name. Args: <pattern> [directory]",        tool_file_search },
    {42, "uptime",        "System uptime and load. Args: (none)",                  tool_uptime },
    {43, "ip_info",       "IP geolocation. Args: [ip_address]",                    tool_ip_info },
    {44, "whois_lookup",  "Domain WHOIS. Args: <domain>",                          tool_whois_lookup },
    {45, "ssl_check",     "SSL certificate info. Args: <domain>",                  tool_ssl_check },
    {46, "json_to_csv",   "JSON array to CSV. Args: <json_array>",                 tool_json_to_csv },
    {47, "weather",       "Current weather. Args: <city>",                          tool_weather },
    {48, "unit_convert",  "Unit conversion. Args: <val> <from> <to>",              tool_unit_convert },
    {49, "password_gen",  "Generate password. Args: [length] [-n no symbols]",      tool_password_gen },
    {50, "count_lines",   "Count lines of code. Args: [dir] [ext]",                tool_count_lines },
    {51, "edit_file",     "Edit file. Args: <path>|||<find>|||<replace>",           tool_edit_file },
    {52, "cron_manage",   "Manage cron. Args: list|add|remove|pause|resume",       tool_cron_manage },
    {53, "memory_manage", "Memory. Args: read|write|append|daily|bootstrap",       tool_memory_manage },
    {54, "web_search",    "Brave web search. Args: <query>",                       tool_web_search },
    {55, "spawn",         "Spawn sub-agent. Args: <task description>",              tool_spawn },
    {56, "message",       "Send message. Args: <channel:chat_id> <text>",           tool_message },
};

static const u32 s_registry_count = sizeof(s_registry) / sizeof(s_registry[0]);

/* ── API ──────────────────────────────────────────────────── */

void sea_tools_init(void) {
    SEA_LOG_INFO("HANDS", "Tool registry loaded: %u tools", s_registry_count);
}

u32 sea_tools_count(void) {
    return s_registry_count;
}

const SeaTool* sea_tool_by_name(const char* name) {
    for (u32 i = 0; i < s_registry_count; i++) {
        if (strcmp(s_registry[i].name, name) == 0) {
            return &s_registry[i];
        }
    }
    return NULL;
}

const SeaTool* sea_tool_by_id(u32 id) {
    for (u32 i = 0; i < s_registry_count; i++) {
        if (s_registry[i].id == id) {
            return &s_registry[i];
        }
    }
    return NULL;
}

SeaError sea_tool_exec(const char* name, SeaSlice args, SeaArena* arena, SeaSlice* output) {
    const SeaTool* tool = sea_tool_by_name(name);
    if (!tool) return SEA_ERR_TOOL_NOT_FOUND;

    SEA_LOG_INFO("HANDS", "Executing tool: %s", tool->name);
    SeaError err = tool->func(args, arena, output);

    if (err != SEA_OK) {
        SEA_LOG_ERROR("HANDS", "Tool '%s' failed: %s", tool->name, sea_error_str(err));
    }
    return err;
}

void sea_tools_list(void) {
    printf("  %-4s %-20s %s\n", "ID", "Name", "Description");
    printf("  %-4s %-20s %s\n", "──", "────────────────────", "───────────────────────────");
    for (u32 i = 0; i < s_registry_count; i++) {
        printf("  %-4u %-20s %s\n",
               s_registry[i].id,
               s_registry[i].name,
               s_registry[i].description);
    }
}
