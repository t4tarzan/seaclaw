/*
 * tool_pm.c — Project Management tools for SeaClaw agents
 *
 * Four tools registered here:
 *   pm_task      Args: create|<title>|<project>|<priority>
 *                      list|<project_or_all>
 *                      update|<id>|<status>|<note>
 *                      done|<id>
 *   pm_project   Args: create|<name>|<description>
 *                      list
 *                      status|<name>
 *   pm_milestone Args: set|<project>|<milestone>|<due_date>
 *                      list|<project>
 *   pm_report    Args: <project_or_all>   — generates a markdown summary
 *
 * All state stored in the agent's own SQLite DB (s_db).
 * Schema auto-created on first use.
 */

#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_db.h"
#include "seaclaw/sea_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

/* External DB handle from main.c */
extern SeaDb* s_db;

#define PM_OUT_MAX 8192

/* ── Schema bootstrap ───────────────────────────────────────── */

static bool s_schema_init = false;

static void ensure_pm_schema(void) {
    if (s_schema_init || !s_db) return;
    s_schema_init = true;

    const char* ddl =
        "CREATE TABLE IF NOT EXISTS pm_projects ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name TEXT NOT NULL UNIQUE,"
        "  description TEXT,"
        "  status TEXT DEFAULT 'active',"
        "  created_at DATETIME DEFAULT (datetime('now'))"
        ");"
        "CREATE TABLE IF NOT EXISTS pm_tasks ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  project TEXT NOT NULL DEFAULT 'default',"
        "  title TEXT NOT NULL,"
        "  priority TEXT DEFAULT 'medium',"
        "  status TEXT DEFAULT 'todo',"
        "  note TEXT,"
        "  created_at DATETIME DEFAULT (datetime('now')),"
        "  updated_at DATETIME DEFAULT (datetime('now'))"
        ");"
        "CREATE TABLE IF NOT EXISTS pm_milestones ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  project TEXT NOT NULL,"
        "  name TEXT NOT NULL,"
        "  due_date TEXT,"
        "  done INTEGER DEFAULT 0,"
        "  created_at DATETIME DEFAULT (datetime('now'))"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_pm_tasks_project ON pm_tasks(project);"
        "CREATE INDEX IF NOT EXISTS idx_pm_tasks_status  ON pm_tasks(status);";

    sea_db_exec(s_db, ddl);
}

/* ── Helpers ────────────────────────────────────────────────── */

/* Split buf on '|', fill parts[], return count */
static int split_pipe(char* buf, char* parts[], int max_parts) {
    int n = 0;
    char* p = buf;
    while (n < max_parts) {
        parts[n++] = p;
        p = strchr(p, '|');
        if (!p) break;
        *p++ = '\0';
    }
    return n;
}

/* ── pm_task ────────────────────────────────────────────────── */

