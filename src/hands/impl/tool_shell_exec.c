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
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

#define MAX_OUTPUT_SIZE (8 * 1024)

/* Safe environment variables - only these are passed to shell commands */
static char* s_safe_env[] = {
    "PATH=/usr/bin:/bin:/usr/local/bin",
    "HOME=/tmp",
    "TERM=xterm",
    "USER=seaclaw",
    "LANG=C.UTF-8",
    NULL
};

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

    /* Create pipe for capturing output */
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        *output = SEA_SLICE_LIT("Error: failed to create pipe");
        return SEA_OK;
    }

    /* Prepare arguments for /bin/sh -c "command" */
    char* argv[] = { "/bin/sh", "-c", c, NULL };

    /* Spawn process with safe environment */
    pid_t pid;
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    
    /* Redirect stdout and stderr to pipe */
    posix_spawn_file_actions_adddup2(&actions, pipefd[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions, pipefd[1], STDERR_FILENO);
    posix_spawn_file_actions_addclose(&actions, pipefd[0]);
    posix_spawn_file_actions_addclose(&actions, pipefd[1]);

    int spawn_result = posix_spawn(&pid, "/bin/sh", &actions, NULL, argv, s_safe_env);
    posix_spawn_file_actions_destroy(&actions);

    if (spawn_result != 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        *output = SEA_SLICE_LIT("Error: failed to spawn process");
        return SEA_OK;
    }

    /* Close write end in parent */
    close(pipefd[1]);

    /* Read output from pipe */
    u8* buf = (u8*)sea_arena_alloc(arena, MAX_OUTPUT_SIZE + 64, 1);
    if (!buf) {
        close(pipefd[0]);
        waitpid(pid, NULL, 0);
        return SEA_ERR_ARENA_FULL;
    }

    size_t total = 0;
    ssize_t n;
    while (total < MAX_OUTPUT_SIZE && (n = read(pipefd[0], buf + total, MAX_OUTPUT_SIZE - total)) > 0) {
        total += (size_t)n;
    }
    close(pipefd[0]);

    /* Wait for process to complete */
    int status;
    waitpid(pid, &status, 0);

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
