/*
 * sea_bus.h — Message Bus
 *
 * Thread-safe pub/sub message bus that decouples channels
 * from the agent loop. Channels publish inbound messages,
 * the agent consumes them, processes, and publishes outbound.
 *
 * All message data is copied into the bus arena.
 * The bus owns the memory until messages are consumed.
 *
 * "The nervous system — signals flow, organs respond."
 */

#ifndef SEA_BUS_H
#define SEA_BUS_H

#include "sea_types.h"
#include "sea_arena.h"
#include <pthread.h>

/* ── Message Types ────────────────────────────────────────── */

typedef enum {
    SEA_MSG_USER = 0,       /* User message from a channel          */
    SEA_MSG_SYSTEM,         /* System message (cron, heartbeat)     */
    SEA_MSG_TOOL_RESULT,    /* Tool execution result                */
    SEA_MSG_OUTBOUND,       /* Response to send back to channel     */
} SeaMsgType;

/* ── Bus Message ──────────────────────────────────────────── */

typedef struct {
    SeaMsgType  type;
    const char* channel;      /* Channel name: "telegram", "discord", etc. */
    const char* sender_id;    /* Sender identifier (user ID as string)     */
    i64         chat_id;      /* Chat/conversation ID                      */
    const char* content;      /* Message text (arena-allocated)            */
    u32         content_len;  /* Length of content                         */
    const char* session_key;  /* Session key: "channel:chat_id"            */
    u64         timestamp_ms; /* When the message was created              */
} SeaBusMsg;

/* ── Bus Configuration ────────────────────────────────────── */

#define SEA_BUS_QUEUE_SIZE 256  /* Max messages in each queue */

/* ── Bus Structure ────────────────────────────────────────── */

typedef struct {
    /* Inbound queue: channels → agent */
    SeaBusMsg   inbound[SEA_BUS_QUEUE_SIZE];
    u32         in_head;
    u32         in_tail;
    u32         in_count;
    pthread_mutex_t in_mutex;
    pthread_cond_t  in_cond;

    /* Outbound queue: agent → channels */
    SeaBusMsg   outbound[SEA_BUS_QUEUE_SIZE];
    u32         out_head;
    u32         out_tail;
    u32         out_count;
    pthread_mutex_t out_mutex;
    pthread_cond_t  out_cond;

    /* Arena for message data (strings are copied here) */
    SeaArena    arena;
    bool        running;
} SeaBus;

/* ── API ──────────────────────────────────────────────────── */

/* Initialize the message bus. arena_size is bytes for message data. */
SeaError sea_bus_init(SeaBus* bus, u64 arena_size);

/* Destroy the bus and free all resources. */
void sea_bus_destroy(SeaBus* bus);

/* Publish an inbound message (channel → agent).
 * Content is copied into the bus arena. Thread-safe. */
SeaError sea_bus_publish_inbound(SeaBus* bus, SeaMsgType type,
                                 const char* channel, const char* sender_id,
                                 i64 chat_id, const char* content, u32 content_len);

/* Consume an inbound message (blocking with timeout).
 * Returns SEA_OK if a message was consumed, SEA_ERR_TIMEOUT if timed out.
 * The message data is valid until the next arena reset. */
SeaError sea_bus_consume_inbound(SeaBus* bus, SeaBusMsg* out, u32 timeout_ms);

/* Publish an outbound message (agent → channel).
 * Content is copied into the bus arena. Thread-safe. */
SeaError sea_bus_publish_outbound(SeaBus* bus, const char* channel,
                                  i64 chat_id, const char* content, u32 content_len);

/* Consume an outbound message (non-blocking).
 * Returns SEA_OK if a message was consumed, SEA_ERR_NOT_FOUND if empty. */
SeaError sea_bus_consume_outbound(SeaBus* bus, SeaBusMsg* out);

/* Consume outbound for a specific channel (non-blocking).
 * Returns SEA_OK if found, SEA_ERR_NOT_FOUND if no message for this channel. */
SeaError sea_bus_consume_outbound_for(SeaBus* bus, const char* channel, SeaBusMsg* out);

/* Reset the bus arena (call periodically to reclaim memory). */
void sea_bus_reset_arena(SeaBus* bus);

/* Get queue depths for monitoring. */
u32 sea_bus_inbound_count(SeaBus* bus);
u32 sea_bus_outbound_count(SeaBus* bus);

#endif /* SEA_BUS_H */