SeaError tool_pm_task(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    ensure_pm_schema();
    if (!s_db) { *output = SEA_SLICE_LIT("Error: database not available"); return SEA_OK; }

    if (args.len == 0) {
        *output = SEA_SLICE_LIT(
            "pm_task usage:\n"
            "  create|<title>|<project>|<priority(low/medium/high)>\n"
            "  list|<project_or_all>\n"
            "  update|<id>|<status(todo/in_progress/done)>|<note>\n"
            "  done|<id>");
        return SEA_OK;
    }

    char buf[2048];
    u32 len = args.len < sizeof(buf)-1 ? args.len : (u32)(sizeof(buf)-1);
    memcpy(buf, args.data, len); buf[len] = '\0';

    char* parts[6];
    int n = split_pipe(buf, parts, 6);
    const char* sub = parts[0];

    /* ── create ── */
    if (strcmp(sub, "create") == 0) {
        const char* title    = n > 1 ? parts[1] : "Untitled";
        const char* project  = n > 2 ? parts[2] : "default";
        const char* priority = n > 3 ? parts[3] : "medium";

        char sql[512];
        snprintf(sql, sizeof(sql),
            "INSERT INTO pm_tasks(project,title,priority,status) VALUES('%s','%s','%s','todo')",
            project, title, priority);
        SeaError err = sea_db_exec(s_db, sql);

        char out[256];
        int olen;
        if (err == SEA_OK) {
            olen = snprintf(out, sizeof(out),
                "Task created: [%s] \"%s\" (priority: %s)", project, title, priority);
        } else {
            olen = snprintf(out, sizeof(out), "Error creating task");
        }
        u8* dst = (u8*)sea_arena_push_bytes(arena, out, (u64)olen);
        if (dst) { output->data = dst; output->len = (u32)olen; }
        return SEA_OK;
    }

    /* ── list ── */
    if (strcmp(sub, "list") == 0) {
        const char* project = n > 1 ? parts[1] : "all";
        char sql[512];
        if (strcmp(project, "all") == 0) {
            snprintf(sql, sizeof(sql),
                "SELECT id,project,title,priority,status,note FROM pm_tasks ORDER BY project,status,id LIMIT 100");
        } else {
            snprintf(sql, sizeof(sql),
                "SELECT id,project,title,priority,status,note FROM pm_tasks WHERE project='%s' ORDER BY status,id LIMIT 100",
                project);
        }

        char out[PM_OUT_MAX];
        int pos = 0;
        int count = 0;

        /* Query via sqlite3 CLI — DB path from environment */
        const char* db_path = getenv("SEA_DB");
        if (!db_path) db_path = "/userdata/seaclaw.db";

        char cmd[768];
        if (strcmp(project, "all") == 0) {
            snprintf(cmd, sizeof(cmd),
                "sqlite3 %s \"SELECT id,project,title,priority,status FROM pm_tasks ORDER BY project,id LIMIT 50\" 2>&1",
                db_path);
        } else {
            snprintf(cmd, sizeof(cmd),
                "sqlite3 %s \"SELECT id,project,title,priority,status FROM pm_tasks WHERE project='%s' ORDER BY id LIMIT 50\" 2>&1",
                db_path, project);
        }

        FILE* pipe = popen(cmd, "r");
        if (!pipe) {
            *output = SEA_SLICE_LIT("Error: could not query tasks");
            return SEA_OK;
        }

        pos = snprintf(out, PM_OUT_MAX, "Tasks [%s]:\n", project);
        char line[512];
        while (fgets(line, sizeof(line), pipe) && pos < PM_OUT_MAX - 128) {
            /* Format: id|project|title|priority|status */
            int id; char proj[64], title2[256], prio[16], stat[32];
            if (sscanf(line, "%d|%63[^|]|%255[^|]|%15[^|]|%31s", &id, proj, title2, prio, stat) == 5) {
                pos += snprintf(out + pos, PM_OUT_MAX - (size_t)pos,
                    "  #%d [%s] %s  (%s / %s)\n", id, proj, title2, prio, stat);
            } else {
                /* Raw line fallback */
                int ll = (int)strlen(line);
                if (pos + ll < PM_OUT_MAX - 2) {
                    memcpy(out + pos, line, (size_t)ll);
                    pos += ll;
                }
            }
            count++;
        }
        pclose(pipe);

        if (count == 0)
            pos += snprintf(out + pos, PM_OUT_MAX - (size_t)pos, "  (no tasks)\n");

        u8* dst = (u8*)sea_arena_push_bytes(arena, out, (u64)pos);
        if (dst) { output->data = dst; output->len = (u32)pos; }
        return SEA_OK;
    }

    /* ── update ── */
    if (strcmp(sub, "update") == 0) {
        if (n < 3) { *output = SEA_SLICE_LIT("Usage: update|<id>|<status>|<note>"); return SEA_OK; }
        int id = atoi(parts[1]);
        const char* status = parts[2];
        const char* note = n > 3 ? parts[3] : "";

        char sql[512];
        snprintf(sql, sizeof(sql),
            "UPDATE pm_tasks SET status='%s',note='%s',updated_at=datetime('now') WHERE id=%d",
            status, note, id);
        SeaError err = sea_db_exec(s_db, sql);

        char out[128];
        int olen;
        if (err == SEA_OK)
            olen = snprintf(out, sizeof(out), "Task #%d updated → %s", id, status);
        else
            olen = snprintf(out, sizeof(out), "Error updating task #%d", id);

        u8* dst = (u8*)sea_arena_push_bytes(arena, out, (u64)olen);
        if (dst) { output->data = dst; output->len = (u32)olen; }
        return SEA_OK;
    }

    /* ── done ── */
    if (strcmp(sub, "done") == 0) {
        if (n < 2) { *output = SEA_SLICE_LIT("Usage: done|<id>"); return SEA_OK; }
        int id = atoi(parts[1]);
        char sql[256];
        snprintf(sql, sizeof(sql),
            "UPDATE pm_tasks SET status='done',updated_at=datetime('now') WHERE id=%d", id);
        sea_db_exec(s_db, sql);

        char out[64];
        int olen = snprintf(out, sizeof(out), "Task #%d marked done", id);
        u8* dst = (u8*)sea_arena_push_bytes(arena, out, (u64)olen);
        if (dst) { output->data = dst; output->len = (u32)olen; }
        return SEA_OK;
    }

    *output = SEA_SLICE_LIT("Unknown subcommand. Use: create|list|update|done");
    return SEA_OK;
}

