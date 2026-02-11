/*
 * sea_recall.h — SQLite-Backed Memory Index (The Vault)
 *
 * Stores atomic facts as rows with keyword tokens.
 * Queries score facts by keyword overlap + recency.
 * Only top-N relevant facts loaded into context → saves tokens.
 *
 * Architecture:
 *   facts table: id, category, content, keywords, importance, created_at, accessed_at, access_count
 *   Query: tokenize input → match keywords → score (overlap * importance * recency) → top-N
 *
 * Categories: "user", "preference", "fact", "rule", "context", "identity"
 *
 * "Remember everything. Recall only what matters."
 */

#ifndef SEA_RECALL_H
#define SEA_RECALL_H

#include "sea_types.h"
#include "sea_arena.h"

/* Forward declare — we only need the opaque handle */
typedef struct SeaDb SeaDb;

/* ── Fact Record ─────────────────────────────────────────── */

typedef struct {
    i32         id;
    const char* category;       /* user, preference, fact, rule, context, identity */
    const char* content;        /* The actual fact text */
    const char* keywords;       /* Space-separated keyword tokens */
    i32         importance;     /* 1-10, higher = more likely to be recalled */
    const char* created_at;
    const char* accessed_at;
    i32         access_count;
    f64         score;          /* Computed relevance score (transient) */
} SeaRecallFact;

/* ── Recall Engine ───────────────────────────────────────── */

typedef struct {
    SeaDb*  db;
    bool    initialized;
    u32     max_context_tokens; /* Approx token budget for context injection */
} SeaRecall;

/* ── API ──────────────────────────────────────────────────── */

/* Initialize recall engine. Creates facts table if needed. */
SeaError sea_recall_init(SeaRecall* rc, SeaDb* db, u32 max_context_tokens);

/* Destroy recall engine. */
void sea_recall_destroy(SeaRecall* rc);

/* Store a new fact. Keywords auto-extracted if NULL. */
SeaError sea_recall_store(SeaRecall* rc, const char* category,
                           const char* content, const char* keywords,
                           i32 importance);

/* Query: find top-N facts relevant to the input query.
 * Returns count of facts loaded into `out` array.
 * Facts scored by: keyword_overlap * importance * recency_decay. */
i32 sea_recall_query(SeaRecall* rc, const char* query,
                     SeaRecallFact* out, i32 max_results,
                     SeaArena* arena);

/* Build compressed context string from top facts for a query.
 * Stays within max_context_tokens budget (~4 chars/token estimate).
 * Returns arena-allocated string or NULL. */
const char* sea_recall_build_context(SeaRecall* rc, const char* query,
                                      SeaArena* arena);

/* Forget a fact by ID. */
SeaError sea_recall_forget(SeaRecall* rc, i32 fact_id);

/* Forget all facts in a category. */
SeaError sea_recall_forget_category(SeaRecall* rc, const char* category);

/* Get total fact count. */
u32 sea_recall_count(SeaRecall* rc);

/* Get fact count by category. */
u32 sea_recall_count_category(SeaRecall* rc, const char* category);

#endif /* SEA_RECALL_H */
