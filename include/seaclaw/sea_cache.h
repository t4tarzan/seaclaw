/*
 * sea_cache.h — LLM Response Caching
 *
 * Cache LLM responses by query hash to avoid redundant API calls.
 * SQLite backend with TTL and hit counting.
 */

#ifndef SEA_CACHE_H
#define SEA_CACHE_H

#include "sea_types.h"

/* Cache configuration */
typedef struct {
    u32 ttl_seconds;      /* Time-to-live in seconds (default: 3600 = 1 hour) */
    u32 max_entries;      /* Max cache entries (default: 1000) */
    bool enabled;         /* Enable/disable caching */
} SeaCacheConfig;

/* Initialize cache system */
SeaError sea_cache_init(const char* db_path, SeaCacheConfig* config);

/* Get cached response for a query hash */
const char* sea_cache_get(const char* query_hash);

/* Store response in cache */
SeaError sea_cache_put(const char* query_hash, const char* response);

/* Clear expired cache entries */
void sea_cache_cleanup(void);

/* Get cache statistics */
void sea_cache_stats(u32* total_entries, u32* total_hits, u32* total_misses);

/* Clear all cache entries */
void sea_cache_clear(void);

#endif /* SEA_CACHE_H */
