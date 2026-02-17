/*
 * sea_cache.c — LLM Response Caching
 *
 * SQLite-backed response caching with TTL and hit counting.
 */

#include "seaclaw/sea_cache.h"
#include "seaclaw/sea_log.h"
#include <sqlite3.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

/* Default configuration */
#define DEFAULT_TTL_SECONDS 3600      /* 1 hour */
#define DEFAULT_MAX_ENTRIES 1000

/* Global state */
static sqlite3* s_db = NULL;
static SeaCacheConfig s_config = {
    .ttl_seconds = DEFAULT_TTL_SECONDS,
    .max_entries = DEFAULT_MAX_ENTRIES,
    .enabled = true
};
static u32 s_cache_hits = 0;
static u32 s_cache_misses = 0;

/* ── Database schema ──────────────────────────────────────── */

static const char* SCHEMA_SQL = 
    "CREATE TABLE IF NOT EXISTS response_cache ("
    "  query_hash TEXT PRIMARY KEY,"
    "  response TEXT NOT NULL,"
    "  cached_at INTEGER NOT NULL,"
    "  hit_count INTEGER DEFAULT 0,"
    "  last_hit_at INTEGER"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_cache_time ON response_cache(cached_at);";

/* ── Initialization ───────────────────────────────────────── */

SeaError sea_cache_init(const char* db_path, SeaCacheConfig* config) {
    if (s_db) {
        SEA_LOG_WARN("CACHE", "Already initialized");
        return SEA_OK;
    }

    if (config) {
        s_config = *config;
    }

    int rc = sqlite3_open(db_path, &s_db);
    if (rc != SQLITE_OK) {
        SEA_LOG_ERROR("CACHE", "Failed to open database: %s", sqlite3_errmsg(s_db));
        return SEA_ERR_IO;
    }

    /* Create schema */
    char* err = NULL;
    rc = sqlite3_exec(s_db, SCHEMA_SQL, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        SEA_LOG_ERROR("CACHE", "Failed to create schema: %s", err);
        sqlite3_free(err);
        return SEA_ERR_IO;
    }

    SEA_LOG_INFO("CACHE", "Response cache initialized: %s (TTL: %us, max: %u)", 
                 db_path, s_config.ttl_seconds, s_config.max_entries);
    return SEA_OK;
}

/* ── Get cached response ──────────────────────────────────── */

const char* sea_cache_get(const char* query_hash) {
    if (!s_db || !s_config.enabled || !query_hash) {
        s_cache_misses++;
        return NULL;
    }

    time_t now = time(NULL);
    time_t cutoff = now - s_config.ttl_seconds;

    const char* sql = "SELECT response FROM response_cache WHERE query_hash = ? AND cached_at >= ?";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(s_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        SEA_LOG_ERROR("CACHE", "Failed to prepare query: %s", sqlite3_errmsg(s_db));
        s_cache_misses++;
        return NULL;
    }

    sqlite3_bind_text(stmt, 1, query_hash, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, cutoff);

    const char* response = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* cached_response = (const char*)sqlite3_column_text(stmt, 0);
        if (cached_response) {
            response = strdup(cached_response);
            s_cache_hits++;
            
            /* Update hit count */
            const char* update_sql = "UPDATE response_cache SET hit_count = hit_count + 1, last_hit_at = ? WHERE query_hash = ?";
            sqlite3_stmt* update_stmt;
            if (sqlite3_prepare_v2(s_db, update_sql, -1, &update_stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_int64(update_stmt, 1, now);
                sqlite3_bind_text(update_stmt, 2, query_hash, -1, SQLITE_STATIC);
                sqlite3_step(update_stmt);
                sqlite3_finalize(update_stmt);
            }
            
            SEA_LOG_DEBUG("CACHE", "Cache hit for hash: %.16s...", query_hash);
        }
    } else {
        s_cache_misses++;
    }

    sqlite3_finalize(stmt);
    return response;
}

/* ── Store response in cache ──────────────────────────────── */

SeaError sea_cache_put(const char* query_hash, const char* response) {
    if (!s_db || !s_config.enabled || !query_hash || !response) {
        return SEA_OK;
    }

    /* Check if cache is full */
    const char* count_sql = "SELECT COUNT(*) FROM response_cache";
    sqlite3_stmt* count_stmt;
    u32 entry_count = 0;
    
    if (sqlite3_prepare_v2(s_db, count_sql, -1, &count_stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(count_stmt) == SQLITE_ROW) {
            entry_count = (u32)sqlite3_column_int(count_stmt, 0);
        }
        sqlite3_finalize(count_stmt);
    }

    /* If full, delete oldest entries */
    if (entry_count >= s_config.max_entries) {
        const char* delete_sql = "DELETE FROM response_cache WHERE query_hash IN "
                                "(SELECT query_hash FROM response_cache ORDER BY cached_at ASC LIMIT 100)";
        sqlite3_exec(s_db, delete_sql, NULL, NULL, NULL);
        SEA_LOG_DEBUG("CACHE", "Evicted 100 oldest entries (cache full)");
    }

    /* Insert or replace */
    const char* sql = "INSERT OR REPLACE INTO response_cache (query_hash, response, cached_at, hit_count) "
                     "VALUES (?, ?, ?, 0)";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(s_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        SEA_LOG_ERROR("CACHE", "Failed to prepare insert: %s", sqlite3_errmsg(s_db));
        return SEA_ERR_IO;
    }

    time_t now = time(NULL);
    sqlite3_bind_text(stmt, 1, query_hash, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, response, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, now);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        SEA_LOG_ERROR("CACHE", "Failed to cache response: %s", sqlite3_errmsg(s_db));
        return SEA_ERR_IO;
    }

    SEA_LOG_DEBUG("CACHE", "Cached response for hash: %.16s...", query_hash);
    return SEA_OK;
}

/* ── Clean up expired entries ─────────────────────────────── */

void sea_cache_cleanup(void) {
    if (!s_db) return;

    time_t cutoff = time(NULL) - s_config.ttl_seconds;
    const char* sql = "DELETE FROM response_cache WHERE cached_at < ?";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(s_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        SEA_LOG_ERROR("CACHE", "Failed to prepare cleanup: %s", sqlite3_errmsg(s_db));
        return;
    }

    sqlite3_bind_int64(stmt, 1, cutoff);
    rc = sqlite3_step(stmt);
    
    int deleted = sqlite3_changes(s_db);
    sqlite3_finalize(stmt);

    if (deleted > 0) {
        SEA_LOG_INFO("CACHE", "Cleaned up %d expired entries", deleted);
    }
}

/* ── Get cache statistics ─────────────────────────────────── */

void sea_cache_stats(u32* total_entries, u32* total_hits, u32* total_misses) {
    if (total_entries) {
        *total_entries = 0;
        if (s_db) {
            const char* sql = "SELECT COUNT(*) FROM response_cache";
            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(s_db, sql, -1, &stmt, NULL) == SQLITE_OK) {
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    *total_entries = (u32)sqlite3_column_int(stmt, 0);
                }
                sqlite3_finalize(stmt);
            }
        }
    }
    if (total_hits) *total_hits = s_cache_hits;
    if (total_misses) *total_misses = s_cache_misses;
}

/* ── Clear all cache entries ──────────────────────────────── */

void sea_cache_clear(void) {
    if (!s_db) return;

    const char* sql = "DELETE FROM response_cache";
    sqlite3_exec(s_db, sql, NULL, NULL, NULL);
    
    SEA_LOG_INFO("CACHE", "Cache cleared");
}