/* ── pm_project ─────────────────────────────────────────────── */

SeaError tool_pm_project(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    ensure_pm_schema();
    if (!s_db) { *output = SEA_SLICE_LIT("Error: database not available"); return SEA_OK; }

    if (args.len == 0) {
        *output = SEA_SLICE_LIT("pm_project usage:\n  create|<name>|<description>\n  list\n  status|<name>");
        return SEA_OK;
    }

    char buf[1024];
    u32 len = args.len < sizeof(buf)-1 ? args.len : (u32)(sizeof(buf)-1);
    memcpy(buf, args.data, len); buf[len] = '\0';

    char* parts[4];
    int n = split_pipe(buf, parts, 4);
    const char* sub = parts[0];
    const char* db_path = getenv("SEA_DB");
    if (!db_path) db_path = "/userdata/seaclaw.db";

    if (strcmp(sub, "create") == 0) {
        const char* name = n > 1 ? parts[1] : "unnamed";
        const char* desc = n > 2 ? parts[2] : "";
        char sql[512];
        snprintf(sql, sizeof(sql),
            "INSERT OR IGNORE INTO pm_projects(name,description) VALUES('%s','%s')", name, desc);
        SeaError err = sea_db_exec(s_db, sql);
        char out[256];
        int olen = snprintf(out, sizeof(out), err == SEA_OK ?
            "Project '%s' created" : "Error (already exists?)", name);
        u8* dst = (u8*)sea_arena_push_bytes(arena, out, (u64)olen);
        if (dst) { output->data = dst; output->len = (u32)olen; }
        return SEA_OK;
    }

    if (strcmp(sub, "list") == 0) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd),
            "sqlite3 %s \"SELECT name,status,description FROM pm_projects ORDER BY name\" 2>&1", db_path);
        FILE* pipe = popen(cmd, "r");
        if (!pipe) { *output = SEA_SLICE_LIT("Error querying projects"); return SEA_OK; }
        char out[PM_OUT_MAX];
        int pos = snprintf(out, PM_OUT_MAX, "Projects:\n");
        char line[512]; int count = 0;
        while (fgets(line, sizeof(line), pipe) && pos < PM_OUT_MAX - 64) {
            pos += snprintf(out + pos, PM_OUT_MAX - (size_t)pos, "  %s", line);
            count++;
        }
        pclose(pipe);
        if (count == 0) pos += snprintf(out + pos, PM_OUT_MAX - (size_t)pos, "  (none)\n");
        u8* dst = (u8*)sea_arena_push_bytes(arena, out, (u64)pos);
        if (dst) { output->data = dst; output->len = (u32)pos; }
        return SEA_OK;
    }

    if (strcmp(sub, "status") == 0) {
        const char* name = n > 1 ? parts[1] : "default";
        char cmd[512];
        snprintf(cmd, sizeof(cmd),
            "sqlite3 %s \"SELECT status,count(*) FROM pm_tasks WHERE project='%s' GROUP BY status\" 2>&1",
            db_path, name);
        FILE* pipe = popen(cmd, "r");
        if (!pipe) { *output = SEA_SLICE_LIT("Error"); return SEA_OK; }
        char out[PM_OUT_MAX];
        int pos = snprintf(out, PM_OUT_MAX, "Project '%s' status:\n", name);
        char line[256];
        while (fgets(line, sizeof(line), pipe) && pos < PM_OUT_MAX - 64)
            pos += snprintf(out + pos, PM_OUT_MAX - (size_t)pos, "  %s", line);
        pclose(pipe);
        u8* dst = (u8*)sea_arena_push_bytes(arena, out, (u64)pos);
        if (dst) { output->data = dst; output->len = (u32)pos; }
        return SEA_OK;
    }

    *output = SEA_SLICE_LIT("Unknown subcommand. Use: create|list|status");
    return SEA_OK;
}

