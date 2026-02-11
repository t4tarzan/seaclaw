/*
 * sea_usage.h — Usage Tracking
 *
 * Tracks token consumption per session, per provider, per day.
 * Persisted to SQLite for billing/audit. Lightweight counters.
 *
 * "Every token has a cost. The Vault keeps the ledger."
 */

#ifndef SEA_USAGE_H
#define SEA_USAGE_H

#include "sea_types.h"
#include "sea_db.h"

/* ── Provider Stats ───────────────────────────────────────── */

#define SEA_USAGE_PROVIDER_MAX 8
#define SEA_USAGE_PROVIDER_NAME_MAX 32

typedef struct {
    char  name[SEA_USAGE_PROVIDER_NAME_MAX];
    u64   tokens_in;
    u64   tokens_out;
    u64   requests;
    u64   errors;
} SeaUsageProvider;

/* ── Daily Stats ──────────────────────────────────────────── */

typedef struct {
    u32   date;         /* YYYYMMDD as integer */
    u64   tokens_in;
    u64   tokens_out;
    u64   requests;
    u64   errors;
} SeaUsageDay;

/* ── Usage Tracker ────────────────────────────────────────── */

#define SEA_USAGE_DAYS_MAX 30

typedef struct {
    SeaUsageProvider providers[SEA_USAGE_PROVIDER_MAX];
    u32              provider_count;
    SeaUsageDay      days[SEA_USAGE_DAYS_MAX];
    u32              day_count;
    u64              total_tokens_in;
    u64              total_tokens_out;
    u64              total_requests;
    u64              total_errors;
    SeaDb*           db;
} SeaUsageTracker;

/* ── API ──────────────────────────────────────────────────── */

/* Initialize the usage tracker. Creates DB table if needed. */
SeaError sea_usage_init(SeaUsageTracker* tracker, SeaDb* db);

/* Record a completed request. */
void sea_usage_record(SeaUsageTracker* tracker, const char* provider,
                       u32 tokens_in, u32 tokens_out, bool error);

/* Get stats for a specific provider. Returns NULL if not found. */
const SeaUsageProvider* sea_usage_provider(const SeaUsageTracker* tracker,
                                            const char* provider);

/* Get today's stats. Returns NULL if no activity today. */
const SeaUsageDay* sea_usage_today(const SeaUsageTracker* tracker);

/* Get total token count (in + out). */
u64 sea_usage_total_tokens(const SeaUsageTracker* tracker);

/* Save stats to DB. */
SeaError sea_usage_save(SeaUsageTracker* tracker);

/* Load stats from DB. */
SeaError sea_usage_load(SeaUsageTracker* tracker);

/* Format a human-readable summary into a buffer. Returns length. */
u32 sea_usage_summary(const SeaUsageTracker* tracker, char* buf, u32 buf_size);

#endif /* SEA_USAGE_H */
