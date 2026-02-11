/*
 * tool_recall.c — Remember/recall/forget facts via SQLite memory index
 *
 * Args:
 *   remember <category> <importance> <content>  — Store a fact
 *   recall <query>                               — Find relevant facts
 *   forget <id>                                  — Delete a fact by ID
 *   forget_all <category>                        — Delete all facts in category
 *   count                                        — Total fact count
 *   list <category>                              — List facts in category
 *
 * Categories: user, preference, fact, rule, context, identity
 */

#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_recall.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* External recall engine from main.c (may be NULL) */
extern SeaRecall* s_recall;

SeaError tool_recall(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT(
            "Usage: remember <category> <importance> <content> | "
            "recall <query> | forget <id> | forget_all <category> | "
            "count | list <category>\n"
            "Categories: user, preference, fact, rule, context, identity");
        return SEA_OK;
    }

    if (!s_recall) {
        *output = SEA_SLICE_LIT("Error: recall system not initialized");
        return SEA_OK;
    }

    char buf[4096];
    u32 len = args.len < sizeof(buf) - 1 ? args.len : (u32)sizeof(buf) - 1;
    memcpy(buf, args.data, len);
    buf[len] = '\0';

    if (strncmp(buf, "remember ", 9) == 0) {
        /* Parse: remember <category> <importance> <content> */
        char category[32];
        int importance = 5;
        int offset = 0;

        if (sscanf(buf + 9, "%31s %d %n", category, &importance, &offset) < 2 || offset == 0) {
            *output = SEA_SLICE_LIT("Usage: remember <category> <importance> <content>");
            return SEA_OK;
        }

        const char* content = buf + 9 + offset;
        if (!content[0]) {
            *output = SEA_SLICE_LIT("Error: no content provided");
            return SEA_OK;
        }

        SeaError err = sea_recall_store(s_recall, category, content, NULL, importance);
        if (err == SEA_OK) {
            char* resp = (char*)sea_arena_alloc(arena, 256, 1);
            if (resp) {
                snprintf(resp, 256, "Remembered [%s] (importance=%d): %.120s%s",
                         category, importance, content, strlen(content) > 120 ? "..." : "");
                output->data = (const u8*)resp;
                output->len = (u32)strlen(resp);
            }
        } else {
            *output = SEA_SLICE_LIT("Error: failed to store fact");
        }

    } else if (strncmp(buf, "recall ", 7) == 0) {
        const char* query = buf + 7;
        SeaRecallFact facts[10];
        i32 count = sea_recall_query(s_recall, query, facts, 10, arena);

        char* resp = (char*)sea_arena_alloc(arena, 4096, 1);
        if (!resp) return SEA_ERR_ARENA_FULL;
        int pos = 0;

        if (count == 0) {
            pos = snprintf(resp, 4096, "No relevant facts found for: %s", query);
        } else {
            pos = snprintf(resp, 4096, "Found %d relevant facts:\n", count);
            for (i32 i = 0; i < count && pos < 3900; i++) {
                pos += snprintf(resp + pos, 4096 - (size_t)pos,
                    "#%d [%s] (score=%.1f, imp=%d): %s\n",
                    facts[i].id, facts[i].category, facts[i].score,
                    facts[i].importance, facts[i].content);
            }
        }
        output->data = (const u8*)resp;
        output->len = (u32)pos;

    } else if (strncmp(buf, "forget_all ", 11) == 0) {
        const char* category = buf + 11;
        SeaError err = sea_recall_forget_category(s_recall, category);
        if (err == SEA_OK) {
            char* resp = (char*)sea_arena_alloc(arena, 128, 1);
            if (resp) {
                snprintf(resp, 128, "Forgot all facts in category: %s", category);
                output->data = (const u8*)resp;
                output->len = (u32)strlen(resp);
            }
        } else {
            *output = SEA_SLICE_LIT("Error: failed to forget category");
        }

    } else if (strncmp(buf, "forget ", 7) == 0) {
        int id = atoi(buf + 7);
        if (id <= 0) {
            *output = SEA_SLICE_LIT("Usage: forget <id> (positive integer)");
            return SEA_OK;
        }
        SeaError err = sea_recall_forget(s_recall, id);
        if (err == SEA_OK) {
            char* resp = (char*)sea_arena_alloc(arena, 64, 1);
            if (resp) {
                snprintf(resp, 64, "Forgot fact #%d", id);
                output->data = (const u8*)resp;
                output->len = (u32)strlen(resp);
            }
        } else {
            *output = SEA_SLICE_LIT("Error: failed to forget fact");
        }

    } else if (strcmp(buf, "count") == 0) {
        u32 total = sea_recall_count(s_recall);
        char* resp = (char*)sea_arena_alloc(arena, 128, 1);
        if (resp) {
            snprintf(resp, 128, "Total facts in memory: %u", total);
            output->data = (const u8*)resp;
            output->len = (u32)strlen(resp);
        }

    } else if (strncmp(buf, "list ", 5) == 0) {
        const char* category = buf + 5;
        /* Use recall query with empty string to get all, filter by category */
        SeaRecallFact facts[20];
        i32 count = sea_recall_query(s_recall, "", facts, 20, arena);

        char* resp = (char*)sea_arena_alloc(arena, 4096, 1);
        if (!resp) return SEA_ERR_ARENA_FULL;
        int pos = snprintf(resp, 4096, "Facts in [%s]:\n", category);

        i32 shown = 0;
        for (i32 i = 0; i < count && pos < 3900; i++) {
            if (strcmp(facts[i].category, category) == 0) {
                pos += snprintf(resp + pos, 4096 - (size_t)pos,
                    "#%d (imp=%d): %s\n",
                    facts[i].id, facts[i].importance, facts[i].content);
                shown++;
            }
        }
        if (shown == 0) {
            pos += snprintf(resp + pos, 4096 - (size_t)pos, "(none)\n");
        }
        output->data = (const u8*)resp;
        output->len = (u32)pos;

    } else {
        *output = SEA_SLICE_LIT(
            "Unknown subcommand. Use: remember | recall | forget | forget_all | count | list");
    }

    return SEA_OK;
}
