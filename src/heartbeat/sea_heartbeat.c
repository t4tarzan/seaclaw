/*
 * sea_heartbeat.c — Proactive Agent Heartbeat Implementation
 *
 * Scans HEARTBEAT.md for uncompleted tasks (- [ ] lines)
 * and injects them into the agent loop via the message bus.
 */

#include "seaclaw/sea_heartbeat.h"
#include "seaclaw/sea_log.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/* ── Helpers ──────────────────────────────────────────────── */

static u64 now_epoch(void) {
    return (u64)time(NULL);
}

static void heartbeat_path(const SeaHeartbeat* hb, char* out, u32 out_size) {
    snprintf(out, out_size, "%s/%s",
             sea_memory_workspace(hb->memory), SEA_HEARTBEAT_FILE);
}

/* ── Init ─────────────────────────────────────────────────── */

void sea_heartbeat_init(SeaHeartbeat* hb, SeaMemory* memory,
                         SeaBus* bus, u64 interval_sec) {
    if (!hb) return;
    memset(hb, 0, sizeof(*hb));
    hb->memory = memory;
    hb->bus = bus;
    hb->interval_sec = interval_sec > 0 ? interval_sec
                                         : SEA_HEARTBEAT_DEFAULT_INTERVAL_SEC;
    hb->enabled = true;
    hb->last_check = 0;
    SEA_LOG_INFO("HEARTBEAT", "Initialized (interval: %llus)",
                 (unsigned long long)hb->interval_sec);
}

/* ── Parse HEARTBEAT.md ───────────────────────────────────── */

u32 sea_heartbeat_parse(SeaHeartbeat* hb, SeaHeartbeatTask* out, u32 max) {
    if (!hb || !hb->memory || !out || max == 0) return 0;

    char path[4096 + 64];
    heartbeat_path(hb, path, sizeof(path));

    FILE* f = fopen(path, "r");
    if (!f) return 0;

    u32 count = 0;
    u32 line_num = 0;
    char line[SEA_HEARTBEAT_TASK_MAX];

    while (fgets(line, sizeof(line), f) && count < max) {
        line_num++;

        /* Trim trailing whitespace */
        int len = (int)strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r' ||
                            line[len - 1] == ' ')) {
            line[--len] = '\0';
        }

        /* Skip empty lines and headers */
        if (len == 0 || line[0] == '#') continue;

        /* Check for "- [x]" (completed) */
        if (strstr(line, "- [x]") || strstr(line, "- [X]")) {
            out[count].completed = true;
            const char* text = strstr(line, "] ");
            if (text) text += 2; else text = line;
            strncpy(out[count].text, text, SEA_HEARTBEAT_TASK_MAX - 1);
            out[count].text[SEA_HEARTBEAT_TASK_MAX - 1] = '\0';
            out[count].line = line_num;
            count++;
            continue;
        }

        /* Check for "- [ ]" (pending) */
        if (strstr(line, "- [ ]")) {
            out[count].completed = false;
            const char* text = strstr(line, "] ");
            if (text) text += 2; else text = line;
            strncpy(out[count].text, text, SEA_HEARTBEAT_TASK_MAX - 1);
            out[count].text[SEA_HEARTBEAT_TASK_MAX - 1] = '\0';
            out[count].line = line_num;
            count++;
            continue;
        }
    }

    fclose(f);
    return count;
}

/* ── Inject pending tasks into bus ────────────────────────── */

static u32 inject_pending(SeaHeartbeat* hb) {
    if (!hb->bus) return 0;

    SeaHeartbeatTask tasks[SEA_HEARTBEAT_MAX_TASKS];
    u32 total = sea_heartbeat_parse(hb, tasks, SEA_HEARTBEAT_MAX_TASKS);

    u32 injected = 0;
    for (u32 i = 0; i < total; i++) {
        if (tasks[i].completed) continue;

        /* Build prompt: "Heartbeat task: <task text>" */
        char prompt[SEA_HEARTBEAT_TASK_MAX + 64];
        snprintf(prompt, sizeof(prompt),
                 "[Heartbeat] Pending task from HEARTBEAT.md: %s", tasks[i].text);

        sea_bus_publish_inbound(hb->bus, SEA_MSG_SYSTEM,
                                 "heartbeat", "system", 0,
                                 prompt, (u32)strlen(prompt));
        injected++;

        SEA_LOG_INFO("HEARTBEAT", "Injected task: %.80s%s",
                     tasks[i].text,
                     strlen(tasks[i].text) > 80 ? "..." : "");
    }

    return injected;
}

