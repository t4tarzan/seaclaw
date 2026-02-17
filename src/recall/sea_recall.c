/*
 * sea_recall.c — SQLite-Backed Memory Index
 *
 * Stores atomic facts with keyword tokens.
 * Queries score by keyword overlap * importance * recency decay.
 * Only top-N relevant facts loaded into context.
 */

#include "seaclaw/sea_recall.h"
#include "seaclaw/sea_db.h"
#include "seaclaw/sea_log.h"

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <math.h>

/* ── Access internal DB handle ───────────────────────────── */

struct SeaDb { sqlite3* handle; };

/* ── Schema ──────────────────────────────────────────────── */

static const char* SCHEMA_SQL =
    "CREATE TABLE IF NOT EXISTS recall_facts ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  category TEXT NOT NULL DEFAULT 'fact',"
    "  content TEXT NOT NULL,"
    "  keywords TEXT NOT NULL DEFAULT '',"
    "  importance INTEGER NOT NULL DEFAULT 5,"
    "  created_at TEXT NOT NULL DEFAULT (datetime('now')),"
    "  accessed_at TEXT NOT NULL DEFAULT (datetime('now')),"
    "  access_count INTEGER NOT NULL DEFAULT 0"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_recall_keywords ON recall_facts(keywords);"
    "CREATE INDEX IF NOT EXISTS idx_recall_category ON recall_facts(category);";

/* ── Keyword Extraction ──────────────────────────────────── */

/* Stop words to skip during keyword extraction */
static const char* STOP_WORDS[] = {
    "the", "a", "an", "is", "are", "was", "were", "be", "been", "being",
    "have", "has", "had", "do", "does", "did", "will", "would", "could",
    "should", "may", "might", "shall", "can", "need", "dare", "ought",
    "used", "to", "of", "in", "for", "on", "with", "at", "by", "from",
    "as", "into", "through", "during", "before", "after", "above", "below",
    "between", "out", "off", "over", "under", "again", "further", "then",
    "once", "here", "there", "when", "where", "why", "how", "all", "both",
    "each", "few", "more", "most", "other", "some", "such", "no", "nor",
    "not", "only", "own", "same", "so", "than", "too", "very", "just",
    "and", "but", "or", "if", "while", "that", "this", "it", "its",
    "i", "me", "my", "we", "our", "you", "your", "he", "him", "his",
    "she", "her", "they", "them", "their", "what", "which", "who", "whom",
    NULL
};

static bool is_stop_word(const char* word) {
    for (int i = 0; STOP_WORDS[i]; i++) {
        if (strcmp(word, STOP_WORDS[i]) == 0) return true;
    }
    return false;
}

/* Extract keywords from text: lowercase, skip stop words, skip short words.
 * Returns space-separated keywords in buf. */
static void extract_keywords(const char* text, char* buf, u32 buf_size) {
    if (!text || !buf || buf_size == 0) return;
    buf[0] = '\0';
    u32 pos = 0;

    char word[64];
    u32 wlen = 0;

    for (const char* p = text; ; p++) {
        if (*p && (isalnum((unsigned char)*p) || *p == '_')) {
            if (wlen < sizeof(word) - 1) {
                word[wlen++] = (char)tolower((unsigned char)*p);
            }
        } else {
            if (wlen >= 3) { /* Skip words shorter than 3 chars */
                word[wlen] = '\0';
                if (!is_stop_word(word)) {
                    if (pos > 0 && pos < buf_size - 1) buf[pos++] = ' ';
                    u32 copy = wlen;
                    if (pos + copy >= buf_size - 1) copy = buf_size - 1 - pos;
                    memcpy(buf + pos, word, copy);
                    pos += copy;
                }
            }
            wlen = 0;
            if (!*p) break;
        }
    }
    buf[pos] = '\0';
}

/* ── Keyword Scoring ─────────────────────────────────────── */

/* Count how many query keywords appear in the fact's keywords.
 * Returns overlap count. */
static i32 keyword_overlap(const char* query_kw, const char* fact_kw) {
    if (!query_kw || !fact_kw || !query_kw[0] || !fact_kw[0]) return 0;

    i32 overlap = 0;
    char qcopy[1024];
    strncpy(qcopy, query_kw, sizeof(qcopy) - 1);
    qcopy[sizeof(qcopy) - 1] = '\0';

    char* saveptr = NULL;
    char* token = strtok_r(qcopy, " ", &saveptr);
    while (token) {
        /* Check if this query keyword appears in fact keywords */
        if (strstr(fact_kw, token)) {
            overlap++;
        }
        token = strtok_r(NULL, " ", &saveptr);
    }
    return overlap;
}