/* ── pm_milestone ───────────────────────────────────────────── */

SeaError tool_pm_milestone(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    ensure_pm_schema();
    if (!s_db) { *output = SEA_SLICE_LIT("Error: database not available"); return SEA_OK; }

    if (args.len == 0) {
        *output = SEA_SLICE_LIT("pm_milestone usage:\n  set|<project>|<name>|<due_date YYYY-MM-DD>\n  list|<project>\n  done|<id>");
        return SEA_OK;
    }

    char buf[512];
    u32 len = args.len < sizeof(buf)-1 ? args.len : (u32)(sizeof(buf)-1);
    memcpy(buf, args.data, len); buf[len] = '\0';

    char* parts[5];
    int n = split_pipe(buf, parts, 5);
    const char* sub = parts[0];
    const char* db_path = getenv("SEA_DB");
    if (!db_path) db_path = "/userdata/seaclaw.db";

    if (strcmp(sub, "set") == 0) {
        const char* project  = n > 1 ? parts[1] : "default";
        const char* name     = n > 2 ? parts[2] : "Milestone";
        const char* due_date = n > 3 ? parts[3] : "";
        char sql[512];
        snprintf(sql, sizeof(sql),
            "INSERT INTO pm_milestones(project,name,due_date) VALUES('%s','%s','%s')",
            project, name, due_date);
        sea_db_exec(s_db, sql);
        char out[256];
        int olen = snprintf(out, sizeof(out), "Milestone '%s' set for project '%s' (due: %s)",
                            name, project, due_date[0] ? due_date : "no date");
        u8* dst = (u8*)sea_arena_push_bytes(arena, out, (u64)olen);
        if (dst) { output->data = dst; output->len = (u32)olen; }
        return SEA_OK;
    }

    if (strcmp(sub, "list") == 0) {
        const char* project = n > 1 ? parts[1] : "default";
        char cmd[512];
        snprintf(cmd, sizeof(cmd),
            "sqlite3 %s \"SELECT id,name,due_date,done FROM pm_milestones WHERE project='%s' ORDER BY due_date\" 2>&1",
            db_path, project);
        FILE* pipe = popen(cmd, "r");
        if (!pipe) { *output = SEA_SLICE_LIT("Error"); return SEA_OK; }
        char out[PM_OUT_MAX];
        int pos = snprintf(out, PM_OUT_MAX, "Milestones [%s]:\n", project);
        char line[256]; int count = 0;
        while (fgets(line, sizeof(line), pipe) && pos < PM_OUT_MAX - 64) {
            int mid; char mname[128], due[32]; int done;
            if (sscanf(line, "%d|%127[^|]|%31[^|]|%d", &mid, mname, due, &done) >= 3) {
                pos += snprintf(out + pos, PM_OUT_MAX - (size_t)pos,
                    "  [%s] #%d %s (due: %s)\n", done ? "✓" : " ", mid, mname, due);
            } else {
                pos += snprintf(out + pos, PM_OUT_MAX - (size_t)pos, "  %s", line);
            }
            count++;
        }
        pclose(pipe);
        if (count == 0) pos += snprintf(out + pos, PM_OUT_MAX - (size_t)pos, "  (none)\n");
        u8* dst = (u8*)sea_arena_push_bytes(arena, out, (u64)pos);
        if (dst) { output->data = dst; output->len = (u32)pos; }
        return SEA_OK;
    }

    if (strcmp(sub, "done") == 0) {
        if (n < 2) { *output = SEA_SLICE_LIT("Usage: done|<id>"); return SEA_OK; }
        int id = atoi(parts[1]);
        char sql[128];
        snprintf(sql, sizeof(sql), "UPDATE pm_milestones SET done=1 WHERE id=%d", id);
        sea_db_exec(s_db, sql);
        char out[64];
        int olen = snprintf(out, sizeof(out), "Milestone #%d marked complete", id);
        u8* dst = (u8*)sea_arena_push_bytes(arena, out, (u64)olen);
        if (dst) { output->data = dst; output->len = (u32)olen; }
        return SEA_OK;
    }

    *output = SEA_SLICE_LIT("Unknown subcommand. Use: set|list|done");
    return SEA_OK;
}

