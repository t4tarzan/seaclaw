/*
 * tool_git.c — Git operations for SeaClaw agents
 *
 * Six tools registered here:
 *   git_clone    Args: <url> [branch] [dest_dir]
 *   git_status   Args: <repo_path>
 *   git_pull     Args: <repo_path>
 *   git_log      Args: <repo_path> [count]
 *   git_diff     Args: <repo_path>
 *   git_checkout Args: <repo_path> <branch>
 *
 * Security:
 *   - URL must start with https:// or git@  (no file:// or custom protocols)
 *   - repo_path must start with /workspace or /userdata (pod-owned dirs only)
 *   - All args are shell-escaped before being passed to popen()
 *   - Output truncated at 16 KB
 */

#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define GIT_MAX_OUTPUT (16 * 1024)
#define GIT_CMD_MAX    1024

/* ── Helpers ────────────────────────────────────────────────── */

static bool valid_git_url(const char* url) {
    return (strncmp(url, "https://", 8) == 0 ||
            strncmp(url, "http://",  7) == 0 ||
            strncmp(url, "git@",     4) == 0);
}

static bool valid_repo_path(const char* path) {
    return (strncmp(path, "/workspace", 10) == 0 ||
            strncmp(path, "/userdata",   9) == 0);
}

/* Escape a string for single-quote shell usage: replace ' with '\'' */
static void shell_escape(const char* in, char* out, size_t out_sz) {
    size_t j = 0;
    out[j++] = '\'';
    for (size_t i = 0; in[i] && j + 5 < out_sz; i++) {
        if (in[i] == '\'') {
            out[j++] = '\''; out[j++] = '\\';
            out[j++] = '\''; out[j++] = '\'';
        } else {
            out[j++] = in[i];
        }
    }
    out[j++] = '\'';
    out[j]   = '\0';
}

static SeaError run_git_cmd(const char* cmd, SeaArena* arena, SeaSlice* output) {
    SEA_LOG_INFO("GIT", "exec: %s", cmd);

    char full[GIT_CMD_MAX + 16];
    snprintf(full, sizeof(full), "%s 2>&1", cmd);

    FILE* pipe = popen(full, "r");
    if (!pipe) {
        *output = SEA_SLICE_LIT("Error: failed to run git command");
        return SEA_OK;
    }

    u8* buf = (u8*)sea_arena_alloc(arena, GIT_MAX_OUTPUT + 64, 1);
    if (!buf) { pclose(pipe); return SEA_ERR_ARENA_FULL; }

    size_t total = 0;
    while (total < GIT_MAX_OUTPUT) {
        size_t n = fread(buf + total, 1, GIT_MAX_OUTPUT - total, pipe);
        if (n == 0) break;
        total += n;
    }
    int rc = pclose(pipe);

    if (total == GIT_MAX_OUTPUT) {
        const char* trunc = "\n... (truncated at 16KB)";
        memcpy(buf + total, trunc, strlen(trunc));
        total += strlen(trunc);
    }

    char footer[48];
    int flen = snprintf(footer, sizeof(footer), "\n[exit: %d]", rc);
    if (total + (size_t)flen < GIT_MAX_OUTPUT + 64) {
        memcpy(buf + total, footer, (size_t)flen);
        total += (size_t)flen;
    }

    output->data = buf;
    output->len  = (u32)total;
    return SEA_OK;
}

/* Parse first token out of args (space-delimited), return pointer to rest */
static const char* next_token(const char* s, char* tok, size_t tok_sz) {
    while (*s == ' ' || *s == '\t') s++;
    size_t i = 0;
    while (*s && *s != ' ' && *s != '\t' && i + 1 < tok_sz) tok[i++] = *s++;
    tok[i] = '\0';
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

/* ── git_clone ──────────────────────────────────────────────── */

SeaError tool_git_clone(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Usage: git_clone <url> [branch] [dest_dir]\nExample: git_clone https://github.com/owner/repo.git main /workspace/myrepo");
        return SEA_OK;
    }

    char raw[512];
    u32 len = args.len < sizeof(raw) - 1 ? args.len : (u32)(sizeof(raw) - 1);
    memcpy(raw, args.data, len);
    raw[len] = '\0';

    char url[256] = {0}, branch[128] = {0}, dest[256] = {0};
    const char* p = next_token(raw, url, sizeof(url));
    p = next_token(p, branch, sizeof(branch));
    next_token(p, dest, sizeof(dest));

    if (!valid_git_url(url)) {
        *output = SEA_SLICE_LIT("Error: URL must start with https://, http://, or git@");
        return SEA_OK;
    }

    /* Default dest: /workspace/<repo-name-without-.git> */
    if (dest[0] == '\0') {
        const char* slash = strrchr(url, '/');
        const char* base  = slash ? slash + 1 : url;
        snprintf(dest, sizeof(dest), "/workspace/%s", base);
        /* Strip .git suffix */
        size_t dlen = strlen(dest);
        if (dlen > 4 && strcmp(dest + dlen - 4, ".git") == 0)
            dest[dlen - 4] = '\0';
    }

    if (!valid_repo_path(dest)) {
        *output = SEA_SLICE_LIT("Error: destination must be under /workspace or /userdata");
        return SEA_OK;
    }

    char url_esc[512], dest_esc[512];
    shell_escape(url,  url_esc,  sizeof(url_esc));
    shell_escape(dest, dest_esc, sizeof(dest_esc));

    char cmd[GIT_CMD_MAX];
    if (branch[0] != '\0') {
        char br_esc[256];
        shell_escape(branch, br_esc, sizeof(br_esc));
        snprintf(cmd, sizeof(cmd), "git clone --depth 1 --branch %s %s %s",
                 br_esc, url_esc, dest_esc);
    } else {
        snprintf(cmd, sizeof(cmd), "git clone --depth 1 %s %s",
                 url_esc, dest_esc);
    }

    return run_git_cmd(cmd, arena, output);
}

