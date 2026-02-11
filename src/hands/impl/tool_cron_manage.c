/*
 * tool_cron_manage.c — Create/list/remove cron jobs from agent
 *
 * Args:
 *   list                          — List all cron jobs
 *   add <name> <schedule> <cmd>   — Add a new job
 *   remove <id>                   — Remove a job by ID
 *   pause <id>                    — Pause a job
 *   resume <id>                   — Resume a job
 */

#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_cron.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* External scheduler from main.c (may be NULL if not initialized) */
extern SeaCronScheduler* s_cron;

SeaError tool_cron_manage(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT(
            "Usage: list | add <name> <schedule> <command> | "
            "remove <id> | pause <id> | resume <id>");
        return SEA_OK;
    }

    if (!s_cron) {
        *output = SEA_SLICE_LIT("Error: cron scheduler not initialized");
        return SEA_OK;
    }

    char buf[2048];
    u32 len = args.len < sizeof(buf) - 1 ? args.len : (u32)sizeof(buf) - 1;
    memcpy(buf, args.data, len);
    buf[len] = '\0';

    char* result = (char*)sea_arena_alloc(arena, 4096, 1);
    if (!result) return SEA_ERR_ARENA_FULL;
    int pos = 0;

    if (strncmp(buf, "list", 4) == 0) {
        u32 count = sea_cron_count(s_cron);
        if (count == 0) {
            pos = snprintf(result, 4096, "No cron jobs scheduled.");
        } else {
            pos = snprintf(result, 4096, "Cron jobs (%u):\n", count);
            SeaCronJob* jobs[SEA_MAX_CRON_JOBS];
            u32 n = sea_cron_list(s_cron, jobs, SEA_MAX_CRON_JOBS);
            for (u32 i = 0; i < n && pos < 3900; i++) {
                const char* state = "active";
                if (jobs[i]->state == SEA_CRON_PAUSED) state = "paused";
                else if (jobs[i]->state == SEA_CRON_COMPLETED) state = "completed";
                else if (jobs[i]->state == SEA_CRON_FAILED) state = "failed";

                pos += snprintf(result + pos, 4096 - (size_t)pos,
                    "  #%d %s [%s] %s — runs: %u, cmd: %s\n",
                    jobs[i]->id, jobs[i]->name, state,
                    jobs[i]->schedule, jobs[i]->run_count, jobs[i]->command);
            }
        }
    } else if (strncmp(buf, "add ", 4) == 0) {
        /* Parse: add <name> <schedule> <command> */
        char* p = buf + 4;
        while (*p == ' ') p++;
        char* name = p;
        char* sp = strchr(p, ' ');
        if (!sp) {
            pos = snprintf(result, 4096,
                "Error: usage: add <name> <schedule> <command>");
        } else {
            *sp = '\0';
            char* schedule = sp + 1;
            while (*schedule == ' ') schedule++;
            /* Schedule ends at next space */
            sp = strchr(schedule, ' ');
            if (!sp) {
                pos = snprintf(result, 4096,
                    "Error: usage: add <name> <schedule> <command>");
            } else {
                *sp = '\0';
                char* command = sp + 1;
                while (*command == ' ') command++;

                i32 id = sea_cron_add(s_cron, name, SEA_CRON_SHELL,
                                       schedule, command, NULL);
                if (id < 0) {
                    pos = snprintf(result, 4096,
                        "Error: failed to add job (invalid schedule or full)");
                } else {
                    pos = snprintf(result, 4096,
                        "Added cron job #%d '%s' [%s] → %s", id, name, schedule, command);
                }
            }
        }
    } else if (strncmp(buf, "remove ", 7) == 0) {
        i32 id = atoi(buf + 7);
        SeaError err = sea_cron_remove(s_cron, id);
        if (err == SEA_OK) {
            pos = snprintf(result, 4096, "Removed cron job #%d", id);
        } else {
            pos = snprintf(result, 4096, "Error: job #%d not found", id);
        }
    } else if (strncmp(buf, "pause ", 6) == 0) {
        i32 id = atoi(buf + 6);
        SeaError err = sea_cron_pause(s_cron, id);
        if (err == SEA_OK) {
            pos = snprintf(result, 4096, "Paused cron job #%d", id);
        } else {
            pos = snprintf(result, 4096, "Error: job #%d not found", id);
        }
    } else if (strncmp(buf, "resume ", 7) == 0) {
        i32 id = atoi(buf + 7);
        SeaError err = sea_cron_resume(s_cron, id);
        if (err == SEA_OK) {
            pos = snprintf(result, 4096, "Resumed cron job #%d", id);
        } else {
            pos = snprintf(result, 4096, "Error: job #%d not found", id);
        }
    } else {
        pos = snprintf(result, 4096,
            "Unknown subcommand. Use: list | add | remove | pause | resume");
    }

    output->data = (const u8*)result;
    output->len = (u32)pos;
    return SEA_OK;
}
