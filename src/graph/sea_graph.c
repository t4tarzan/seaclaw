/*
 * sea_graph.c — Knowledge Graph Memory Implementation
 *
 * SQLite-backed entity-relation graph with backlinks.
 * Entities are upserted (create or update on name match).
 * Relations are typed directed edges between entities.
 */

#include "seaclaw/sea_graph.h"
#include "seaclaw/sea_db.h"
#include "seaclaw/sea_log.h"

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

/* ── Access internal DB handle ───────────────────────────── */

struct SeaDb { sqlite3* handle; };

/* ── Helpers ──────────────────────────────────────────────── */

static i64 now_epoch(void) { return (i64)time(NULL); }

static const char* entity_type_str(SeaEntityType t) {
    switch (t) {
        case SEA_ENTITY_PERSON:     return "person";
        case SEA_ENTITY_PROJECT:    return "project";
        case SEA_ENTITY_DECISION:   return "decision";
        case SEA_ENTITY_COMMITMENT: return "commitment";
        case SEA_ENTITY_TOPIC:      return "topic";
        case SEA_ENTITY_TOOL:       return "tool";
        case SEA_ENTITY_LOCATION:   return "location";
        case SEA_ENTITY_CUSTOM:     return "custom";
    }
    return "custom";
}

static SeaEntityType entity_type_from_str(const char* s) {
    if (!s) return SEA_ENTITY_CUSTOM;
    if (strcmp(s, "person") == 0)     return SEA_ENTITY_PERSON;
    if (strcmp(s, "project") == 0)    return SEA_ENTITY_PROJECT;
    if (strcmp(s, "decision") == 0)   return SEA_ENTITY_DECISION;
    if (strcmp(s, "commitment") == 0) return SEA_ENTITY_COMMITMENT;
    if (strcmp(s, "topic") == 0)      return SEA_ENTITY_TOPIC;
    if (strcmp(s, "tool") == 0)       return SEA_ENTITY_TOOL;
    if (strcmp(s, "location") == 0)   return SEA_ENTITY_LOCATION;
    return SEA_ENTITY_CUSTOM;
}

static const char* rel_type_str(SeaRelType t) {
    switch (t) {
        case SEA_REL_WORKS_ON:      return "works_on";
        case SEA_REL_DECIDED:       return "decided";
        case SEA_REL_OWNS:          return "owns";
        case SEA_REL_DEPENDS_ON:    return "depends_on";
        case SEA_REL_MENTIONED_IN:  return "mentioned_in";
        case SEA_REL_RELATED_TO:    return "related_to";
        case SEA_REL_BLOCKED_BY:    return "blocked_by";
        case SEA_REL_ASSIGNED_TO:   return "assigned_to";
        case SEA_REL_CUSTOM:        return "custom";
    }
    return "custom";
}

static SeaRelType rel_type_from_str(const char* s) {
    if (!s) return SEA_REL_CUSTOM;
    if (strcmp(s, "works_on") == 0)     return SEA_REL_WORKS_ON;
    if (strcmp(s, "decided") == 0)      return SEA_REL_DECIDED;
    if (strcmp(s, "owns") == 0)         return SEA_REL_OWNS;
    if (strcmp(s, "depends_on") == 0)   return SEA_REL_DEPENDS_ON;
    if (strcmp(s, "mentioned_in") == 0) return SEA_REL_MENTIONED_IN;
    if (strcmp(s, "related_to") == 0)   return SEA_REL_RELATED_TO;
    if (strcmp(s, "blocked_by") == 0)   return SEA_REL_BLOCKED_BY;
    if (strcmp(s, "assigned_to") == 0)  return SEA_REL_ASSIGNED_TO;
    return SEA_REL_CUSTOM;
}

/* Simple SQL-safe string copy (escape single quotes) */
static void sql_escape(char* dst, u32 dst_size, const char* src) {
    u32 j = 0;
    for (u32 i = 0; src[i] && j < dst_size - 2; i++) {
        if (src[i] == '\'') {
            dst[j++] = '\'';
            dst[j++] = '\'';
        } else {
            dst[j++] = src[i];
        }
    }
    dst[j] = '\0';
}

/* ── Init / Destroy ───────────────────────────────────────── */

