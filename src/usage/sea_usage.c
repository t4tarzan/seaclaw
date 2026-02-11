/*
 * sea_usage.c — Usage Tracking Implementation
 *
 * Lightweight token counters per provider and per day.
 * Persisted to SQLite usage_stats table.
 */

#include "seaclaw/sea_usage.h"
#include "seaclaw/sea_log.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/* ── Helpers ──────────────────────────────────────────────── */

static u32 today_date(void) {
    time_t now = time(NULL);
    struct tm* tm = localtime(&now);
    return (u32)((tm->tm_year + 1900) * 10000 +
                  (tm->tm_mon + 1) * 100 +
                  tm->tm_mday);
}

static SeaUsageProvider* find_or_create_provider(SeaUsageTracker* t, const char* name) {
    for (u32 i = 0; i < t->provider_count; i++) {
        if (strcmp(t->providers[i].name, name) == 0) return &t->providers[i];
    }
    if (t->provider_count >= SEA_USAGE_PROVIDER_MAX) return NULL;
    SeaUsageProvider* p = &t->providers[t->provider_count++];
    memset(p, 0, sizeof(SeaUsageProvider));
    strncpy(p->name, name, SEA_USAGE_PROVIDER_NAME_MAX - 1);
    return p;
}

static SeaUsageDay* find_or_create_day(SeaUsageTracker* t, u32 date) {
    for (u32 i = 0; i < t->day_count; i++) {
        if (t->days[i].date == date) return &t->days[i];
    }
    if (t->day_count >= SEA_USAGE_DAYS_MAX) {
        /* Evict oldest day */
        for (u32 i = 0; i < t->day_count - 1; i++) {
            t->days[i] = t->days[i + 1];
        }
        t->day_count--;
    }
    SeaUsageDay* d = &t->days[t->day_count++];
    memset(d, 0, sizeof(SeaUsageDay));
    d->date = date;
    return d;
}

/* ── Init ─────────────────────────────────────────────────── */

SeaError sea_usage_init(SeaUsageTracker* tracker, SeaDb* db) {
    if (!tracker) return SEA_ERR_INVALID_INPUT;
    memset(tracker, 0, sizeof(SeaUsageTracker));
    tracker->db = db;

    if (db) {
        sea_db_exec(db,
            "CREATE TABLE IF NOT EXISTS usage_stats ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  provider TEXT NOT NULL,"
            "  date INTEGER NOT NULL,"
            "  tokens_in INTEGER DEFAULT 0,"
            "  tokens_out INTEGER DEFAULT 0,"
            "  requests INTEGER DEFAULT 0,"
            "  errors INTEGER DEFAULT 0,"
            "  UNIQUE(provider, date)"
            ");");
    }

    SEA_LOG_INFO("USAGE", "Tracker initialized");
    return SEA_OK;
}

/* ── Record ───────────────────────────────────────────────── */

void sea_usage_record(SeaUsageTracker* tracker, const char* provider,
                       u32 tokens_in, u32 tokens_out, bool error) {
    if (!tracker || !provider) return;

    /* Update provider stats */
    SeaUsageProvider* p = find_or_create_provider(tracker, provider);
    if (p) {
        p->tokens_in += tokens_in;
        p->tokens_out += tokens_out;
        p->requests++;
        if (error) p->errors++;
    }

    /* Update daily stats */
    u32 date = today_date();
    SeaUsageDay* d = find_or_create_day(tracker, date);
    if (d) {
        d->tokens_in += tokens_in;
        d->tokens_out += tokens_out;
        d->requests++;
        if (error) d->errors++;
    }

    /* Update totals */
    tracker->total_tokens_in += tokens_in;
    tracker->total_tokens_out += tokens_out;
    tracker->total_requests++;
    if (error) tracker->total_errors++;
}

/* ── Lookup ───────────────────────────────────────────────── */