/* ── pm_report ──────────────────────────────────────────────── */

SeaError tool_pm_report(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    ensure_pm_schema();
    if (!s_db) { *output = SEA_SLICE_LIT("Error: database not available"); return SEA_OK; }

    char project[128] = "all";
    if (args.len > 0) {
        u32 len = args.len < sizeof(project)-1 ? args.len : (u32)(sizeof(project)-1);
        memcpy(project, args.data, len);
        project[len] = '\0';
        /* strip whitespace */
        for (int i = (int)strlen(project)-1; i >= 0 && (project[i] == ' ' || project[i] == '\n'); i--)
            project[i] = '\0';
    }

    const char* db_path = getenv("SEA_DB");
    if (!db_path) db_path = "/userdata/seaclaw.db";

    /* Get time */
    time_t now = time(NULL);
    char ts[32];
    struct tm* tm_info = gmtime(&now);
    strftime(ts, sizeof(ts), "%Y-%m-%d", tm_info);

    char out[PM_OUT_MAX];
    int pos = 0;

    pos += snprintf(out + pos, PM_OUT_MAX - (size_t)pos,
        "# Project Report — %s\nGenerated: %s\n\n", project, ts);

    /* Tasks by status */
    char cmd[512];
    if (strcmp(project, "all") == 0) {
        snprintf(cmd, sizeof(cmd),
            "sqlite3 %s \"SELECT status,count(*) FROM pm_tasks GROUP BY status\" 2>&1", db_path);
    } else {
        snprintf(cmd, sizeof(cmd),
            "sqlite3 %s \"SELECT status,count(*) FROM pm_tasks WHERE project='%s' GROUP BY status\" 2>&1",
            db_path, project);
    }

    pos += snprintf(out + pos, PM_OUT_MAX - (size_t)pos, "## Summary\n");
    FILE* pipe = popen(cmd, "r");
    if (pipe) {
        int todo = 0, inprog = 0, done = 0;
        char line[128];
        while (fgets(line, sizeof(line), pipe)) {
            char stat[32]; int cnt;
            if (sscanf(line, "%31[^|]|%d", stat, &cnt) == 2) {
                if (strcmp(stat, "todo")        == 0) todo   = cnt;
                if (strcmp(stat, "in_progress") == 0) inprog = cnt;
                if (strcmp(stat, "done")        == 0) done   = cnt;
            }
        }
        pclose(pipe);
        int total = todo + inprog + done;
        int pct = total > 0 ? (done * 100 / total) : 0;
        pos += snprintf(out + pos, PM_OUT_MAX - (size_t)pos,
            "- Total tasks: %d\n- Done: %d (%d%%)\n- In Progress: %d\n- To Do: %d\n\n",
            total, done, pct, inprog, todo);
    }

    /* Recent completions */
    pos += snprintf(out + pos, PM_OUT_MAX - (size_t)pos, "## Recently Completed\n");
    if (strcmp(project, "all") == 0) {
        snprintf(cmd, sizeof(cmd),
            "sqlite3 %s \"SELECT title,updated_at FROM pm_tasks WHERE status='done' ORDER BY updated_at DESC LIMIT 5\" 2>&1",
            db_path);
    } else {
        snprintf(cmd, sizeof(cmd),
            "sqlite3 %s \"SELECT title,updated_at FROM pm_tasks WHERE project='%s' AND status='done' ORDER BY updated_at DESC LIMIT 5\" 2>&1",
            db_path, project);
    }
    pipe = popen(cmd, "r");
    if (pipe) {
        char line[512]; int cnt = 0;
        while (fgets(line, sizeof(line), pipe) && pos < PM_OUT_MAX - 128) {
            char title[256], updated[32];
            if (sscanf(line, "%255[^|]|%31s", title, updated) >= 1) {
                pos += snprintf(out + pos, PM_OUT_MAX - (size_t)pos, "- %s\n", title);
            }
            cnt++;
        }
        pclose(pipe);
        if (cnt == 0) pos += snprintf(out + pos, PM_OUT_MAX - (size_t)pos, "- (none yet)\n");
    }
    pos += snprintf(out + pos, PM_OUT_MAX - (size_t)pos, "\n");

    /* Active tasks */
    pos += snprintf(out + pos, PM_OUT_MAX - (size_t)pos, "## In Progress\n");
    if (strcmp(project, "all") == 0) {
        snprintf(cmd, sizeof(cmd),
            "sqlite3 %s \"SELECT project,title FROM pm_tasks WHERE status='in_progress' ORDER BY project LIMIT 10\" 2>&1",
            db_path);
    } else {
        snprintf(cmd, sizeof(cmd),
            "sqlite3 %s \"SELECT title FROM pm_tasks WHERE project='%s' AND status='in_progress' LIMIT 10\" 2>&1",
            db_path, project);
    }
    pipe = popen(cmd, "r");
    if (pipe) {
        char line[512]; int cnt = 0;
        while (fgets(line, sizeof(line), pipe) && pos < PM_OUT_MAX - 128) {
            /* strip trailing newline */
            line[strcspn(line, "\n")] = '\0';
            pos += snprintf(out + pos, PM_OUT_MAX - (size_t)pos, "- %s\n", line);
            cnt++;
        }
        pclose(pipe);
        if (cnt == 0) pos += snprintf(out + pos, PM_OUT_MAX - (size_t)pos, "- (none)\n");
    }
    pos += snprintf(out + pos, PM_OUT_MAX - (size_t)pos, "\n");

    /* Milestones */
    pos += snprintf(out + pos, PM_OUT_MAX - (size_t)pos, "## Milestones\n");
    if (strcmp(project, "all") == 0) {
        snprintf(cmd, sizeof(cmd),
            "sqlite3 %s \"SELECT project,name,due_date,done FROM pm_milestones ORDER BY done,due_date LIMIT 10\" 2>&1",
            db_path);
    } else {
        snprintf(cmd, sizeof(cmd),
            "sqlite3 %s \"SELECT name,due_date,done FROM pm_milestones WHERE project='%s' ORDER BY done,due_date LIMIT 10\" 2>&1",
            db_path, project);
    }
    pipe = popen(cmd, "r");
    if (pipe) {
        char line[512]; int cnt = 0;
        while (fgets(line, sizeof(line), pipe) && pos < PM_OUT_MAX - 128) {
            line[strcspn(line, "\n")] = '\0';
            pos += snprintf(out + pos, PM_OUT_MAX - (size_t)pos, "- %s\n", line);
            cnt++;
        }
        pclose(pipe);
        if (cnt == 0) pos += snprintf(out + pos, PM_OUT_MAX - (size_t)pos, "- (none)\n");
    }

    u8* dst = (u8*)sea_arena_push_bytes(arena, out, (u64)pos);
    if (dst) { output->data = dst; output->len = (u32)pos; }
    return SEA_OK;
}
