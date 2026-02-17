/*
 * sea_events.c — Observability Events System
 *
 * SQLite-backed event logging for monitoring and debugging.
 */

#include "seaclaw/sea_events.h"
#include "seaclaw/sea_log.h"
#include <sqlite3.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

/* Global state */
static sqlite3* s_db = NULL;
static const char* s_socket_path = NULL;

/* ── Database schema ──────────────────────────────────────── */

static const char* SCHEMA_SQL = 
    "CREATE TABLE IF NOT EXISTS events ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  type INTEGER NOT NULL,"
    "  timestamp INTEGER NOT NULL,"
    "  data TEXT"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_events_type ON events(type);"
    "CREATE INDEX IF NOT EXISTS idx_events_time ON events(timestamp);";

/* ── Initialization ───────────────────────────────────────── */

SeaError sea_events_init(const char* db_path, const char* socket_path) {
    if (s_db) {
        SEA_LOG_WARN("EVENTS", "Already initialized");
        return SEA_OK;
    }

    int rc = sqlite3_open(db_path, &s_db);
    if (rc != SQLITE_OK) {
        SEA_LOG_ERROR("EVENTS", "Failed to open database: %s", sqlite3_errmsg(s_db));
        return SEA_ERR_IO;
    }

    /* Create schema */
    char* err = NULL;
    rc = sqlite3_exec(s_db, SCHEMA_SQL, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        SEA_LOG_ERROR("EVENTS", "Failed to create schema: %s", err);
        sqlite3_free(err);
        return SEA_ERR_IO;
    }

    s_socket_path = socket_path;
    SEA_LOG_INFO("EVENTS", "Events system initialized: %s", db_path);
    return SEA_OK;
}

/* ── Event type names ─────────────────────────────────────── */

const char* sea_events_type_name(SeaEventType type) {
    switch (type) {
        case SEA_EVENT_TOOL_EXEC:      return "TOOL_EXEC";
        case SEA_EVENT_TOOL_SUCCESS:   return "TOOL_SUCCESS";
        case SEA_EVENT_TOOL_FAILED:    return "TOOL_FAILED";
        case SEA_EVENT_LLM_REQUEST:    return "LLM_REQUEST";
        case SEA_EVENT_LLM_RESPONSE:   return "LLM_RESPONSE";
        case SEA_EVENT_LLM_ERROR:      return "LLM_ERROR";
        case SEA_EVENT_SHIELD_BLOCK:   return "SHIELD_BLOCK";
        case SEA_EVENT_RATE_LIMIT:     return "RATE_LIMIT";
        case SEA_EVENT_CACHE_HIT:      return "CACHE_HIT";
        case SEA_EVENT_CACHE_MISS:     return "CACHE_MISS";
        case SEA_EVENT_MEMORY_STORE:   return "MEMORY_STORE";
        case SEA_EVENT_MEMORY_RECALL:  return "MEMORY_RECALL";
        case SEA_EVENT_SSRF_BLOCK:     return "SSRF_BLOCK";
        case SEA_EVENT_RISK_HIGH:      return "RISK_HIGH";
        case SEA_EVENT_SESSION_START:  return "SESSION_START";
        case SEA_EVENT_SESSION_END:    return "SESSION_END";
        default:                       return "UNKNOWN";
    }
}

/* ── Emit event ───────────────────────────────────────────── */

void sea_events_emit(SeaEventType type, const char* data_json) {
    if (!s_db) return;

    const char* sql = "INSERT INTO events (type, timestamp, data) VALUES (?, ?, ?)";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(s_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        SEA_LOG_ERROR("EVENTS", "Failed to prepare insert: %s", sqlite3_errmsg(s_db));
        return;
    }

    i64 now = (i64)time(NULL);
    sqlite3_bind_int(stmt, 1, (int)type);
    sqlite3_bind_int64(stmt, 2, now);
    sqlite3_bind_text(stmt, 3, data_json ? data_json : "", -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        SEA_LOG_ERROR("EVENTS", "Failed to emit event: %s", sqlite3_errmsg(s_db));
        return;
    }

    SEA_LOG_DEBUG("EVENTS", "Emitted %s: %s", sea_events_type_name(type), 
                  data_json ? data_json : "");

    /* TODO: Send to Unix socket if configured */
    (void)s_socket_path;
}

/* ── Query events ─────────────────────────────────────────── */

i32 sea_events_query(SeaEventType type, i64 since_timestamp, SeaEvent* out, i32 max_results) {
    if (!s_db || !out || max_results <= 0) return 0;

    const char* sql = "SELECT type, timestamp, data FROM events "
                     "WHERE type = ? AND timestamp >= ? "
                     "ORDER BY timestamp DESC LIMIT ?";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(s_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        SEA_LOG_ERROR("EVENTS", "Failed to prepare query: %s", sqlite3_errmsg(s_db));
        return 0;
    }

    sqlite3_bind_int(stmt, 1, (int)type);
    sqlite3_bind_int64(stmt, 2, since_timestamp);
    sqlite3_bind_int(stmt, 3, max_results);

    i32 count = 0;
    while (count < max_results && sqlite3_step(stmt) == SQLITE_ROW) {
        out[count].type = (SeaEventType)sqlite3_column_int(stmt, 0);
        out[count].timestamp = sqlite3_column_int64(stmt, 1);
        const char* data = (const char*)sqlite3_column_text(stmt, 2);
        out[count].data = data ? strdup(data) : NULL;
        count++;
    }

    sqlite3_finalize(stmt);
    return count;
}

/* ── Clean up old events ──────────────────────────────────── */

void sea_events_cleanup(i32 days_to_keep) {
    if (!s_db || days_to_keep <= 0) return;

    i64 cutoff = (i64)time(NULL) - (days_to_keep * 86400);
    const char* sql = "DELETE FROM events WHERE timestamp < ?";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(s_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        SEA_LOG_ERROR("EVENTS", "Failed to prepare cleanup: %s", sqlite3_errmsg(s_db));
        return;
    }

    sqlite3_bind_int64(stmt, 1, cutoff);
    rc = sqlite3_step(stmt);
    
    int deleted = sqlite3_changes(s_db);
    sqlite3_finalize(stmt);

    if (deleted > 0) {
        SEA_LOG_INFO("EVENTS", "Cleaned up %d old events (> %d days)", deleted, days_to_keep);
    }
}

/* ── Get statistics ───────────────────────────────────────── */

void sea_events_stats(u32* total_events, u32* events_by_type) {
    if (!s_db) return;

    if (total_events) {
        const char* sql = "SELECT COUNT(*) FROM events";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(s_db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                *total_events = (u32)sqlite3_column_int(stmt, 0);
            }
            sqlite3_finalize(stmt);
        }
    }

    if (events_by_type) {
        const char* sql = "SELECT type, COUNT(*) FROM events GROUP BY type";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(s_db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int type = sqlite3_column_int(stmt, 0);
                int count = sqlite3_column_int(stmt, 1);
                if (type >= 0 && type < 16) {
                    events_by_type[type] = (u32)count;
                }
            }
            sqlite3_finalize(stmt);
        }
    }
}
