/*
 * sea_channel.c — Channel Manager Implementation
 *
 * Manages channel lifecycle, starts each channel's poll loop
 * in its own thread, and dispatches outbound messages.
 */

#include "seaclaw/sea_channel.h"
#include "seaclaw/sea_log.h"

#include <string.h>
#include <pthread.h>
#include <unistd.h>

/* ── Channel Base Init ────────────────────────────────────── */

void sea_channel_base_init(SeaChannel* ch, const char* name,
                            const SeaChannelVTable* vtable, void* impl) {
    if (!ch) return;
    memset(ch, 0, sizeof(SeaChannel));
    strncpy(ch->name, name ? name : "unknown", SEA_CHAN_NAME_MAX - 1);
    ch->vtable  = vtable;
    ch->impl    = impl;
    ch->state   = SEA_CHAN_STOPPED;
    ch->enabled = true;
}

/* ── Channel Thread ───────────────────────────────────────── */

typedef struct {
    SeaChannel* channel;
} ChannelThreadArg;

static void* channel_poll_thread(void* arg) {
    ChannelThreadArg* cta = (ChannelThreadArg*)arg;
    SeaChannel* ch = cta->channel;

    SEA_LOG_INFO("CHANNEL", "[%s] Poll thread started", ch->name);
    ch->state = SEA_CHAN_RUNNING;

    while (ch->state == SEA_CHAN_RUNNING) {
        if (!ch->vtable || !ch->vtable->poll) {
            usleep(100000); /* 100ms */
            continue;
        }

        SeaError err = ch->vtable->poll(ch);
        if (err != SEA_OK && err != SEA_ERR_TIMEOUT) {
            SEA_LOG_WARN("CHANNEL", "[%s] Poll error: %s (retrying in 5s)",
                         ch->name, sea_error_str(err));
            sleep(5);
        }
    }

    SEA_LOG_INFO("CHANNEL", "[%s] Poll thread stopped", ch->name);
    return NULL;
}

/* ── Channel Manager ──────────────────────────────────────── */

SeaError sea_channel_manager_init(SeaChannelManager* mgr, SeaBus* bus) {
    if (!mgr || !bus) return SEA_ERR_INVALID_INPUT;
    memset(mgr, 0, sizeof(SeaChannelManager));
    mgr->bus = bus;
    mgr->running = false;
    return SEA_OK;
}

SeaError sea_channel_manager_register(SeaChannelManager* mgr, SeaChannel* ch) {
    if (!mgr || !ch) return SEA_ERR_INVALID_INPUT;
    if (mgr->count >= SEA_MAX_CHANNELS) return SEA_ERR_ARENA_FULL;

    ch->bus = mgr->bus;
    mgr->channels[mgr->count++] = ch;

    SEA_LOG_INFO("CHANNEL", "Registered channel: %s (enabled=%s)",
                 ch->name, ch->enabled ? "yes" : "no");
    return SEA_OK;
}

SeaError sea_channel_manager_start_all(SeaChannelManager* mgr) {
    if (!mgr) return SEA_ERR_INVALID_INPUT;

    mgr->running = true;
    u32 started = 0;

    for (u32 i = 0; i < mgr->count; i++) {
        SeaChannel* ch = mgr->channels[i];
        if (!ch->enabled) continue;

        /* Initialize channel */
        if (ch->vtable && ch->vtable->init) {
            SeaError err = ch->vtable->init(ch, mgr->bus, ch->arena);
            if (err != SEA_OK) {
                SEA_LOG_ERROR("CHANNEL", "[%s] Init failed: %s",
                              ch->name, sea_error_str(err));
                ch->state = SEA_CHAN_ERROR;
                continue;
            }
        }

        /* Start channel */
        if (ch->vtable && ch->vtable->start) {
            SeaError err = ch->vtable->start(ch);
            if (err != SEA_OK) {
                SEA_LOG_ERROR("CHANNEL", "[%s] Start failed: %s",
                              ch->name, sea_error_str(err));
                ch->state = SEA_CHAN_ERROR;
                continue;
            }
        }

        /* Launch poll thread */
        ch->state = SEA_CHAN_STARTING;
        static ChannelThreadArg args[SEA_MAX_CHANNELS];
        args[i].channel = ch;

        pthread_t tid;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

        if (pthread_create(&tid, &attr, channel_poll_thread, &args[i]) != 0) {
            SEA_LOG_ERROR("CHANNEL", "[%s] Failed to create poll thread", ch->name);
            ch->state = SEA_CHAN_ERROR;
            pthread_attr_destroy(&attr);
            continue;
        }
        pthread_attr_destroy(&attr);
        started++;
    }

    SEA_LOG_INFO("CHANNEL", "Started %u/%u channels", started, mgr->count);
    return SEA_OK;
}

void sea_channel_manager_stop_all(SeaChannelManager* mgr) {
    if (!mgr) return;

    mgr->running = false;

    for (u32 i = 0; i < mgr->count; i++) {
        SeaChannel* ch = mgr->channels[i];
        if (ch->state == SEA_CHAN_RUNNING) {
            ch->state = SEA_CHAN_STOPPED;
            if (ch->vtable && ch->vtable->stop) {
                ch->vtable->stop(ch);
            }
            SEA_LOG_INFO("CHANNEL", "[%s] Stopped", ch->name);
        }
    }
}

SeaChannel* sea_channel_manager_get(SeaChannelManager* mgr, const char* name) {
    if (!mgr || !name) return NULL;
    for (u32 i = 0; i < mgr->count; i++) {
        if (strcmp(mgr->channels[i]->name, name) == 0) {
            return mgr->channels[i];
        }
    }
    return NULL;
}

u32 sea_channel_manager_enabled_names(SeaChannelManager* mgr,
                                       const char** names, u32 max_names) {
    if (!mgr || !names) return 0;
    u32 count = 0;
    for (u32 i = 0; i < mgr->count && count < max_names; i++) {
        if (mgr->channels[i]->enabled) {
            names[count++] = mgr->channels[i]->name;
        }
    }
    return count;
}

/* ── Outbound Dispatcher ──────────────────────────────────── */

u32 sea_channel_dispatch_outbound(SeaChannelManager* mgr) {
    if (!mgr || !mgr->bus) return 0;

    u32 dispatched = 0;
    SeaBusMsg msg;

    while (sea_bus_consume_outbound(mgr->bus, &msg) == SEA_OK) {
        /* Find the target channel */
        SeaChannel* ch = sea_channel_manager_get(mgr, msg.channel);
        if (!ch) {
            SEA_LOG_WARN("CHANNEL", "No channel '%s' for outbound message",
                         msg.channel ? msg.channel : "(null)");
            continue;
        }

        if (ch->state != SEA_CHAN_RUNNING) {
            SEA_LOG_WARN("CHANNEL", "[%s] Not running, dropping outbound", ch->name);
            continue;
        }

        if (ch->vtable && ch->vtable->send) {
            SeaError err = ch->vtable->send(ch, msg.chat_id,
                                             msg.content, msg.content_len);
            if (err != SEA_OK) {
                SEA_LOG_ERROR("CHANNEL", "[%s] Send failed: %s",
                              ch->name, sea_error_str(err));
            } else {
                dispatched++;
            }
        }
    }

    return dispatched;
}