/* ── git_status ─────────────────────────────────────────────── */

SeaError tool_git_status(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Usage: git_status <repo_path>");
        return SEA_OK;
    }

    char path[256];
    u32 len = args.len < sizeof(path) - 1 ? args.len : (u32)(sizeof(path) - 1);
    memcpy(path, args.data, len);
    path[len] = '\0';
    /* strip trailing whitespace */
    for (int i = (int)strlen(path) - 1; i >= 0 && isspace((unsigned char)path[i]); i--)
        path[i] = '\0';

    if (!valid_repo_path(path)) {
        *output = SEA_SLICE_LIT("Error: path must be under /workspace or /userdata");
        return SEA_OK;
    }

    char path_esc[512];
    shell_escape(path, path_esc, sizeof(path_esc));

    char cmd[GIT_CMD_MAX];
    snprintf(cmd, sizeof(cmd), "git -C %s status", path_esc);
    return run_git_cmd(cmd, arena, output);
}

/* ── git_pull ───────────────────────────────────────────────── */

SeaError tool_git_pull(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Usage: git_pull <repo_path>");
        return SEA_OK;
    }

    char path[256];
    u32 len = args.len < sizeof(path) - 1 ? args.len : (u32)(sizeof(path) - 1);
    memcpy(path, args.data, len);
    path[len] = '\0';
    for (int i = (int)strlen(path) - 1; i >= 0 && isspace((unsigned char)path[i]); i--)
        path[i] = '\0';

    if (!valid_repo_path(path)) {
        *output = SEA_SLICE_LIT("Error: path must be under /workspace or /userdata");
        return SEA_OK;
    }

    char path_esc[512];
    shell_escape(path, path_esc, sizeof(path_esc));

    char cmd[GIT_CMD_MAX];
    snprintf(cmd, sizeof(cmd), "git -C %s pull --ff-only", path_esc);
    return run_git_cmd(cmd, arena, output);
}

/* ── git_log ────────────────────────────────────────────────── */

SeaError tool_git_log(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Usage: git_log <repo_path> [count]");
        return SEA_OK;
    }

    char raw[384];
    u32 len = args.len < sizeof(raw) - 1 ? args.len : (u32)(sizeof(raw) - 1);
    memcpy(raw, args.data, len);
    raw[len] = '\0';

    char path[256], count_s[16];
    const char* p = next_token(raw, path, sizeof(path));
    next_token(p, count_s, sizeof(count_s));

    if (!valid_repo_path(path)) {
        *output = SEA_SLICE_LIT("Error: path must be under /workspace or /userdata");
        return SEA_OK;
    }

    int count = 10;
    if (count_s[0] != '\0') {
        int c = atoi(count_s);
        if (c > 0 && c <= 100) count = c;
    }

    char path_esc[512];
    shell_escape(path, path_esc, sizeof(path_esc));

    char cmd[GIT_CMD_MAX];
    snprintf(cmd, sizeof(cmd),
             "git -C %s log --oneline --decorate -n %d", path_esc, count);
    return run_git_cmd(cmd, arena, output);
}

/* ── git_diff ───────────────────────────────────────────────── */

SeaError tool_git_diff(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Usage: git_diff <repo_path>");
        return SEA_OK;
    }

    char path[256];
    u32 len = args.len < sizeof(path) - 1 ? args.len : (u32)(sizeof(path) - 1);
    memcpy(path, args.data, len);
    path[len] = '\0';
    for (int i = (int)strlen(path) - 1; i >= 0 && isspace((unsigned char)path[i]); i--)
        path[i] = '\0';

    if (!valid_repo_path(path)) {
        *output = SEA_SLICE_LIT("Error: path must be under /workspace or /userdata");
        return SEA_OK;
    }

    char path_esc[512];
    shell_escape(path, path_esc, sizeof(path_esc));

    char cmd[GIT_CMD_MAX];
    snprintf(cmd, sizeof(cmd), "git -C %s diff --stat HEAD", path_esc);
    return run_git_cmd(cmd, arena, output);
}

/* ── git_checkout ───────────────────────────────────────────── */

SeaError tool_git_checkout(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Usage: git_checkout <repo_path> <branch>");
        return SEA_OK;
    }

    char raw[384];
    u32 len = args.len < sizeof(raw) - 1 ? args.len : (u32)(sizeof(raw) - 1);
    memcpy(raw, args.data, len);
    raw[len] = '\0';

    char path[256], branch[128];
    const char* p = next_token(raw, path, sizeof(path));
    next_token(p, branch, sizeof(branch));

    if (!valid_repo_path(path)) {
        *output = SEA_SLICE_LIT("Error: path must be under /workspace or /userdata");
        return SEA_OK;
    }
    if (branch[0] == '\0') {
        *output = SEA_SLICE_LIT("Error: branch name required");
        return SEA_OK;
    }

    char path_esc[512], br_esc[256];
    shell_escape(path,   path_esc, sizeof(path_esc));
    shell_escape(branch, br_esc,   sizeof(br_esc));

    char cmd[GIT_CMD_MAX];
    snprintf(cmd, sizeof(cmd), "git -C %s checkout %s", path_esc, br_esc);
    return run_git_cmd(cmd, arena, output);
}
