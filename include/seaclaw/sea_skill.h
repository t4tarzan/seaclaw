/*
 * sea_skill.h — Skills & Plugin System
 *
 * Markdown-based skills that extend the agent's capabilities.
 * Skills are .md files with YAML frontmatter + instruction body.
 * They can be loaded from a local directory or installed from
 * a GitHub URL.
 *
 * Skill format:
 *   ---
 *   name: summarize
 *   description: Summarize text concisely
 *   trigger: /summarize
 *   ---
 *   You are a summarization expert. Given the following text,
 *   produce a concise summary capturing all key points...
 *
 * "New abilities, loaded at runtime. The Vault learns."
 */

#ifndef SEA_SKILL_H
#define SEA_SKILL_H

#include "sea_types.h"
#include "sea_arena.h"

/* ── Skill Structure ──────────────────────────────────────── */

#define SEA_SKILL_NAME_MAX    64
#define SEA_SKILL_DESC_MAX    256
#define SEA_SKILL_TRIGGER_MAX 64
#define SEA_SKILL_BODY_MAX    4096
#define SEA_SKILL_PATH_MAX    512

typedef struct {
    char  name[SEA_SKILL_NAME_MAX];
    char  description[SEA_SKILL_DESC_MAX];
    char  trigger[SEA_SKILL_TRIGGER_MAX];   /* Command trigger e.g. "/summarize" */
    char  body[SEA_SKILL_BODY_MAX];         /* Prompt/instruction body           */
    char  path[SEA_SKILL_PATH_MAX];         /* Source file path                  */
    bool  enabled;
} SeaSkill;

/* ── Skill Registry ───────────────────────────────────────── */

#define SEA_MAX_SKILLS 64

typedef struct {
    SeaSkill  skills[SEA_MAX_SKILLS];
    u32       count;
    char      skills_dir[SEA_SKILL_PATH_MAX]; /* Directory to scan for .md files */
    SeaArena  arena;
} SeaSkillRegistry;

/* ── API ──────────────────────────────────────────────────── */

/* Initialize the skill registry. skills_dir is scanned for .md files. */
SeaError sea_skill_init(SeaSkillRegistry* reg, const char* skills_dir);

/* Destroy the registry. */
void sea_skill_destroy(SeaSkillRegistry* reg);

/* Load all .md skills from the skills directory. */
SeaError sea_skill_load_all(SeaSkillRegistry* reg);

/* Load a single skill from a .md file. */
SeaError sea_skill_load_file(SeaSkillRegistry* reg, const char* path);

/* Parse a skill from markdown content (frontmatter + body).
 * Fills the provided SeaSkill struct. */
SeaError sea_skill_parse(const char* content, u32 content_len, SeaSkill* out);

/* Register a skill manually (not from file). */
SeaError sea_skill_register(SeaSkillRegistry* reg, const SeaSkill* skill);

/* Find a skill by name. Returns NULL if not found. */
const SeaSkill* sea_skill_find(const SeaSkillRegistry* reg, const char* name);

/* Find a skill by trigger command. Returns NULL if not found. */
const SeaSkill* sea_skill_find_by_trigger(const SeaSkillRegistry* reg, const char* trigger);

/* Get skill count. */
u32 sea_skill_count(const SeaSkillRegistry* reg);

/* List all skill names. Returns count. */
u32 sea_skill_list(const SeaSkillRegistry* reg, const char** names, u32 max);

/* Enable/disable a skill by name. */
SeaError sea_skill_enable(SeaSkillRegistry* reg, const char* name, bool enabled);

/* Build a prompt by combining skill body with user input.
 * Result is allocated in the provided arena. */
const char* sea_skill_build_prompt(const SeaSkill* skill,
                                    const char* user_input, SeaArena* arena);

#endif /* SEA_SKILL_H */
