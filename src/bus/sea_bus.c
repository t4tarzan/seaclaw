/*
 * sea_bus.c — Message Bus Implementation
 *
 * Thread-safe circular buffer queues with pthread mutexes.
 * All string data is copied into the bus arena so callers
 * can free their buffers immediately after publishing.
 */

#include "seaclaw/sea_bus.h"
#include "seaclaw/sea_log.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/* ── Helpers ──────────────────────────────────────────────── */

static u64 now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (u64)ts.tv_sec * 1000 + (u64)ts.tv_nsec / 1000000;
}

/* Copy a C string into the bus arena. Returns arena pointer. */
static const char* arena_strdup(SeaArena* arena, const char* src) {
    if (!src) return NULL;
    u32 len = (u32)strlen(src);
    char* dst = (char*)sea_arena_alloc(arena, len + 1, 1);
    if (!dst) return NULL;
    memcpy(dst, src, len);
    dst[len] = '\0';
    return dst;
}

/* Copy content (may not be null-terminated) into arena. */
static const char* arena_copy(SeaArena* arena, const char* src, u32 len) {
    if (!src || len == 0) return NULL;
    char* dst = (char*)sea_arena_alloc(arena, len + 1, 1);
    if (!dst) return NULL;
    memcpy(dst, src, len);
    dst[len] = '\0';
    return dst;
}

/* Build session key "channel:chat_id" */
static const char* build_session_key(SeaArena* arena, const char* channel, i64 chat_id) {
    char buf[128];
    int n = snprintf(buf, sizeof(buf), "%s:%lld",
                     channel ? channel : "unknown", (long long)chat_id);
    if (n <= 0) return NULL;
    return arena_copy(arena, buf, (u32)n);
}

/* ── Init / Destroy ───────────────────────────────────────── */

SeaError sea_bus_init(SeaBus* bus, u64 arena_size) {
    if (!bus) return SEA_ERR_INVALID_INPUT;

    memset(bus, 0, sizeof(SeaBus));

    SeaError err = sea_arena_create(&bus->arena, arena_size);
    if (err != SEA_OK) return err;

    pthread_mutex_init(&bus->in_mutex, NULL);
    pthread_cond_init(&bus->in_cond, NULL);
    pthread_mutex_init(&bus->out_mutex, NULL);
    pthread_cond_init(&bus->out_cond, NULL);

    bus->running = true;

    SEA_LOG_INFO("BUS", "Message bus initialized (arena: %llu bytes)", (unsigned long long)arena_size);
    return SEA_OK;
}

void sea_bus_destroy(SeaBus* bus) {
    if (!bus) return;

    bus->running = false;

    /* Wake any blocked consumers */
    pthread_cond_broadcast(&bus->in_cond);
    pthread_cond_broadcast(&bus->out_cond);

    pthread_mutex_destroy(&bus->in_mutex);
    pthread_cond_destroy(&bus->in_cond);
    pthread_mutex_destroy(&bus->out_mutex);
    pthread_cond_destroy(&bus->out_cond);

    sea_arena_destroy(&bus->arena);

    SEA_LOG_INFO("BUS", "Message bus destroyed");
}

/* ── Publish Inbound ──────────────────────────────────────── */

SeaError sea_bus_publish_inbound(SeaBus* bus, SeaMsgType type,
                                 const char* channel, const char* sender_id,
                                 i64 chat_id, const char* content, u32 content_len) {
    if (!bus || !content) return SEA_ERR_INVALID_INPUT;

    pthread_mutex_lock(&bus->in_mutex);

    if (bus->in_count >= SEA_BUS_QUEUE_SIZE) {
        pthread_mutex_unlock(&bus->in_mutex);
        SEA_LOG_WARN("BUS", "Inbound queue full, dropping message");
        return SEA_ERR_ARENA_FULL;
    }

    SeaBusMsg* msg = &bus->inbound[bus->in_tail];
    msg->type         = type;
    msg->channel      = arena_strdup(&bus->arena, channel);
    msg->sender_id    = arena_strdup(&bus->arena, sender_id);
    msg->chat_id      = chat_id;
    msg->content      = arena_copy(&bus->arena, content, content_len);
    msg->content_len  = content_len;
    msg->session_key  = build_session_key(&bus->arena, channel, chat_id);
    msg->timestamp_ms = now_ms();

    bus->in_tail = (bus->in_tail + 1) % SEA_BUS_QUEUE_SIZE;
    bus->in_count++;

    pthread_cond_signal(&bus->in_cond);
    pthread_mutex_unlock(&bus->in_mutex);

    SEA_LOG_DEBUG("BUS", "Inbound: [%s] chat=%lld len=%u",
                  channel ? channel : "?", (long long)chat_id, content_len);
    return SEA_OK;
}

/* ── Consume Inbound (blocking with timeout) ──────────────── */

