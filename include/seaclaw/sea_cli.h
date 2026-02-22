/*
 * sea_cli.h — CLI Subcommand Dispatch
 *
 * Table-driven subcommand routing for `sea_claw <subcommand>`.
 * Each subcommand is a function pointer with argc/argv.
 *
 * "One binary, many faces."
 */

#ifndef SEA_CLI_H
#define SEA_CLI_H

#include "sea_types.h"

/* ── Subcommand function signature ────────────────────────── */

typedef int (*SeaCliFunc)(int argc, char** argv);

/* ── Subcommand definition ────────────────────────────────── */

#define SEA_CLI_NAME_MAX  16
#define SEA_CLI_MAX       32

typedef struct {
    char        name[SEA_CLI_NAME_MAX];
    const char* description;
    const char* usage;
    SeaCliFunc  func;
} SeaCliCmd;

/* ── CLI Registry ─────────────────────────────────────────── */

typedef struct {
    SeaCliCmd  commands[SEA_CLI_MAX];
    u32        count;
} SeaCli;

/* Initialize the CLI registry with built-in subcommands. */
void sea_cli_init(SeaCli* cli);

/* Register a subcommand. Returns SEA_OK or SEA_ERR_FULL. */
SeaError sea_cli_register(SeaCli* cli, const char* name,
                           const char* description, const char* usage,
                           SeaCliFunc func);

/* Find a subcommand by name. Returns NULL if not found. */
const SeaCliCmd* sea_cli_find(const SeaCli* cli, const char* name);

/* Dispatch: run the matching subcommand, or return -1 if not found. */
int sea_cli_dispatch(const SeaCli* cli, int argc, char** argv);

/* Print help listing all subcommands. */
void sea_cli_help(const SeaCli* cli);

/* ── Built-in subcommand implementations ──────────────────── */

int sea_cmd_doctor(int argc, char** argv);
int sea_cmd_onboard(int argc, char** argv);
int sea_cmd_version(int argc, char** argv);
int sea_cmd_help(int argc, char** argv);

#endif /* SEA_CLI_H */