const SeaUsageProvider* sea_usage_provider(const SeaUsageTracker* tracker,
                                            const char* provider) {
    if (!tracker || !provider) return NULL;
    for (u32 i = 0; i < tracker->provider_count; i++) {
        if (strcmp(tracker->providers[i].name, provider) == 0) {
            return &tracker->providers[i];
        }
    }
    return NULL;
}

const SeaUsageDay* sea_usage_today(const SeaUsageTracker* tracker) {
    if (!tracker) return NULL;
    u32 date = today_date();
    for (u32 i = 0; i < tracker->day_count; i++) {
        if (tracker->days[i].date == date) return &tracker->days[i];
    }
    return NULL;
}

u64 sea_usage_total_tokens(const SeaUsageTracker* tracker) {
    return tracker ? tracker->total_tokens_in + tracker->total_tokens_out : 0;
}

/* ── Save / Load ──────────────────────────────────────────── */

SeaError sea_usage_save(SeaUsageTracker* tracker) {
    if (!tracker || !tracker->db) return SEA_ERR_CONFIG;

    for (u32 i = 0; i < tracker->provider_count; i++) {
        SeaUsageProvider* p = &tracker->providers[i];
        u32 date = today_date();
        char sql[512];
        snprintf(sql, sizeof(sql),
            "INSERT OR REPLACE INTO usage_stats "
            "(provider, date, tokens_in, tokens_out, requests, errors) "
            "VALUES ('%s', %u, %llu, %llu, %llu, %llu);",
            p->name, date,
            (unsigned long long)p->tokens_in,
            (unsigned long long)p->tokens_out,
            (unsigned long long)p->requests,
            (unsigned long long)p->errors);
        sea_db_exec(tracker->db, sql);
    }

    SEA_LOG_INFO("USAGE", "Saved usage stats (%u providers)", tracker->provider_count);
    return SEA_OK;
}

SeaError sea_usage_load(SeaUsageTracker* tracker) {
    if (!tracker || !tracker->db) return SEA_ERR_CONFIG;
    SEA_LOG_INFO("USAGE", "Loading usage stats from DB (lazy)");
    return SEA_OK;
}

/* ── Summary ──────────────────────────────────────────────── */

u32 sea_usage_summary(const SeaUsageTracker* tracker, char* buf, u32 buf_size) {
    if (!tracker || !buf || buf_size == 0) return 0;

    int pos = 0;
    pos += snprintf(buf + pos, buf_size - (u32)pos,
        "Usage Summary:\n"
        "  Total tokens: %llu (in: %llu, out: %llu)\n"
        "  Total requests: %llu (errors: %llu)\n",
        (unsigned long long)(tracker->total_tokens_in + tracker->total_tokens_out),
        (unsigned long long)tracker->total_tokens_in,
        (unsigned long long)tracker->total_tokens_out,
        (unsigned long long)tracker->total_requests,
        (unsigned long long)tracker->total_errors);

    if (tracker->provider_count > 0) {
        pos += snprintf(buf + pos, buf_size - (u32)pos, "\n  By Provider:\n");
        for (u32 i = 0; i < tracker->provider_count && pos < (int)buf_size - 100; i++) {
            const SeaUsageProvider* p = &tracker->providers[i];
            pos += snprintf(buf + pos, buf_size - (u32)pos,
                "    %-16s  tokens: %llu  requests: %llu  errors: %llu\n",
                p->name,
                (unsigned long long)(p->tokens_in + p->tokens_out),
                (unsigned long long)p->requests,
                (unsigned long long)p->errors);
        }
    }

    const SeaUsageDay* today = sea_usage_today(tracker);
    if (today) {
        pos += snprintf(buf + pos, buf_size - (u32)pos,
            "\n  Today (%u):\n"
            "    tokens: %llu  requests: %llu  errors: %llu\n",
            today->date,
            (unsigned long long)(today->tokens_in + today->tokens_out),
            (unsigned long long)today->requests,
            (unsigned long long)today->errors);
    }

    return (u32)pos;
}