SeaError sea_bus_consume_inbound(SeaBus* bus, SeaBusMsg* out, u32 timeout_ms) {
    if (!bus || !out) return SEA_ERR_INVALID_INPUT;

    pthread_mutex_lock(&bus->in_mutex);

    while (bus->in_count == 0 && bus->running) {
        if (timeout_ms == 0) {
            /* Non-blocking */
            pthread_mutex_unlock(&bus->in_mutex);
            return SEA_ERR_NOT_FOUND;
        }

        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec  += timeout_ms / 1000;
        ts.tv_nsec += (timeout_ms % 1000) * 1000000;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000;
        }

        int rc = pthread_cond_timedwait(&bus->in_cond, &bus->in_mutex, &ts);
        if (rc != 0) {
            pthread_mutex_unlock(&bus->in_mutex);
            return SEA_ERR_TIMEOUT;
        }
    }

    if (!bus->running) {
        pthread_mutex_unlock(&bus->in_mutex);
        return SEA_ERR_EOF;
    }

    *out = bus->inbound[bus->in_head];
    bus->in_head = (bus->in_head + 1) % SEA_BUS_QUEUE_SIZE;
    bus->in_count--;

    pthread_mutex_unlock(&bus->in_mutex);
    return SEA_OK;
}

/* ── Publish Outbound ─────────────────────────────────────── */

SeaError sea_bus_publish_outbound(SeaBus* bus, const char* channel,
                                  i64 chat_id, const char* content, u32 content_len) {
    if (!bus || !content) return SEA_ERR_INVALID_INPUT;

    pthread_mutex_lock(&bus->out_mutex);

    if (bus->out_count >= SEA_BUS_QUEUE_SIZE) {
        pthread_mutex_unlock(&bus->out_mutex);
        SEA_LOG_WARN("BUS", "Outbound queue full, dropping message");
        return SEA_ERR_ARENA_FULL;
    }

    SeaBusMsg* msg = &bus->outbound[bus->out_tail];
    msg->type         = SEA_MSG_OUTBOUND;
    msg->channel      = arena_strdup(&bus->arena, channel);
    msg->sender_id    = NULL;
    msg->chat_id      = chat_id;
    msg->content      = arena_copy(&bus->arena, content, content_len);
    msg->content_len  = content_len;
    msg->session_key  = build_session_key(&bus->arena, channel, chat_id);
    msg->timestamp_ms = now_ms();

    bus->out_tail = (bus->out_tail + 1) % SEA_BUS_QUEUE_SIZE;
    bus->out_count++;

    pthread_cond_signal(&bus->out_cond);
    pthread_mutex_unlock(&bus->out_mutex);

    SEA_LOG_DEBUG("BUS", "Outbound: [%s] chat=%lld len=%u",
                  channel ? channel : "?", (long long)chat_id, content_len);
    return SEA_OK;
}

/* ── Consume Outbound (non-blocking) ──────────────────────── */

SeaError sea_bus_consume_outbound(SeaBus* bus, SeaBusMsg* out) {
    if (!bus || !out) return SEA_ERR_INVALID_INPUT;

    pthread_mutex_lock(&bus->out_mutex);

    if (bus->out_count == 0) {
        pthread_mutex_unlock(&bus->out_mutex);
        return SEA_ERR_NOT_FOUND;
    }

    *out = bus->outbound[bus->out_head];
    bus->out_head = (bus->out_head + 1) % SEA_BUS_QUEUE_SIZE;
    bus->out_count--;

    pthread_mutex_unlock(&bus->out_mutex);
    return SEA_OK;
}

/* ── Consume Outbound for specific channel ────────────────── */

SeaError sea_bus_consume_outbound_for(SeaBus* bus, const char* channel, SeaBusMsg* out) {
    if (!bus || !channel || !out) return SEA_ERR_INVALID_INPUT;

    pthread_mutex_lock(&bus->out_mutex);

    /* Scan the queue for a message matching this channel */
    for (u32 i = 0; i < bus->out_count; i++) {
        u32 idx = (bus->out_head + i) % SEA_BUS_QUEUE_SIZE;
        if (bus->outbound[idx].channel &&
            strcmp(bus->outbound[idx].channel, channel) == 0) {
            *out = bus->outbound[idx];

            /* Remove from queue by shifting remaining elements */
            for (u32 j = i; j < bus->out_count - 1; j++) {
                u32 src = (bus->out_head + j + 1) % SEA_BUS_QUEUE_SIZE;
                u32 dst_idx = (bus->out_head + j) % SEA_BUS_QUEUE_SIZE;
                bus->outbound[dst_idx] = bus->outbound[src];
            }
            bus->out_count--;
            if (bus->out_count == 0) {
                bus->out_head = 0;
                bus->out_tail = 0;
            } else {
                bus->out_tail = (bus->out_head + bus->out_count) % SEA_BUS_QUEUE_SIZE;
            }

            pthread_mutex_unlock(&bus->out_mutex);
            return SEA_OK;
        }
    }

    pthread_mutex_unlock(&bus->out_mutex);
    return SEA_ERR_NOT_FOUND;
}

/* ── Utility ──────────────────────────────────────────────── */

void sea_bus_reset_arena(SeaBus* bus) {
    if (!bus) return;
    pthread_mutex_lock(&bus->in_mutex);
    pthread_mutex_lock(&bus->out_mutex);
    sea_arena_reset(&bus->arena);
    pthread_mutex_unlock(&bus->out_mutex);
    pthread_mutex_unlock(&bus->in_mutex);
}

u32 sea_bus_inbound_count(SeaBus* bus) {
    if (!bus) return 0;
    pthread_mutex_lock(&bus->in_mutex);
    u32 c = bus->in_count;
    pthread_mutex_unlock(&bus->in_mutex);
    return c;
}

u32 sea_bus_outbound_count(SeaBus* bus) {
    if (!bus) return 0;
    pthread_mutex_lock(&bus->out_mutex);
    u32 c = bus->out_count;
    pthread_mutex_unlock(&bus->out_mutex);
    return c;
}
