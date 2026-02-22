/*
 * sea_graph.h — Knowledge Graph Memory
 *
 * Entity-relation graph stored in SQLite. Entities are people,
 * projects, decisions, commitments. Relations are typed edges
 * (works_on, decided, owns, depends_on, etc.).
 *
 * Entities can be exported as Obsidian-compatible markdown with
 * [[wiki-links]] backlinks for inspection and editing.
 *
 * Inspired by Rowboat's compounding knowledge graph.
 *
 * "Memory that compounds, not retrieval that starts cold."
 */

#ifndef SEA_GRAPH_H
#define SEA_GRAPH_H

#include "sea_types.h"
#include "sea_arena.h"

/* Forward declare */
typedef struct SeaDb SeaDb;

/* ── Entity Types ─────────────────────────────────────────── */

typedef enum {
    SEA_ENTITY_PERSON = 0,
    SEA_ENTITY_PROJECT,
    SEA_ENTITY_DECISION,
    SEA_ENTITY_COMMITMENT,
    SEA_ENTITY_TOPIC,
    SEA_ENTITY_TOOL,
    SEA_ENTITY_LOCATION,
    SEA_ENTITY_CUSTOM,
} SeaEntityType;

/* ── Relation Types ───────────────────────────────────────── */

typedef enum {
    SEA_REL_WORKS_ON = 0,       /* person → project        */
    SEA_REL_DECIDED,            /* person → decision       */
    SEA_REL_OWNS,               /* person → project/topic  */
    SEA_REL_DEPENDS_ON,         /* project → project       */
    SEA_REL_MENTIONED_IN,       /* entity → context        */
    SEA_REL_RELATED_TO,         /* entity → entity         */
    SEA_REL_BLOCKED_BY,         /* project → project       */
    SEA_REL_ASSIGNED_TO,        /* commitment → person     */
    SEA_REL_CUSTOM,
} SeaRelType;

/* ── Entity Record ────────────────────────────────────────── */

#define SEA_ENTITY_NAME_MAX     128
#define SEA_ENTITY_SUMMARY_MAX  512

typedef struct {
    i32             id;
    SeaEntityType   type;
    char            name[SEA_ENTITY_NAME_MAX];
    char            summary[SEA_ENTITY_SUMMARY_MAX];
    i32             mention_count;
    i64             created_at;
    i64             updated_at;
} SeaGraphEntity;

/* ── Relation Record ──────────────────────────────────────── */

#define SEA_REL_LABEL_MAX  64

typedef struct {
    i32         id;
    i32         from_id;        /* Source entity ID */
    i32         to_id;          /* Target entity ID */
    SeaRelType  type;
    char        label[SEA_REL_LABEL_MAX];   /* Human-readable label */
    i64         created_at;
} SeaGraphRelation;

/* ── Graph Engine ─────────────────────────────────────────── */

#define SEA_GRAPH_MAX_RESULTS 32

typedef struct {
    SeaDb*  db;
    bool    initialized;
} SeaGraph;

/* ── API ──────────────────────────────────────────────────── */

/* Initialize the knowledge graph. Creates tables if needed. */
SeaError sea_graph_init(SeaGraph* g, SeaDb* db);

/* Destroy the graph engine. */
void sea_graph_destroy(SeaGraph* g);

/* ── Entity CRUD ──────────────────────────────────────────── */

/* Add or update an entity. If name exists, updates summary + bumps mention_count.
 * Returns entity ID (>= 0) or -1 on error. */
i32 sea_graph_entity_upsert(SeaGraph* g, SeaEntityType type,
                              const char* name, const char* summary);

/* Find entity by name (case-insensitive). Returns NULL if not found. */
SeaGraphEntity* sea_graph_entity_find(SeaGraph* g, const char* name,
                                        SeaArena* arena);

/* Find entity by ID. */
SeaGraphEntity* sea_graph_entity_get(SeaGraph* g, i32 id, SeaArena* arena);

/* List entities by type. Returns count. */
u32 sea_graph_entity_list(SeaGraph* g, SeaEntityType type,
                            SeaGraphEntity* out, u32 max, SeaArena* arena);

/* Search entities by name substring. Returns count. */
u32 sea_graph_entity_search(SeaGraph* g, const char* query,
                              SeaGraphEntity* out, u32 max, SeaArena* arena);

/* Delete entity and all its relations. */
SeaError sea_graph_entity_delete(SeaGraph* g, i32 entity_id);

/* Get total entity count. */
u32 sea_graph_entity_count(SeaGraph* g);

/* ── Relations ────────────────────────────────────────────── */

/* Add a relation between two entities. Returns relation ID or -1. */
i32 sea_graph_relate(SeaGraph* g, i32 from_id, i32 to_id,
                      SeaRelType type, const char* label);

/* Get all relations FROM an entity. Returns count. */
u32 sea_graph_relations_from(SeaGraph* g, i32 entity_id,
                               SeaGraphRelation* out, u32 max, SeaArena* arena);

/* Get all relations TO an entity (backlinks). Returns count. */
u32 sea_graph_relations_to(SeaGraph* g, i32 entity_id,
                             SeaGraphRelation* out, u32 max, SeaArena* arena);

/* Remove a relation by ID. */
SeaError sea_graph_unrelate(SeaGraph* g, i32 relation_id);

/* ── Context Building ─────────────────────────────────────── */

/* Build a context string for an entity: its summary + all related
 * entities with [[wiki-link]] backlinks. Arena-allocated. */
const char* sea_graph_build_entity_context(SeaGraph* g, i32 entity_id,
                                             SeaArena* arena);

/* Build a context string for a query: find matching entities and
 * their neighborhoods. For injection into LLM system prompt. */
const char* sea_graph_build_query_context(SeaGraph* g, const char* query,
                                            SeaArena* arena);

/* ── Markdown Export ──────────────────────────────────────── */

/* Export an entity as Obsidian-compatible markdown with [[backlinks]].
 * Returns arena-allocated markdown string. */
const char* sea_graph_export_markdown(SeaGraph* g, i32 entity_id,
                                       SeaArena* arena);

#endif /* SEA_GRAPH_H */