SeaError sea_graph_init(SeaGraph* g, SeaDb* db) {
    if (!g || !db) return SEA_ERR_INVALID_INPUT;

    g->db = db;
    g->initialized = true;

    /* Create tables */
    sqlite3_exec(db->handle,
        "CREATE TABLE IF NOT EXISTS graph_entities ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  type TEXT NOT NULL,"
        "  name TEXT NOT NULL UNIQUE COLLATE NOCASE,"
        "  summary TEXT DEFAULT '',"
        "  mention_count INTEGER DEFAULT 1,"
        "  created_at INTEGER NOT NULL,"
        "  updated_at INTEGER NOT NULL"
        ");",
        NULL, NULL, NULL);

    sqlite3_exec(db->handle,
        "CREATE TABLE IF NOT EXISTS graph_relations ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  from_id INTEGER NOT NULL,"
        "  to_id INTEGER NOT NULL,"
        "  type TEXT NOT NULL,"
        "  label TEXT DEFAULT '',"
        "  created_at INTEGER NOT NULL,"
        "  FOREIGN KEY (from_id) REFERENCES graph_entities(id) ON DELETE CASCADE,"
        "  FOREIGN KEY (to_id) REFERENCES graph_entities(id) ON DELETE CASCADE"
        ");",
        NULL, NULL, NULL);

    sqlite3_exec(db->handle,
        "CREATE INDEX IF NOT EXISTS idx_ge_name ON graph_entities(name COLLATE NOCASE);"
        "CREATE INDEX IF NOT EXISTS idx_ge_type ON graph_entities(type);"
        "CREATE INDEX IF NOT EXISTS idx_gr_from ON graph_relations(from_id);"
        "CREATE INDEX IF NOT EXISTS idx_gr_to ON graph_relations(to_id);",
        NULL, NULL, NULL);

    SEA_LOG_INFO("GRAPH", "Knowledge graph initialized");
    return SEA_OK;
}

void sea_graph_destroy(SeaGraph* g) {
    if (!g) return;
    g->initialized = false;
    g->db = NULL;
}

/* ── Entity Upsert ────────────────────────────────────────── */