/* Recency decay: facts accessed recently score higher.
 * Returns 0.1 to 1.0 based on days since last access. */
static f64 recency_score(const char* accessed_at) {
    if (!accessed_at || !accessed_at[0]) return 0.5;

    /* Parse "YYYY-MM-DD HH:MM:SS" */
    struct tm tm_accessed;
    memset(&tm_accessed, 0, sizeof(tm_accessed));
    if (sscanf(accessed_at, "%d-%d-%d %d:%d:%d",
               &tm_accessed.tm_year, &tm_accessed.tm_mon, &tm_accessed.tm_mday,
               &tm_accessed.tm_hour, &tm_accessed.tm_min, &tm_accessed.tm_sec) < 3) {
        return 0.5;
    }
    tm_accessed.tm_year -= 1900;
    tm_accessed.tm_mon -= 1;

    time_t accessed = mktime(&tm_accessed);
    time_t now = time(NULL);
    double days = difftime(now, accessed) / 86400.0;

    /* Exponential decay: half-life of 7 days */
    return 0.1 + 0.9 * exp(-days / 7.0);
}

/* ── Init / Destroy ──────────────────────────────────────── */

SeaError sea_recall_init(SeaRecall* rc, SeaDb* db, u32 max_context_tokens) {
    if (!rc || !db) return SEA_ERR_INVALID_INPUT;

    rc->db = db;
    rc->max_context_tokens = max_context_tokens > 0 ? max_context_tokens : 800;
    rc->initialized = false;

    /* Create schema */
    char* err_msg = NULL;
    int ret = sqlite3_exec(db->handle, SCHEMA_SQL, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK) {
        SEA_LOG_ERROR("RECALL", "Schema creation failed: %s", err_msg ? err_msg : "unknown");
        if (err_msg) sqlite3_free(err_msg);
        return SEA_ERR_IO;
    }

    rc->initialized = true;
    SEA_LOG_INFO("RECALL", "Memory index ready (budget: %u tokens)", rc->max_context_tokens);
    return SEA_OK;
}

void sea_recall_destroy(SeaRecall* rc) {
    if (!rc) return;
    rc->initialized = false;
    rc->db = NULL;
}

/* ── Store ────────────────────────────────────────────────── */

