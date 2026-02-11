/*
 * tool_task_manage.c — Create and list tasks in the database
 *
 * Args format:
 *   "list"                    — list all tasks
 *   "create|title|description" — create a new task
 *   "done|task_id"            — mark task as done
 */

#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_db.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* External DB handle from main.c */
extern SeaDb* s_db;

SeaError tool_task_manage(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (!s_db) {
        *output = SEA_SLICE_LIT("Error: database not available");
        return SEA_OK;
    }

    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Usage: list | create|title|desc | done|id");
        return SEA_OK;
    }

    /* Null-terminate args */
    char buf[2048];
    u32 blen = args.len < sizeof(buf) - 1 ? args.len : (u32)(sizeof(buf) - 1);
    memcpy(buf, args.data, blen);
    buf[blen] = '\0';

    /* Strip whitespace */
    char* cmd = buf;
    while (*cmd == ' ') cmd++;

    if (strcmp(cmd, "list") == 0) {
        /* List all tasks */
        SeaDbTask tasks[20];
        i32 count = sea_db_task_list(s_db, NULL, tasks, 20, arena);

        char out[4096];
        int pos = 0;
        if (count <= 0) {
            pos += snprintf(out + pos, sizeof(out) - (size_t)pos, "No tasks found.");
        } else {
            pos += snprintf(out + pos, sizeof(out) - (size_t)pos, "Tasks (%d):\n", count);
            for (i32 i = 0; i < count && pos < (int)sizeof(out) - 128; i++) {
                pos += snprintf(out + pos, sizeof(out) - (size_t)pos,
                    "  #%d [%s] %s\n",
                    tasks[i].id,
                    tasks[i].status ? tasks[i].status : "?",
                    tasks[i].title ? tasks[i].title : "(untitled)");
            }
        }

        u8* dst = (u8*)sea_arena_push_bytes(arena, out, (u64)pos);
        if (dst) { output->data = dst; output->len = (u32)pos; }
        return SEA_OK;
    }

    if (strncmp(cmd, "create|", 7) == 0) {
        /* Parse: create|title|description */
        char* title = cmd + 7;
        char* desc = strchr(title, '|');
        if (desc) {
            *desc = '\0';
            desc++;
        } else {
            desc = "";
        }

        SeaError err = sea_db_task_create(s_db, title, "medium", desc);
        if (err == SEA_OK) {
            char msg[256];
            int mlen = snprintf(msg, sizeof(msg), "Task created: '%s'", title);
            u8* dst = (u8*)sea_arena_push_bytes(arena, msg, (u64)mlen);
            if (dst) { output->data = dst; output->len = (u32)mlen; }
        } else {
            *output = SEA_SLICE_LIT("Error: failed to create task");
        }
        return SEA_OK;
    }

    if (strncmp(cmd, "done|", 5) == 0) {
        i32 task_id = (i32)atoi(cmd + 5);
        SeaError err = sea_db_task_update_status(s_db, task_id, "completed");
        if (err == SEA_OK) {
            char msg[128];
            int mlen = snprintf(msg, sizeof(msg),
                                "Task %d marked as completed", task_id);
            u8* dst = (u8*)sea_arena_push_bytes(arena, msg, (u64)mlen);
            if (dst) { output->data = dst; output->len = (u32)mlen; }
        } else {
            *output = SEA_SLICE_LIT("Error: failed to update task");
        }
        return SEA_OK;
    }

    *output = SEA_SLICE_LIT("Unknown subcommand. Usage: list | create|title|desc | done|id");
    return SEA_OK;
}