/* ── Tick ─────────────────────────────────────────────────── */

u32 sea_heartbeat_tick(SeaHeartbeat* hb) {
    if (!hb || !hb->enabled) return 0;

    u64 now = now_epoch();
    if (hb->last_check > 0 && (now - hb->last_check) < hb->interval_sec) {
        return 0; /* Not time yet */
    }

    hb->last_check = now;
    hb->total_checks++;

    u32 injected = inject_pending(hb);
    hb->total_injected += injected;

    if (injected > 0) {
        SEA_LOG_INFO("HEARTBEAT", "Check #%u: injected %u tasks",
                     hb->total_checks, injected);
    }

    return injected;
}

/* ── Trigger (force immediate) ────────────────────────────── */

u32 sea_heartbeat_trigger(SeaHeartbeat* hb) {
    if (!hb || !hb->enabled) return 0;

    hb->last_check = now_epoch();
    hb->total_checks++;

    u32 injected = inject_pending(hb);
    hb->total_injected += injected;

    SEA_LOG_INFO("HEARTBEAT", "Manual trigger: injected %u tasks", injected);
    return injected;
}

/* ── Complete a task ──────────────────────────────────────── */

SeaError sea_heartbeat_complete(SeaHeartbeat* hb, u32 task_line) {
    if (!hb || !hb->memory || task_line == 0) return SEA_ERR_INVALID_INPUT;

    char path[4096 + 64];
    heartbeat_path(hb, path, sizeof(path));

    FILE* f = fopen(path, "r");
    if (!f) return SEA_ERR_IO;

    /* Read entire file */
    char content[SEA_HEARTBEAT_MAX_TASKS * SEA_HEARTBEAT_TASK_MAX];
    char result[sizeof(content)];
    int rpos = 0;
    u32 line_num = 0;
    char line[SEA_HEARTBEAT_TASK_MAX];
    bool found = false;

    while (fgets(line, sizeof(line), f)) {
        line_num++;
        if (line_num == task_line) {
            /* Replace "- [ ]" with "- [x]" */
            char* checkbox = strstr(line, "- [ ]");
            if (checkbox) {
                checkbox[3] = 'x';
                found = true;
            }
        }
        int llen = (int)strlen(line);
        if (rpos + llen < (int)sizeof(result)) {
            memcpy(result + rpos, line, (size_t)llen);
            rpos += llen;
        }
    }
    fclose(f);

    if (!found) return SEA_ERR_NOT_FOUND;

    /* Write back */
    f = fopen(path, "w");
    if (!f) return SEA_ERR_IO;
    fwrite(result, 1, (size_t)rpos, f);
    fclose(f);

    (void)content; /* suppress unused warning */
    SEA_LOG_INFO("HEARTBEAT", "Completed task at line %u", task_line);
    return SEA_OK;
}

/* ── Enable/Disable ───────────────────────────────────────── */

void sea_heartbeat_enable(SeaHeartbeat* hb, bool enabled) {
    if (!hb) return;
    hb->enabled = enabled;
    SEA_LOG_INFO("HEARTBEAT", "%s", enabled ? "Enabled" : "Disabled");
}

/* ── Stats ────────────────────────────────────────────────── */

u32 sea_heartbeat_check_count(const SeaHeartbeat* hb) {
    return hb ? hb->total_checks : 0;
}

u32 sea_heartbeat_injected_count(const SeaHeartbeat* hb) {
    return hb ? hb->total_injected : 0;
}
