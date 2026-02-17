/*
 * sea_rate_limit.c — Tool Execution Rate Limiting
 *
 * SQLite-backed rate limiting with sliding window per hour.
 */

#include "seaclaw/sea_rate_limit.h"
#include "seaclaw/sea_log.h"
#include "seaclaw/sea_db.h"
#include <sqlite3.h>
#include <string.h>
#include <time.h>

/* Default rate limits */
#define DEFAULT_MAX_PER_HOUR 100
#define DEFAULT_MAX_PER_DAY  1000

/* Global state */
static sqlite3* s_db = NULL;
static SeaRateLimitConfig s_custom_limits[64];
static u32 s_custom_limit_count = 0;

/* ── Database schema ──────────────────────────────────────── */

static const char* SCHEMA_SQL = 
    "CREATE TABLE IF NOT EXISTS rate_limits ("
    "  tool_name TEXT NOT NULL,"
    "  timestamp INTEGER NOT NULL,"
    "  PRIMARY KEY (tool_name, timestamp)"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_rate_limits_tool ON rate_limits(tool_name);"
    "CREATE INDEX IF NOT EXISTS idx_rate_limits_time ON rate_limits(timestamp);";

/* ── Initialization ───────────────────────────────────────── */

SeaError sea_rate_limit_init(const char* db_path) {
    if (s_db) {
        SEA_LOG_WARN("RATE_LIMIT", "Already initialized");
        return SEA_OK;
    }

    int rc = sqlite3_open(db_path, &s_db);
    if (rc != SQLITE_OK) {
        SEA_LOG_ERROR("RATE_LIMIT", "Failed to open database: %s", sqlite3_errmsg(s_db));
        return SEA_ERR_IO;
    }

    /* Create schema */
    char* err = NULL;
    rc = sqlite3_exec(s_db, SCHEMA_SQL, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        SEA_LOG_ERROR("RATE_LIMIT", "Failed to create schema: %s", err);
        sqlite3_free(err);
        return SEA_ERR_IO;
    }

    SEA_LOG_INFO("RATE_LIMIT", "Rate limiting initialized: %s", db_path);
    return SEA_OK;
}

/* ── Get rate limit configuration ─────────────────────────── */

static void get_limits(const char* tool_name, u32* max_hour, u32* max_day) {
    /* Check custom limits */
    for (u32 i = 0; i < s_custom_limit_count; i++) {
        if (strcmp(s_custom_limits[i].tool_name, tool_name) == 0) {
            *max_hour = s_custom_limits[i].max_per_hour;
            *max_day = s_custom_limits[i].max_per_day;
            return;
        }
    }

    /* Default limits based on tool type */
    if (strstr(tool_name, "shell_exec") || strstr(tool_name, "spawn")) {
        *max_hour = 50;   /* Shell commands: more restricted */
        *max_day = 500;
    } else if (strstr(tool_name, "web_") || strstr(tool_name, "http_")) {
        *max_hour = 100;  /* Web requests: moderate */
        *max_day = 1000;
    } else if (strstr(tool_name, "file_write") || strstr(tool_name, "edit_file")) {
        *max_hour = 50;   /* Write operations: restricted */
        *max_day = 500;
    } else {
        *max_hour = DEFAULT_MAX_PER_HOUR;
        *max_day = DEFAULT_MAX_PER_DAY;
    }
}

/* ── Get current usage count ──────────────────────────────── */

u32 sea_rate_limit_get_count(const char* tool_name, u32 window_hours) {
    if (!s_db || !tool_name) return 0;

    time_t now = time(NULL);
    time_t window_start = now - (window_hours * 3600);

    const char* sql = "SELECT COUNT(*) FROM rate_limits WHERE tool_name = ? AND timestamp >= ?";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(s_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        SEA_LOG_ERROR("RATE_LIMIT", "Failed to prepare query: %s", sqlite3_errmsg(s_db));
        return 0;
    }

    sqlite3_bind_text(stmt, 1, tool_name, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, window_start);

    u32 count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = (u32)sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return count;
}

/* ── Check if tool execution is allowed ───────────────────── */