SeaError sea_recall_store(SeaRecall* rc, const char* category,
                           const char* content, const char* keywords,
                           i32 importance) {
    if (!rc || !rc->initialized || !content) return SEA_ERR_INVALID_INPUT;
    if (!category) category = "fact";
    if (importance < 1) importance = 1;
    if (importance > 10) importance = 10;

    /* Auto-extract keywords if not provided */
    char auto_kw[1024];
    if (!keywords || !keywords[0]) {
        extract_keywords(content, auto_kw, sizeof(auto_kw));
        keywords = auto_kw;
    }

    /* Check for duplicate: same content already exists */
    const char* dup_sql = "SELECT id FROM recall_facts WHERE content = ? LIMIT 1";
    sqlite3_stmt* dup_stmt;
    if (sqlite3_prepare_v2(rc->db->handle, dup_sql, -1, &dup_stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(dup_stmt, 1, content, -1, SQLITE_STATIC);
        if (sqlite3_step(dup_stmt) == SQLITE_ROW) {
            /* Duplicate found — update accessed_at and access_count instead */
            i32 existing_id = sqlite3_column_int(dup_stmt, 0);
            sqlite3_finalize(dup_stmt);

            const char* update_sql =
                "UPDATE recall_facts SET accessed_at = datetime('now'), "
                "access_count = access_count + 1 WHERE id = ?";
            sqlite3_stmt* upd;
            if (sqlite3_prepare_v2(rc->db->handle, update_sql, -1, &upd, NULL) == SQLITE_OK) {
                sqlite3_bind_int(upd, 1, existing_id);
                sqlite3_step(upd);
                sqlite3_finalize(upd);
            }
            SEA_LOG_INFO("RECALL", "Fact already exists (id=%d), refreshed", existing_id);
            return SEA_OK;
        }
        sqlite3_finalize(dup_stmt);
    }

    const char* sql =
        "INSERT INTO recall_facts (category, content, keywords, importance) "
        "VALUES (?, ?, ?, ?)";

    sqlite3_stmt* stmt;
    int ret = sqlite3_prepare_v2(rc->db->handle, sql, -1, &stmt, NULL);
    if (ret != SQLITE_OK) return SEA_ERR_IO;

    sqlite3_bind_text(stmt, 1, category, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, content, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, keywords, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, importance);

    ret = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (ret != SQLITE_DONE) {
        SEA_LOG_ERROR("RECALL", "Failed to store fact: %s", sqlite3_errmsg(rc->db->handle));
        return SEA_ERR_IO;
    }

    SEA_LOG_INFO("RECALL", "Stored [%s] (%d): %.60s...", category, importance, content);
    return SEA_OK;
}

/* ── Query ────────────────────────────────────────────────── */

i32 sea_recall_query(SeaRecall* rc, const char* query,
                     SeaRecallFact* out, i32 max_results,
                     SeaArena* arena) {
    if (!rc || !rc->initialized || !query || !out || max_results <= 0) return 0;

    /* Extract query keywords */
    char query_kw[1024];
    extract_keywords(query, query_kw, sizeof(query_kw));

    /* Load all facts (we score in C for flexibility) */
    const char* sql =
        "SELECT id, category, content, keywords, importance, "
        "created_at, accessed_at, access_count "
        "FROM recall_facts ORDER BY accessed_at DESC LIMIT 500";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(rc->db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;

    /* Temporary array for scoring */
    #define MAX_CANDIDATES 500
    SeaRecallFact candidates[MAX_CANDIDATES];
    i32 count = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW && count < MAX_CANDIDATES) {
        SeaRecallFact* f = &candidates[count];

        f->id = sqlite3_column_int(stmt, 0);

        /* Copy strings into arena */
        const char* cat = (const char*)sqlite3_column_text(stmt, 1);
        const char* con = (const char*)sqlite3_column_text(stmt, 2);
        const char* kw  = (const char*)sqlite3_column_text(stmt, 3);
        const char* cre = (const char*)sqlite3_column_text(stmt, 5);
        const char* acc = (const char*)sqlite3_column_text(stmt, 6);

        f->category = cat ? (const char*)sea_arena_push_cstr(arena, cat).data : "";
        f->content  = con ? (const char*)sea_arena_push_cstr(arena, con).data : "";
        f->keywords = kw  ? (const char*)sea_arena_push_cstr(arena, kw).data : "";
        f->created_at  = cre ? (const char*)sea_arena_push_cstr(arena, cre).data : "";
        f->accessed_at = acc ? (const char*)sea_arena_push_cstr(arena, acc).data : "";
        f->importance = sqlite3_column_int(stmt, 4);
        f->access_count = sqlite3_column_int(stmt, 7);

        /* Score: keyword_overlap * importance_weight * recency */
        i32 overlap = keyword_overlap(query_kw, f->keywords);
        f64 imp_weight = (f64)f->importance / 10.0;
        f64 recency = recency_score(f->accessed_at);

        /* Base score from keyword overlap */
        f->score = (f64)overlap * 10.0;

        /* Boost by importance and recency */
        f->score *= (0.5 + imp_weight);
        f->score *= recency;

        /* Bonus for high-importance facts even without keyword match */
        if (overlap == 0 && f->importance >= 8) {
            f->score = 2.0 * recency;
        }

        /* Category bonus: "user" and "identity" facts always somewhat relevant */
        if (strcmp(f->category, "user") == 0 || strcmp(f->category, "identity") == 0) {
            f->score += 1.0;
        }

        count++;
    }
    sqlite3_finalize(stmt);

    /* Sort by score descending (simple insertion sort, N <= 500) */
    for (i32 i = 1; i < count; i++) {
        SeaRecallFact tmp = candidates[i];
        i32 j = i - 1;
        while (j >= 0 && candidates[j].score < tmp.score) {
            candidates[j + 1] = candidates[j];
            j--;
        }
        candidates[j + 1] = tmp;
    }

    /* Copy top-N to output */
    i32 result_count = count < max_results ? count : max_results;
    for (i32 i = 0; i < result_count; i++) {
        out[i] = candidates[i];
    }

    /* Update accessed_at for returned facts */
    for (i32 i = 0; i < result_count; i++) {
        if (out[i].score > 0) {
            const char* upd_sql =
                "UPDATE recall_facts SET accessed_at = datetime('now'), "
                "access_count = access_count + 1 WHERE id = ?";
            sqlite3_stmt* upd;
            if (sqlite3_prepare_v2(rc->db->handle, upd_sql, -1, &upd, NULL) == SQLITE_OK) {
                sqlite3_bind_int(upd, 1, out[i].id);
                sqlite3_step(upd);
                sqlite3_finalize(upd);
            }
        }
    }

    return result_count;
}

/* ── Build Context ────────────────────────────────────────── */

const char* sea_recall_build_context(SeaRecall* rc, const char* query,
                                      SeaArena* arena) {
    if (!rc || !rc->initialized || !arena) return NULL;

    /* Query top 20 facts */
    SeaRecallFact facts[20];
    i32 count = sea_recall_query(rc, query ? query : "", facts, 20, arena);
    if (count == 0) return NULL;

    /* Budget: max_context_tokens * 4 chars/token */
    u32 budget = rc->max_context_tokens * 4;
    char* ctx = (char*)sea_arena_alloc(arena, budget, 1);
    if (!ctx) return NULL;

    u32 pos = 0;
    pos += (u32)snprintf(ctx + pos, budget - pos, "[Memory — %d relevant facts]\n", count);

    for (i32 i = 0; i < count && pos < budget - 100; i++) {
        if (facts[i].score <= 0 && i > 3) break; /* Skip zero-score facts after top 3 */

        u32 added = (u32)snprintf(ctx + pos, budget - pos,
            "- [%s] %s\n", facts[i].category, facts[i].content);
        if (pos + added >= budget - 10) break;
        pos += added;
    }

    return pos > 0 ? ctx : NULL;
}

/* ── Forget ───────────────────────────────────────────────── */

SeaError sea_recall_forget(SeaRecall* rc, i32 fact_id) {
    if (!rc || !rc->initialized) return SEA_ERR_INVALID_INPUT;

    const char* sql = "DELETE FROM recall_facts WHERE id = ?";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(rc->db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return SEA_ERR_IO;

    sqlite3_bind_int(stmt, 1, fact_id);
    int ret = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return ret == SQLITE_DONE ? SEA_OK : SEA_ERR_IO;
}

SeaError sea_recall_forget_category(SeaRecall* rc, const char* category) {
    if (!rc || !rc->initialized || !category) return SEA_ERR_INVALID_INPUT;

    const char* sql = "DELETE FROM recall_facts WHERE category = ?";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(rc->db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return SEA_ERR_IO;

    sqlite3_bind_text(stmt, 1, category, -1, SQLITE_STATIC);
    int ret = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return ret == SQLITE_DONE ? SEA_OK : SEA_ERR_IO;
}

/* ── Counts ───────────────────────────────────────────────── */

u32 sea_recall_count(SeaRecall* rc) {
    if (!rc || !rc->initialized) return 0;

    const char* sql = "SELECT COUNT(*) FROM recall_facts";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(rc->db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;

    u32 count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = (u32)sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return count;
}

u32 sea_recall_count_category(SeaRecall* rc, const char* category) {
    if (!rc || !rc->initialized || !category) return 0;

    const char* sql = "SELECT COUNT(*) FROM recall_facts WHERE category = ?";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(rc->db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;

    sqlite3_bind_text(stmt, 1, category, -1, SQLITE_STATIC);
    u32 count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = (u32)sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return count;
}

/* ── Automated Memory Hygiene ─────────────────────────────── */

u32 sea_recall_cleanup(SeaRecall* rc, i32 min_importance, i32 min_access_count, i32 days_old) {
    if (!rc || !rc->initialized) return 0;

    /* Delete facts that are:
     * - Low importance (< min_importance)
     * - Rarely accessed (access_count < min_access_count)
     * - Old (created more than days_old days ago)
     */
    const char* sql = 
        "DELETE FROM recall_facts "
        "WHERE importance < ? "
        "AND access_count < ? "
        "AND created_at < datetime('now', '-' || ? || ' days')";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(rc->db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        SEA_LOG_ERROR("RECALL", "Failed to prepare cleanup: %s", 
                      sqlite3_errmsg(rc->db->handle));
        return 0;
    }

    sqlite3_bind_int(stmt, 1, min_importance);
    sqlite3_bind_int(stmt, 2, min_access_count);
    sqlite3_bind_int(stmt, 3, days_old);

    int rc_step = sqlite3_step(stmt);
    u32 deleted = (u32)sqlite3_changes(rc->db->handle);
    sqlite3_finalize(stmt);

    if (rc_step == SQLITE_DONE && deleted > 0) {
        SEA_LOG_INFO("RECALL", "Memory hygiene: deleted %u low-value facts "
                     "(importance < %d, access_count < %d, age > %d days)",
                     deleted, min_importance, min_access_count, days_old);
    }

    return deleted;
}
