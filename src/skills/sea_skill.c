/*
 * sea_skill.c — Skills & Plugin System Implementation
 *
 * Parses markdown skill files with YAML frontmatter.
 * Scans a directory for .md files and loads them into the registry.
 */

#include "seaclaw/sea_skill.h"
#include "seaclaw/sea_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

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