bool sea_rate_limit_check(const char* tool_name) {
    if (!s_db || !tool_name) return true;  /* Allow if not initialized */

    u32 max_hour, max_day;
    get_limits(tool_name, &max_hour, &max_day);

    /* Check hourly limit */
    u32 count_hour = sea_rate_limit_get_count(tool_name, 1);
    if (count_hour >= max_hour) {
        SEA_LOG_WARN("RATE_LIMIT", "Hourly limit exceeded for %s: %u/%u", 
                     tool_name, count_hour, max_hour);
        return false;
    }

    /* Check daily limit */
    if (max_day > 0) {
        u32 count_day = sea_rate_limit_get_count(tool_name, 24);
        if (count_day >= max_day) {
            SEA_LOG_WARN("RATE_LIMIT", "Daily limit exceeded for %s: %u/%u", 
                         tool_name, count_day, max_day);
            return false;
        }
    }

    return true;
}

/* ── Record a tool execution ──────────────────────────────── */

SeaError sea_rate_limit_record(const char* tool_name) {
    if (!s_db || !tool_name) return SEA_OK;

    const char* sql = "INSERT INTO rate_limits (tool_name, timestamp) VALUES (?, ?)";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(s_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        SEA_LOG_ERROR("RATE_LIMIT", "Failed to prepare insert: %s", sqlite3_errmsg(s_db));
        return SEA_ERR_IO;
    }

    time_t now = time(NULL);
    sqlite3_bind_text(stmt, 1, tool_name, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, now);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        SEA_LOG_ERROR("RATE_LIMIT", "Failed to record execution: %s", sqlite3_errmsg(s_db));
        return SEA_ERR_IO;
    }

    SEA_LOG_DEBUG("RATE_LIMIT", "Recorded execution: %s", tool_name);
    return SEA_OK;
}

/* ── Clean up old records ─────────────────────────────────── */

void sea_rate_limit_cleanup(void) {
    if (!s_db) return;

    /* Delete records older than 7 days */
    time_t cutoff = time(NULL) - (7 * 24 * 3600);
    const char* sql = "DELETE FROM rate_limits WHERE timestamp < ?";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(s_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        SEA_LOG_ERROR("RATE_LIMIT", "Failed to prepare cleanup: %s", sqlite3_errmsg(s_db));
        return;
    }

    sqlite3_bind_int64(stmt, 1, cutoff);
    rc = sqlite3_step(stmt);
    
    int deleted = sqlite3_changes(s_db);
    sqlite3_finalize(stmt);

    if (deleted > 0) {
        SEA_LOG_INFO("RATE_LIMIT", "Cleaned up %d old records", deleted);
    }
}

/* ── Set custom rate limit ────────────────────────────────── */

void sea_rate_limit_set(const char* tool_name, u32 max_per_hour, u32 max_per_day) {
    if (!tool_name || s_custom_limit_count >= 64) return;

    /* Check if already exists */
    for (u32 i = 0; i < s_custom_limit_count; i++) {
        if (strcmp(s_custom_limits[i].tool_name, tool_name) == 0) {
            s_custom_limits[i].max_per_hour = max_per_hour;
            s_custom_limits[i].max_per_day = max_per_day;
            SEA_LOG_INFO("RATE_LIMIT", "Updated limit for %s: %u/hour, %u/day", 
                         tool_name, max_per_hour, max_per_day);
            return;
        }
    }

    /* Add new */
    s_custom_limits[s_custom_limit_count].tool_name = tool_name;
    s_custom_limits[s_custom_limit_count].max_per_hour = max_per_hour;
    s_custom_limits[s_custom_limit_count].max_per_day = max_per_day;
    s_custom_limit_count++;

    SEA_LOG_INFO("RATE_LIMIT", "Set limit for %s: %u/hour, %u/day", 
                 tool_name, max_per_hour, max_per_day);
}

/* ── Get rate limit info ──────────────────────────────────── */

bool sea_rate_limit_get_info(const char* tool_name, u32* current_hour, u32* max_hour, 
                              u32* current_day, u32* max_day) {
    if (!tool_name) return false;

    get_limits(tool_name, max_hour, max_day);
    *current_hour = sea_rate_limit_get_count(tool_name, 1);
    *current_day = sea_rate_limit_get_count(tool_name, 24);

    return true;
}
