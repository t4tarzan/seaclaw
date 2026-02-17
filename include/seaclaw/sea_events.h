/*
 * sea_events.h — Observability Events System
 *
 * Emit structured events for monitoring and debugging.
 * Events stored in SQLite and optionally sent to Unix socket.
 */

#ifndef SEA_EVENTS_H
#define SEA_EVENTS_H

#include "sea_types.h"

/* Event types */
typedef enum {
    SEA_EVENT_TOOL_EXEC = 0,      /* Tool execution started */
    SEA_EVENT_TOOL_SUCCESS,       /* Tool execution succeeded */
    SEA_EVENT_TOOL_FAILED,        /* Tool execution failed */
    SEA_EVENT_LLM_REQUEST,        /* LLM API request sent */
    SEA_EVENT_LLM_RESPONSE,       /* LLM API response received */
    SEA_EVENT_LLM_ERROR,          /* LLM API error */
    SEA_EVENT_SHIELD_BLOCK,       /* Shield blocked an operation */
    SEA_EVENT_RATE_LIMIT,         /* Rate limit triggered */
    SEA_EVENT_CACHE_HIT,          /* Cache hit */
    SEA_EVENT_CACHE_MISS,         /* Cache miss */
    SEA_EVENT_MEMORY_STORE,       /* Memory fact stored */
    SEA_EVENT_MEMORY_RECALL,      /* Memory recall query */
    SEA_EVENT_SSRF_BLOCK,         /* SSRF protection blocked URL */
    SEA_EVENT_RISK_HIGH,          /* High risk command detected */
    SEA_EVENT_SESSION_START,      /* Session started */
    SEA_EVENT_SESSION_END,        /* Session ended */
} SeaEventType;

/* Event data structure */
typedef struct {
    SeaEventType type;
    i64          timestamp;       /* Unix timestamp */
    const char*  data;            /* JSON-formatted event data */
} SeaEvent;

/* Initialize events system */
SeaError sea_events_init(const char* db_path, const char* socket_path);

/* Emit an event */
void sea_events_emit(SeaEventType type, const char* data_json);

/* Get event type name */
const char* sea_events_type_name(SeaEventType type);

/* Query recent events */
i32 sea_events_query(SeaEventType type, i64 since_timestamp, SeaEvent* out, i32 max_results);

/* Clean up old events */
void sea_events_cleanup(i32 days_to_keep);

/* Get event statistics */
void sea_events_stats(u32* total_events, u32* events_by_type);

#endif /* SEA_EVENTS_H */
