/*
 * sea_skill.c — Skills & Plugin System Implementation
 *
 * Parses markdown skill files with YAML frontmatter.
 * Scans a directory for .md files and loads them into the registry.
 */

#include "seaclaw/sea_skill.h"
#include "seaclaw/sea_db.h"
#include "seaclaw/sea_log.h"
#include "seaclaw/sea_http.h"
#include "seaclaw/sea_shield.h"

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>
#include <unistd.h>

/* ── Access internal DB handle ───────────────────────────── */

struct SeaDb { sqlite3* handle; };

/* ── Helpers ──────────────────────────────────────────────── */

static void trim_trailing(char* s) {
    if (!s) return;
    int len = (int)strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r' ||
                        s[len - 1] == ' ' || s[len - 1] == '\t')) {
        s[--len] = '\0';
    }
}

/* Extract value after "key: " from a line. Returns pointer into line. */
static const char* extract_yaml_value(const char* line, const char* key) {
    size_t klen = strlen(key);
    if (strncmp(line, key, klen) != 0) return NULL;
    const char* p = line + klen;
    /* Skip ": " */
    if (*p == ':') p++;
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

/* ── Parse Skill ──────────────────────────────────────────── */

SeaError sea_skill_parse(const char* content, u32 content_len, SeaSkill* out) {
    if (!content || content_len == 0 || !out) return SEA_ERR_INVALID_INPUT;

    memset(out, 0, sizeof(SeaSkill));
    out->enabled = true;

    /* Check for YAML frontmatter delimiter "---" */
    if (content_len < 4 || strncmp(content, "---", 3) != 0) {
        return SEA_ERR_PARSE;
    }

    /* Find the closing "---" */
    const char* body_start = NULL;
    const char* fm_start = content + 3;
    /* Skip newline after opening --- */
    while (*fm_start == '\n' || *fm_start == '\r') fm_start++;

    const char* fm_end = strstr(fm_start, "\n---");
    if (!fm_end) {
        return SEA_ERR_PARSE;
    }

    /* Parse frontmatter lines */
    const char* line = fm_start;
    while (line < fm_end) {
        /* Find end of line */
        const char* eol = strchr(line, '\n');
        if (!eol || eol > fm_end) eol = fm_end;

        /* Copy line to temp buffer */
        u32 llen = (u32)(eol - line);
        if (llen > 0 && llen < 512) {
            char buf[512];
            memcpy(buf, line, llen);
            buf[llen] = '\0';
            trim_trailing(buf);

            const char* val;
            if ((val = extract_yaml_value(buf, "name"))) {
                strncpy(out->name, val, SEA_SKILL_NAME_MAX - 1);
                trim_trailing(out->name);
            } else if ((val = extract_yaml_value(buf, "description"))) {
                strncpy(out->description, val, SEA_SKILL_DESC_MAX - 1);
                trim_trailing(out->description);
            } else if ((val = extract_yaml_value(buf, "trigger"))) {
                strncpy(out->trigger, val, SEA_SKILL_TRIGGER_MAX - 1);
                trim_trailing(out->trigger);
            }
        }

        line = eol + 1;
    }

    /* Body starts after closing "---\n" */
    body_start = fm_end + 4; /* skip "\n---" */
    while (*body_start == '\n' || *body_start == '\r') body_start++;

    u32 body_len = content_len - (u32)(body_start - content);
    if (body_len > SEA_SKILL_BODY_MAX - 1) body_len = SEA_SKILL_BODY_MAX - 1;
    memcpy(out->body, body_start, body_len);
    out->body[body_len] = '\0';
    trim_trailing(out->body);

    /* Validate required fields */
    if (out->name[0] == '\0') return SEA_ERR_PARSE;

    return SEA_OK;
}

/* ── Init / Destroy ───────────────────────────────────────── */

SeaError sea_skill_init(SeaSkillRegistry* reg, const char* skills_dir) {
    if (!reg) return SEA_ERR_INVALID_INPUT;

    memset(reg, 0, sizeof(SeaSkillRegistry));

    if (skills_dir) {
        strncpy(reg->skills_dir, skills_dir, SEA_SKILL_PATH_MAX - 1);
    } else {
        const char* home = getenv("HOME");
        if (!home) home = "/tmp";
        snprintf(reg->skills_dir, SEA_SKILL_PATH_MAX, "%s/.seaclaw/skills", home);
    }

    /* Create skills dir if needed */
    struct stat st;
    if (stat(reg->skills_dir, &st) != 0) {
        mkdir(reg->skills_dir, 0755);
    }

    SeaError err = sea_arena_create(&reg->arena, 64 * 1024);
    if (err != SEA_OK) return err;

    SEA_LOG_INFO("SKILL", "Registry initialized (dir: %s)", reg->skills_dir);
    return SEA_OK;
}

/* ── SQL-safe escape helper ───────────────────────────────── */

static void skill_sql_escape(char* dst, u32 dst_size, const char* src) {
    u32 j = 0;
    for (u32 i = 0; src[i] && j < dst_size - 2; i++) {
        if (src[i] == '\'') { dst[j++] = '\''; dst[j++] = '\''; }
        else { dst[j++] = src[i]; }
    }
    dst[j] = '\0';
}

SeaError sea_skill_init_db(SeaSkillRegistry* reg, const char* skills_dir, SeaDb* db) {
    SeaError err = sea_skill_init(reg, skills_dir);
    if (err != SEA_OK) return err;

    reg->db = db;
    if (!db) return SEA_OK;

    sqlite3_exec(db->handle,
        "CREATE TABLE IF NOT EXISTS skills ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name TEXT NOT NULL UNIQUE,"
        "  description TEXT DEFAULT '',"
        "  trigger_cmd TEXT DEFAULT '',"
        "  body TEXT DEFAULT '',"
        "  path TEXT DEFAULT '',"
        "  enabled INTEGER DEFAULT 1,"
        "  installed_at INTEGER NOT NULL"
        ");",
        NULL, NULL, NULL);

    sea_skill_load_db(reg);
    SEA_LOG_INFO("SKILL", "DB persistence enabled (loaded %u skills from DB)", reg->count);
    return SEA_OK;
}

SeaError sea_skill_save(SeaSkillRegistry* reg) {
    if (!reg || !reg->db) return SEA_ERR_INVALID_INPUT;

    sqlite3_exec(reg->db->handle, "DELETE FROM skills;", NULL, NULL, NULL);

    for (u32 i = 0; i < reg->count; i++) {
        const SeaSkill* s = &reg->skills[i];

        char esc_name[SEA_SKILL_NAME_MAX * 2];
        char esc_desc[SEA_SKILL_DESC_MAX * 2];
        char esc_trigger[SEA_SKILL_TRIGGER_MAX * 2];
        char esc_body[SEA_SKILL_BODY_MAX * 2];
        char esc_path[SEA_SKILL_PATH_MAX * 2];

        skill_sql_escape(esc_name, sizeof(esc_name), s->name);
        skill_sql_escape(esc_desc, sizeof(esc_desc), s->description);
        skill_sql_escape(esc_trigger, sizeof(esc_trigger), s->trigger);
        skill_sql_escape(esc_body, sizeof(esc_body), s->body);
        skill_sql_escape(esc_path, sizeof(esc_path), s->path);

        /* Use prepared statement to avoid truncation issues with large bodies */
        sqlite3_stmt* stmt = NULL;
        const char* insert_sql =
            "INSERT OR REPLACE INTO skills (name, description, trigger_cmd, body, "
            "path, enabled, installed_at) VALUES (?,?,?,?,?,?,?);";
        if (sqlite3_prepare_v2(reg->db->handle, insert_sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, s->name, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, s->description, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 3, s->trigger, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 4, s->body, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 5, s->path, -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 6, s->enabled ? 1 : 0);
            sqlite3_bind_int64(stmt, 7, (sqlite3_int64)time(NULL));
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    SEA_LOG_INFO("SKILL", "Saved %u skills to DB", reg->count);
    return SEA_OK;
}

static int load_skill_cb(void* ctx, int ncols, char** vals, char** cols) {
    SeaSkillRegistry* reg = (SeaSkillRegistry*)ctx;
    if (reg->count >= SEA_MAX_SKILLS) return 0;

    SeaSkill* s = &reg->skills[reg->count];
    memset(s, 0, sizeof(*s));
    s->enabled = true;

    for (int i = 0; i < ncols; i++) {
        if (!vals[i]) continue;
        if (strcmp(cols[i], "name") == 0)
            strncpy(s->name, vals[i], SEA_SKILL_NAME_MAX - 1);
        else if (strcmp(cols[i], "description") == 0)
            strncpy(s->description, vals[i], SEA_SKILL_DESC_MAX - 1);
        else if (strcmp(cols[i], "trigger_cmd") == 0)
            strncpy(s->trigger, vals[i], SEA_SKILL_TRIGGER_MAX - 1);
        else if (strcmp(cols[i], "body") == 0)
            strncpy(s->body, vals[i], SEA_SKILL_BODY_MAX - 1);
        else if (strcmp(cols[i], "path") == 0)
            strncpy(s->path, vals[i], SEA_SKILL_PATH_MAX - 1);
        else if (strcmp(cols[i], "enabled") == 0)
            s->enabled = atoi(vals[i]) != 0;
    }
    reg->count++;
    return 0;
}

SeaError sea_skill_load_db(SeaSkillRegistry* reg) {
    if (!reg || !reg->db) return SEA_ERR_INVALID_INPUT;
    reg->count = 0;
    sqlite3_exec(reg->db->handle,
        "SELECT * FROM skills ORDER BY name;",
        load_skill_cb, reg, NULL);
    return SEA_OK;
}

void sea_skill_destroy(SeaSkillRegistry* reg) {
    if (!reg) return;
    sea_arena_destroy(&reg->arena);
    SEA_LOG_INFO("SKILL", "Registry destroyed (%u skills)", reg->count);
}

/* ── Load from File ───────────────────────────────────────── */

SeaError sea_skill_load_file(SeaSkillRegistry* reg, const char* path) {
    if (!reg || !path) return SEA_ERR_INVALID_INPUT;
    if (reg->count >= SEA_MAX_SKILLS) return SEA_ERR_ARENA_FULL;

    FILE* f = fopen(path, "r");
    if (!f) return SEA_ERR_IO;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0 || size > 64 * 1024) {
        fclose(f);
        return SEA_ERR_IO;
    }

    char* buf = (char*)sea_arena_alloc(&reg->arena, (u64)size + 1, 1);
    if (!buf) { fclose(f); return SEA_ERR_OOM; }

    size_t read = fread(buf, 1, (size_t)size, f);
    buf[read] = '\0';
    fclose(f);

    SeaSkill skill;
    SeaError err = sea_skill_parse(buf, (u32)read, &skill);
    if (err != SEA_OK) {
        SEA_LOG_WARN("SKILL", "Failed to parse %s: %s", path, sea_error_str(err));
        return err;
    }

    strncpy(skill.path, path, SEA_SKILL_PATH_MAX - 1);
    reg->skills[reg->count++] = skill;

    SEA_LOG_INFO("SKILL", "Loaded: %s (%s)", skill.name, skill.trigger);
    return SEA_OK;
}

/* ── Load All from Directory ──────────────────────────────── */

SeaError sea_skill_load_all(SeaSkillRegistry* reg) {
    if (!reg) return SEA_ERR_INVALID_INPUT;

    DIR* dir = opendir(reg->skills_dir);
    if (!dir) {
        SEA_LOG_WARN("SKILL", "Skills dir not found: %s", reg->skills_dir);
        return SEA_OK; /* Not an error — just no skills */
    }

    struct dirent* entry;
    u32 loaded = 0;
    while ((entry = readdir(dir)) != NULL) {
        /* Only .md files */
        const char* name = entry->d_name;
        size_t nlen = strlen(name);
        if (nlen < 4 || strcmp(name + nlen - 3, ".md") != 0) continue;

        char path[SEA_SKILL_PATH_MAX + 256];
        snprintf(path, sizeof(path), "%.511s/%.255s", reg->skills_dir, name);

        if (sea_skill_load_file(reg, path) == SEA_OK) {
            loaded++;
        }
    }

    closedir(dir);
    SEA_LOG_INFO("SKILL", "Loaded %u skills from %s", loaded, reg->skills_dir);
    return SEA_OK;
}

/* ── Register ─────────────────────────────────────────────── */

SeaError sea_skill_register(SeaSkillRegistry* reg, const SeaSkill* skill) {
    if (!reg || !skill) return SEA_ERR_INVALID_INPUT;
    if (reg->count >= SEA_MAX_SKILLS) return SEA_ERR_ARENA_FULL;
    if (skill->name[0] == '\0') return SEA_ERR_INVALID_INPUT;

    reg->skills[reg->count++] = *skill;
    SEA_LOG_INFO("SKILL", "Registered: %s", skill->name);
    return SEA_OK;
}

/* ── Find ─────────────────────────────────────────────────── */

const SeaSkill* sea_skill_find(const SeaSkillRegistry* reg, const char* name) {
    if (!reg || !name) return NULL;
    for (u32 i = 0; i < reg->count; i++) {
        if (strcmp(reg->skills[i].name, name) == 0) {
            return &reg->skills[i];
        }
    }
    return NULL;
}

const SeaSkill* sea_skill_find_by_trigger(const SeaSkillRegistry* reg, const char* trigger) {
    if (!reg || !trigger) return NULL;
    for (u32 i = 0; i < reg->count; i++) {
        if (reg->skills[i].trigger[0] &&
            strcmp(reg->skills[i].trigger, trigger) == 0 &&
            reg->skills[i].enabled) {
            return &reg->skills[i];
        }
    }
    return NULL;
}

/* ── Utility ──────────────────────────────────────────────── */

u32 sea_skill_count(const SeaSkillRegistry* reg) {
    return reg ? reg->count : 0;
}

u32 sea_skill_list(const SeaSkillRegistry* reg, const char** names, u32 max) {
    if (!reg || !names) return 0;
    u32 count = reg->count < max ? reg->count : max;
    for (u32 i = 0; i < count; i++) {
        names[i] = reg->skills[i].name;
    }
    return count;
}

SeaError sea_skill_enable(SeaSkillRegistry* reg, const char* name, bool enabled) {
    if (!reg || !name) return SEA_ERR_INVALID_INPUT;
    for (u32 i = 0; i < reg->count; i++) {
        if (strcmp(reg->skills[i].name, name) == 0) {
            reg->skills[i].enabled = enabled;
            SEA_LOG_INFO("SKILL", "%s skill: %s",
                         enabled ? "Enabled" : "Disabled", name);
            return SEA_OK;
        }
    }
    return SEA_ERR_NOT_FOUND;
}

/* ── Build Prompt ─────────────────────────────────────────── */

const char* sea_skill_build_prompt(const SeaSkill* skill,
                                    const char* user_input, SeaArena* arena) {
    if (!skill || !arena) return NULL;

    u32 body_len = (u32)strlen(skill->body);
    u32 input_len = user_input ? (u32)strlen(user_input) : 0;
    u32 total = body_len + input_len + 64;

    char* prompt = (char*)sea_arena_alloc(arena, total, 1);
    if (!prompt) return NULL;

    if (user_input && input_len > 0) {
        snprintf(prompt, total, "%s\n\nUser input:\n%s", skill->body, user_input);
    } else {
        snprintf(prompt, total, "%s", skill->body);
    }

    return prompt;
}

/* ── Skill Install (v2) ──────────────────────────────────── */

SeaError sea_skill_install_content(SeaSkillRegistry* reg,
                                    const char* content, u32 content_len) {
    if (!reg || !content || content_len == 0) return SEA_ERR_INVALID_INPUT;

    /* Shield check — reject injection attempts in skill content */
    SeaSlice content_slice = { .data = (const u8*)content, .len = content_len };
    if (sea_shield_detect_injection(content_slice)) {
        SEA_LOG_WARN("SKILL", "Install rejected: injection detected in content");
        return SEA_ERR_GRAMMAR_REJECT;
    }

    /* Parse to validate YAML frontmatter */
    SeaSkill skill;
    SeaError err = sea_skill_parse(content, content_len, &skill);
    if (err != SEA_OK) {
        SEA_LOG_WARN("SKILL", "Install rejected: invalid skill format");
        return err;
    }

    /* Check for duplicate */
    if (sea_skill_find(reg, skill.name)) {
        SEA_LOG_WARN("SKILL", "Skill already installed: %s", skill.name);
        return SEA_ERR_ALREADY_EXISTS;
    }

    /* Write to skills_dir/<name>.md */
    char dest_path[SEA_SKILL_PATH_MAX + SEA_SKILL_NAME_MAX + 8];
    snprintf(dest_path, sizeof(dest_path), "%s/%s.md", reg->skills_dir, skill.name);

    FILE* f = fopen(dest_path, "w");
    if (!f) {
        SEA_LOG_ERROR("SKILL", "Cannot write to %s", dest_path);
        return SEA_ERR_IO;
    }
    fwrite(content, 1, content_len, f);
    fclose(f);

    /* Register in memory */
    strncpy(skill.path, dest_path, SEA_SKILL_PATH_MAX - 1);
    err = sea_skill_register(reg, &skill);
    if (err != SEA_OK) return err;

    /* Auto-save to DB if available */
    if (reg->db) sea_skill_save(reg);

    SEA_LOG_INFO("SKILL", "Installed: %s → %s", skill.name, dest_path);
    return SEA_OK;
}

SeaError sea_skill_install(SeaSkillRegistry* reg, const char* url) {
    if (!reg || !url) return SEA_ERR_INVALID_INPUT;

    /* Basic URL validation */
    if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0) {
        SEA_LOG_WARN("SKILL", "Install rejected: URL must start with http(s)://");
        return SEA_ERR_INVALID_INPUT;
    }

    /* Download */
    SEA_LOG_INFO("SKILL", "Downloading skill from %s", url);
    SeaArena dl_arena;
    SeaError err = sea_arena_create(&dl_arena, 128 * 1024);
    if (err != SEA_OK) return err;

    SeaHttpResponse resp;
    err = sea_http_get(url, &dl_arena, &resp);
    if (err != SEA_OK) {
        SEA_LOG_ERROR("SKILL", "Download failed: %s", sea_error_str(err));
        sea_arena_destroy(&dl_arena);
        return err;
    }

    if (resp.status_code != 200) {
        SEA_LOG_ERROR("SKILL", "Download failed: HTTP %d", resp.status_code);
        sea_arena_destroy(&dl_arena);
        return SEA_ERR_IO;
    }

    if (resp.body.len == 0) {
        SEA_LOG_ERROR("SKILL", "Download returned empty body");
        sea_arena_destroy(&dl_arena);
        return SEA_ERR_IO;
    }

    /* Null-terminate for parsing */
    char* content = (char*)sea_arena_alloc(&dl_arena, resp.body.len + 1, 1);
    if (!content) { sea_arena_destroy(&dl_arena); return SEA_ERR_OOM; }
    memcpy(content, resp.body.data, resp.body.len);
    content[resp.body.len] = '\0';

    err = sea_skill_install_content(reg, content, resp.body.len);
    sea_arena_destroy(&dl_arena);
    return err;
}

/* ── AGENT.md Discovery (v2) ─────────────────────────────── */

u32 sea_skill_discover_agents(const char* start_dir,
                               SeaAgentMd* out, u32 max) {
    if (!start_dir || !out || max == 0) return 0;

    char dir[PATH_MAX];
    if (!realpath(start_dir, dir)) {
        strncpy(dir, start_dir, PATH_MAX - 1);
        dir[PATH_MAX - 1] = '\0';
    }

    u32 found = 0;

    while (found < max && strlen(dir) > 1) {
        char agent_path[PATH_MAX + 16];
        snprintf(agent_path, sizeof(agent_path), "%s/AGENT.md", dir);

        struct stat st;
        if (stat(agent_path, &st) == 0 && S_ISREG(st.st_mode)) {
            strncpy(out[found].path, agent_path, SEA_SKILL_PATH_MAX - 1);
            out[found].path[SEA_SKILL_PATH_MAX - 1] = '\0';

            /* Extract directory name as agent name */
            const char* last_slash = strrchr(dir, '/');
            const char* dirname = last_slash ? last_slash + 1 : dir;
            snprintf(out[found].name, SEA_SKILL_NAME_MAX, "agent:%.56s", dirname);

            SEA_LOG_INFO("SKILL", "Discovered AGENT.md: %s", agent_path);
            found++;
        }

        /* Move to parent directory */
        char* slash = strrchr(dir, '/');
        if (!slash || slash == dir) break;
        *slash = '\0';
    }

    return found;
}

SeaError sea_skill_load_agents(SeaSkillRegistry* reg, const char* start_dir) {
    if (!reg) return SEA_ERR_INVALID_INPUT;

    const char* dir = start_dir;
    char cwd[PATH_MAX];
    if (!dir) {
        if (!getcwd(cwd, sizeof(cwd))) return SEA_ERR_IO;
        dir = cwd;
    }

    SeaAgentMd agents[SEA_MAX_AGENT_MDS];
    u32 count = sea_skill_discover_agents(dir, agents, SEA_MAX_AGENT_MDS);

    u32 loaded = 0;
    for (u32 i = 0; i < count; i++) {
        if (sea_skill_load_file(reg, agents[i].path) == SEA_OK) {
            loaded++;
        }
    }

    if (loaded > 0) {
        SEA_LOG_INFO("SKILL", "Loaded %u AGENT.md files", loaded);
    }
    return SEA_OK;
}
