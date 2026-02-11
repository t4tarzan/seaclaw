/*
 * tool_shell_exec.c — Execute a shell command (sandboxed)
 *
 * Args: command string
 * Returns: stdout + stderr (truncated to 8KB)
 *
 * Security: Shield validates the command. Dangerous patterns rejected.
 */

#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_shield.h"
#include "seaclaw/sea_log.h"
#include <stdio.h>
#include <string.h>

#define MAX_OUTPUT_SIZE (8 * 1024)

/* Blocklist of dangerous commands */
static bool is_dangerous(const char* cmd) {
    const char* blocklist[] = {
        "rm -rf /", "mkfs", "dd if=", ":(){", "fork bomb",
        "chmod -R 777 /", "shutdown", "reboot", "halt",
        "passwd", "useradd", "userdel", "visudo",
        NULL
    };
    for (int i = 0; blocklist[i]; i++) {
        if (strstr(cmd, blocklist[i])) return true;
    }
    return false;
}

SeaError tool_shell_exec(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Error: no command provided");
        return SEA_OK;
    }

    /* Null-terminate */
    char cmd[2048];
    u32 clen = args.len < sizeof(cmd) - 1 ? args.len : (u32)(sizeof(cmd) - 1);
    memcpy(cmd, args.data, clen);
    cmd[clen] = '\0';

    /* Strip whitespace */
    char* c = cmd;
    while (*c == ' ' || *c == '\t') c++;

    /* Shield: check for injection patterns */
    SeaSlice cmd_slice = { .data = (const u8*)c, .len = (u32)strlen(c) };
    if (sea_shield_detect_injection(cmd_slice)) {
        *output = SEA_SLICE_LIT("Error: command rejected by Shield (injection pattern)");
        return SEA_OK;
    }

    /* Blocklist check */
    if (is_dangerous(c)) {
        *output = SEA_SLICE_LIT("Error: command blocked (dangerous operation)");
        return SEA_OK;
    }

    SEA_LOG_INFO("HANDS", "shell_exec: %s", c);

    /* Redirect stderr to stdout */
    char full_cmd[2200];
    snprintf(full_cmd, sizeof(full_cmd), "%s 2>&1", c);

    FILE* pipe = popen(full_cmd, "r");
    if (!pipe) {
        *output = SEA_SLICE_LIT("Error: failed to execute command");
        return SEA_OK;
    }

    u8* buf = (u8*)sea_arena_alloc(arena, MAX_OUTPUT_SIZE + 64, 1);
    if (!buf) { pclose(pipe); return SEA_ERR_ARENA_FULL; }

    size_t total = 0;
    while (total < MAX_OUTPUT_SIZE) {
        size_t n = fread(buf + total, 1, MAX_OUTPUT_SIZE - total, pipe);
        if (n == 0) break;
        total += n;
    }

    int status = pclose(pipe);

    if (total == MAX_OUTPUT_SIZE) {
        memcpy(buf + total, "\n... (truncated at 8KB)", 23);
        total += 23;
    }

    /* Append exit code */
    char footer[64];
    int flen = snprintf(footer, sizeof(footer), "\n[exit: %d]", status);
    if (total + (size_t)flen < MAX_OUTPUT_SIZE + 64) {
        memcpy(buf + total, footer, (size_t)flen);
        total += (size_t)flen;
    }

    output->data = buf;
    output->len  = (u32)total;
    return SEA_OK;
}