i32 sea_graph_entity_upsert(SeaGraph* g, SeaEntityType type,
                              const char* name, const char* summary) {
    if (!g || !g->db || !name) return -1;

    char esc_name[SEA_ENTITY_NAME_MAX * 2];
    char esc_summary[SEA_ENTITY_SUMMARY_MAX * 2];
    sql_escape(esc_name, sizeof(esc_name), name);
    sql_escape(esc_summary, sizeof(esc_summary), summary ? summary : "");

    i64 now = now_epoch();

    /* Try to find existing */
    char sql[2048];
    snprintf(sql, sizeof(sql),
        "SELECT id FROM graph_entities WHERE name = '%s' COLLATE NOCASE;",
        esc_name);

    i32 existing_id = -1;
    sqlite3_stmt* stmt = NULL;
    if (sqlite3_prepare_v2(g->db->handle, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            existing_id = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    if (existing_id >= 0) {
        /* Update existing */
        snprintf(sql, sizeof(sql),
            "UPDATE graph_entities SET summary = '%s', "
            "mention_count = mention_count + 1, updated_at = %lld "
            "WHERE id = %d;",
            esc_summary, (long long)now, existing_id);
        sqlite3_exec(g->db->handle, sql, NULL, NULL, NULL);
        SEA_LOG_DEBUG("GRAPH", "Updated entity '%s' (id=%d)", name, existing_id);
        return existing_id;
    }

    /* Insert new */
    snprintf(sql, sizeof(sql),
        "INSERT INTO graph_entities (type, name, summary, mention_count, "
        "created_at, updated_at) VALUES ('%s', '%s', '%s', 1, %lld, %lld);",
        entity_type_str(type), esc_name, esc_summary,
        (long long)now, (long long)now);
    sqlite3_exec(g->db->handle, sql, NULL, NULL, NULL);

    i32 new_id = (i32)sqlite3_last_insert_rowid(g->db->handle);

    SEA_LOG_INFO("GRAPH", "Created entity '%s' [%s] (id=%d)",
                 name, entity_type_str(type), new_id);
    return new_id;
}

/* ── Entity Find ──────────────────────────────────────────── */

/* Callback context for entity queries */
typedef struct {
    SeaGraphEntity* out;
    u32 max;
    u32 count;
} EntityCtx;

static int entity_row_cb(void* ctx, int ncols, char** vals, char** cols) {
    EntityCtx* ec = (EntityCtx*)ctx;
    if (ec->count >= ec->max) return 0;

    SeaGraphEntity* e = &ec->out[ec->count];
    memset(e, 0, sizeof(*e));

    for (int i = 0; i < ncols; i++) {
        if (!vals[i]) continue;
        if (strcmp(cols[i], "id") == 0)            e->id = atoi(vals[i]);
        else if (strcmp(cols[i], "type") == 0)     e->type = entity_type_from_str(vals[i]);
        else if (strcmp(cols[i], "name") == 0)     strncpy(e->name, vals[i], SEA_ENTITY_NAME_MAX - 1);
        else if (strcmp(cols[i], "summary") == 0)  strncpy(e->summary, vals[i], SEA_ENTITY_SUMMARY_MAX - 1);
        else if (strcmp(cols[i], "mention_count") == 0) e->mention_count = atoi(vals[i]);
        else if (strcmp(cols[i], "created_at") == 0)    e->created_at = atoll(vals[i]);
        else if (strcmp(cols[i], "updated_at") == 0)    e->updated_at = atoll(vals[i]);
    }
    ec->count++;
    return 0;
}

SeaGraphEntity* sea_graph_entity_find(SeaGraph* g, const char* name,
                                        SeaArena* arena) {
    if (!g || !g->db || !name || !arena) return NULL;

    char esc[SEA_ENTITY_NAME_MAX * 2];
    sql_escape(esc, sizeof(esc), name);

    char sql[512];
    snprintf(sql, sizeof(sql),
        "SELECT * FROM graph_entities WHERE name = '%s' COLLATE NOCASE LIMIT 1;",
        esc);

    SeaGraphEntity* e = (SeaGraphEntity*)sea_arena_alloc(arena, sizeof(SeaGraphEntity), 8);
    if (!e) return NULL;

    EntityCtx ctx = { .out = e, .max = 1, .count = 0 };
    sqlite3_exec(g->db->handle, sql, entity_row_cb, &ctx, NULL);

    return ctx.count > 0 ? e : NULL;
}

SeaGraphEntity* sea_graph_entity_get(SeaGraph* g, i32 id, SeaArena* arena) {
    if (!g || !g->db || !arena) return NULL;

    char sql[128];
    snprintf(sql, sizeof(sql),
        "SELECT * FROM graph_entities WHERE id = %d;", id);

    SeaGraphEntity* e = (SeaGraphEntity*)sea_arena_alloc(arena, sizeof(SeaGraphEntity), 8);
    if (!e) return NULL;

    EntityCtx ctx = { .out = e, .max = 1, .count = 0 };
    sqlite3_exec(g->db->handle, sql, entity_row_cb, &ctx, NULL);

    return ctx.count > 0 ? e : NULL;
}

u32 sea_graph_entity_list(SeaGraph* g, SeaEntityType type,
                            SeaGraphEntity* out, u32 max, SeaArena* arena) {
    if (!g || !g->db || !out) return 0;
    (void)arena;

    char sql[256];
    snprintf(sql, sizeof(sql),
        "SELECT * FROM graph_entities WHERE type = '%s' "
        "ORDER BY mention_count DESC, updated_at DESC LIMIT %u;",
        entity_type_str(type), max);

    EntityCtx ctx = { .out = out, .max = max, .count = 0 };
    sqlite3_exec(g->db->handle, sql, entity_row_cb, &ctx, NULL);
    return ctx.count;
}

u32 sea_graph_entity_search(SeaGraph* g, const char* query,
                              SeaGraphEntity* out, u32 max, SeaArena* arena) {
    if (!g || !g->db || !query || !out) return 0;
    (void)arena;

    char esc[256];
    sql_escape(esc, sizeof(esc), query);

    char sql[512];
    snprintf(sql, sizeof(sql),
        "SELECT * FROM graph_entities WHERE name LIKE '%%%s%%' "
        "ORDER BY mention_count DESC LIMIT %u;",
        esc, max);

    EntityCtx ctx = { .out = out, .max = max, .count = 0 };
    sqlite3_exec(g->db->handle, sql, entity_row_cb, &ctx, NULL);
    return ctx.count;
}

SeaError sea_graph_entity_delete(SeaGraph* g, i32 entity_id) {
    if (!g || !g->db) return SEA_ERR_INVALID_INPUT;

    char sql[128];
    snprintf(sql, sizeof(sql),
        "DELETE FROM graph_relations WHERE from_id = %d OR to_id = %d;",
        entity_id, entity_id);
    sqlite3_exec(g->db->handle, sql, NULL, NULL, NULL);

    snprintf(sql, sizeof(sql),
        "DELETE FROM graph_entities WHERE id = %d;", entity_id);
    sqlite3_exec(g->db->handle, sql, NULL, NULL, NULL);

    SEA_LOG_INFO("GRAPH", "Deleted entity id=%d and its relations", entity_id);
    return SEA_OK;
}

u32 sea_graph_entity_count(SeaGraph* g) {
    if (!g || !g->db) return 0;
    i32 count = 0;
    sqlite3_stmt* stmt = NULL;
    if (sqlite3_prepare_v2(g->db->handle, "SELECT COUNT(*) FROM graph_entities;", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) count = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }
    return (u32)(count > 0 ? count : 0);
}

/* ── Relations ────────────────────────────────────────────── */

i32 sea_graph_relate(SeaGraph* g, i32 from_id, i32 to_id,
                      SeaRelType type, const char* label) {
    if (!g || !g->db) return -1;

    char esc_label[SEA_REL_LABEL_MAX * 2];
    sql_escape(esc_label, sizeof(esc_label), label ? label : "");

    char sql[512];
    snprintf(sql, sizeof(sql),
        "INSERT INTO graph_relations (from_id, to_id, type, label, created_at) "
        "VALUES (%d, %d, '%s', '%s', %lld);",
        from_id, to_id, rel_type_str(type), esc_label, (long long)now_epoch());
    sqlite3_exec(g->db->handle, sql, NULL, NULL, NULL);

    i32 new_id = (i32)sqlite3_last_insert_rowid(g->db->handle);

    SEA_LOG_INFO("GRAPH", "Relation: %d -[%s]-> %d (id=%d)",
                 from_id, rel_type_str(type), to_id, new_id);
    return new_id;
}

typedef struct {
    SeaGraphRelation* out;
    u32 max;
    u32 count;
} RelCtx;

static int rel_row_cb(void* ctx, int ncols, char** vals, char** cols) {
    RelCtx* rc = (RelCtx*)ctx;
    if (rc->count >= rc->max) return 0;

    SeaGraphRelation* r = &rc->out[rc->count];
    memset(r, 0, sizeof(*r));

    for (int i = 0; i < ncols; i++) {
        if (!vals[i]) continue;
        if (strcmp(cols[i], "id") == 0)         r->id = atoi(vals[i]);
        else if (strcmp(cols[i], "from_id") == 0) r->from_id = atoi(vals[i]);
        else if (strcmp(cols[i], "to_id") == 0)   r->to_id = atoi(vals[i]);
        else if (strcmp(cols[i], "type") == 0)    r->type = rel_type_from_str(vals[i]);
        else if (strcmp(cols[i], "label") == 0)   strncpy(r->label, vals[i], SEA_REL_LABEL_MAX - 1);
        else if (strcmp(cols[i], "created_at") == 0) r->created_at = atoll(vals[i]);
    }
    rc->count++;
    return 0;
}

u32 sea_graph_relations_from(SeaGraph* g, i32 entity_id,
                               SeaGraphRelation* out, u32 max, SeaArena* arena) {
    if (!g || !g->db || !out) return 0;
    (void)arena;

    char sql[256];
    snprintf(sql, sizeof(sql),
        "SELECT * FROM graph_relations WHERE from_id = %d ORDER BY created_at DESC LIMIT %u;",
        entity_id, max);

    RelCtx ctx = { .out = out, .max = max, .count = 0 };
    sqlite3_exec(g->db->handle, sql, rel_row_cb, &ctx, NULL);
    return ctx.count;
}

u32 sea_graph_relations_to(SeaGraph* g, i32 entity_id,
                             SeaGraphRelation* out, u32 max, SeaArena* arena) {
    if (!g || !g->db || !out) return 0;
    (void)arena;

    char sql[256];
    snprintf(sql, sizeof(sql),
        "SELECT * FROM graph_relations WHERE to_id = %d ORDER BY created_at DESC LIMIT %u;",
        entity_id, max);

    RelCtx ctx = { .out = out, .max = max, .count = 0 };
    sqlite3_exec(g->db->handle, sql, rel_row_cb, &ctx, NULL);
    return ctx.count;
}

SeaError sea_graph_unrelate(SeaGraph* g, i32 relation_id) {
    if (!g || !g->db) return SEA_ERR_INVALID_INPUT;

    char sql[128];
    snprintf(sql, sizeof(sql),
        "DELETE FROM graph_relations WHERE id = %d;", relation_id);
    sqlite3_exec(g->db->handle, sql, NULL, NULL, NULL);
    return SEA_OK;
}

/* ── Context Building ─────────────────────────────────────── */

const char* sea_graph_build_entity_context(SeaGraph* g, i32 entity_id,
                                             SeaArena* arena) {
    if (!g || !g->db || !arena) return NULL;

    SeaGraphEntity* e = sea_graph_entity_get(g, entity_id, arena);
    if (!e) return NULL;

    /* Start building context */
    char buf[4096];
    int pos = 0;

    pos += snprintf(buf + pos, sizeof(buf) - (u32)pos,
        "## [[%s]] (%s)\n%s\n\n",
        e->name, entity_type_str(e->type), e->summary);

    /* Outgoing relations */
    SeaGraphRelation rels[SEA_GRAPH_MAX_RESULTS];
    u32 out_count = sea_graph_relations_from(g, entity_id, rels, SEA_GRAPH_MAX_RESULTS, arena);
    if (out_count > 0) {
        pos += snprintf(buf + pos, sizeof(buf) - (u32)pos, "### Links\n");
        for (u32 i = 0; i < out_count && pos < (int)sizeof(buf) - 128; i++) {
            SeaGraphEntity* target = sea_graph_entity_get(g, rels[i].to_id, arena);
            if (target) {
                pos += snprintf(buf + pos, sizeof(buf) - (u32)pos,
                    "- %s → [[%s]]\n", rel_type_str(rels[i].type), target->name);
            }
        }
        pos += snprintf(buf + pos, sizeof(buf) - (u32)pos, "\n");
    }

    /* Incoming relations (backlinks) */
    u32 in_count = sea_graph_relations_to(g, entity_id, rels, SEA_GRAPH_MAX_RESULTS, arena);
    if (in_count > 0) {
        pos += snprintf(buf + pos, sizeof(buf) - (u32)pos, "### Backlinks\n");
        for (u32 i = 0; i < in_count && pos < (int)sizeof(buf) - 128; i++) {
            SeaGraphEntity* source = sea_graph_entity_get(g, rels[i].from_id, arena);
            if (source) {
                pos += snprintf(buf + pos, sizeof(buf) - (u32)pos,
                    "- [[%s]] %s this\n", source->name, rel_type_str(rels[i].type));
            }
        }
    }

    /* Copy to arena */
    u32 len = (u32)pos;
    char* result = (char*)sea_arena_alloc(arena, len + 1, 1);
    if (!result) return NULL;
    memcpy(result, buf, len);
    result[len] = '\0';
    return result;
}

const char* sea_graph_build_query_context(SeaGraph* g, const char* query,
                                            SeaArena* arena) {
    if (!g || !g->db || !query || !arena) return NULL;

    SeaGraphEntity results[8];
    u32 count = sea_graph_entity_search(g, query, results, 8, arena);
    if (count == 0) return NULL;

    char buf[8192];
    int pos = 0;
    pos += snprintf(buf + pos, sizeof(buf) - (u32)pos,
        "# Knowledge Graph Context\n\n");

    for (u32 i = 0; i < count && pos < (int)sizeof(buf) - 512; i++) {
        const char* ectx = sea_graph_build_entity_context(g, results[i].id, arena);
        if (ectx) {
            int elen = (int)strlen(ectx);
            if (pos + elen < (int)sizeof(buf) - 16) {
                memcpy(buf + pos, ectx, (size_t)elen);
                pos += elen;
                buf[pos++] = '\n';
            }
        }
    }

    char* result = (char*)sea_arena_alloc(arena, (u32)pos + 1, 1);
    if (!result) return NULL;
    memcpy(result, buf, (size_t)pos);
    result[pos] = '\0';
    return result;
}

/* ── Markdown Export ──────────────────────────────────────── */

const char* sea_graph_export_markdown(SeaGraph* g, i32 entity_id,
                                       SeaArena* arena) {
    /* Same as build_entity_context — already Obsidian-compatible */
    return sea_graph_build_entity_context(g, entity_id, arena);
}
