/*
 * tool_db_query.c — Read-only SQLite query tool
 *
 * Lets the LLM query the database for tasks, trajectory, config, etc.
 * Only SELECT statements are allowed. All mutations blocked.
 */

#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_db.h"
#include "seaclaw/sea_log.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sqlite3.h>

/* External DB handle from main.c */
extern SeaDb* s_db;

/* Access the internal sqlite3 handle (defined in sea_db.c) */
struct SeaDb { sqlite3* handle; };

SeaError tool_db_query(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (!s_db) {
        *output = SEA_SLICE_LIT("Error: No database available.");
        return SEA_OK;
    }

    if (args.len == 0) {
        *output = SEA_SLICE_LIT(
            "Usage: db_query <SQL>\n"
            "Tables: tasks, trajectory, chat_history, config\n"
            "Only SELECT queries allowed.");
        return SEA_OK;
    }

    /* Null-terminate the query */
    char* sql = (char*)sea_arena_alloc(arena, args.len + 1, 1);
    if (!sql) { *output = SEA_SLICE_LIT("Memory error."); return SEA_ERR_OOM; }
    memcpy(sql, args.data, args.len);
    sql[args.len] = '\0';

    /* Security: only allow SELECT statements */
    const char* p = sql;
    while (*p && isspace((unsigned char)*p)) p++;

    /* Check first keyword is SELECT */
    if (strncasecmp(p, "SELECT", 6) != 0 &&
        strncasecmp(p, "PRAGMA", 6) != 0) {
        *output = SEA_SLICE_LIT("Error: Only SELECT and PRAGMA queries allowed.");
        return SEA_OK;
    }

    /* Block dangerous keywords even in SELECT */
    const char* blocked[] = {
        "DROP", "DELETE", "INSERT", "UPDATE", "ALTER", "CREATE",
        "ATTACH", "DETACH", "REPLACE", "VACUUM", NULL
    };
    for (int i = 0; blocked[i]; i++) {
        /* Case-insensitive search */
        const char* s = sql;
        size_t blen = strlen(blocked[i]);
        while (*s) {
            if (strncasecmp(s, blocked[i], blen) == 0) {
                *output = SEA_SLICE_LIT("Error: Query contains blocked keyword.");
                return SEA_OK;
            }
            s++;
        }
    }

    /* Execute query and collect results */
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(s_db->handle, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        char err[256];
        int n = snprintf(err, sizeof(err), "SQL error: %s",
                         sqlite3_errmsg(s_db->handle));
        u8* dst = (u8*)sea_arena_push_bytes(arena, err, (u64)n);
        if (dst) { output->data = dst; output->len = (u32)n; }
        return SEA_OK;
    }

    /* Format results */
    char buf[4096];
    int pos = 0;
    int cols = sqlite3_column_count(stmt);
    int rows = 0;

    /* Header */
    for (int c = 0; c < cols && pos < (int)sizeof(buf) - 64; c++) {
        if (c > 0) buf[pos++] = '|';
        const char* name = sqlite3_column_name(stmt, c);
        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "%s", name);
    }
    buf[pos++] = '\n';

    /* Rows */
    while (sqlite3_step(stmt) == SQLITE_ROW && rows < 20) {
        for (int c = 0; c < cols && pos < (int)sizeof(buf) - 128; c++) {
            if (c > 0) buf[pos++] = '|';
            const char* val = (const char*)sqlite3_column_text(stmt, c);
            if (val) {
                /* Truncate long values */
                int vlen = (int)strlen(val);
                if (vlen > 80) {
                    pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos,
                                    "%.77s...", val);
                } else {
                    pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos,
                                    "%s", val);
                }
            } else {
                pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "NULL");
            }
        }
        buf[pos++] = '\n';
        rows++;
    }

    sqlite3_finalize(stmt);

    if (rows == 0) {
        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "(no rows)\n");
    }
    pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "(%d row%s)",
                    rows, rows == 1 ? "" : "s");

    u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)pos);
    if (dst) { output->data = dst; output->len = (u32)pos; }
    return SEA_OK;
}
