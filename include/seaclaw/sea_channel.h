/*
 * sea_channel.h — Abstract Channel Interface
 *
 * Every messaging channel (Telegram, Discord, WhatsApp, etc.)
 * implements this interface. The channel manager starts/stops
 * channels and routes messages through the bus.
 *
 * Channels are Mirrors — they reflect engine state, never calculate.
 *
 * "One interface, many voices. The engine speaks through all."
 */

#ifndef SEA_CHANNEL_H
#define SEA_CHANNEL_H

#include "sea_types.h"
#include "sea_arena.h"
#include "sea_bus.h"

/* ── Channel State ────────────────────────────────────────── */

typedef enum {
    SEA_CHAN_STOPPED = 0,
    SEA_CHAN_STARTING,
    SEA_CHAN_RUNNING,
    SEA_CHAN_ERROR,
} SeaChanState;

/* ── Forward declaration ──────────────────────────────────── */

typedef struct SeaChannel SeaChannel;

/* ── Channel VTable (virtual method table) ────────────────── */

typedef struct {
    /* Initialize the channel with its config. */
    SeaError (*init)(SeaChannel* ch, SeaBus* bus, SeaArena* arena);

    /* Start the channel (begin polling/listening). */
    SeaError (*start)(SeaChannel* ch);

    /* Poll for new messages (called in channel's thread).
     * Should publish inbound messages to the bus.
     * Return SEA_ERR_TIMEOUT if no messages (normal). */
    SeaError (*poll)(SeaChannel* ch);

    /* Send a message to a specific chat on this channel. */
    SeaError (*send)(SeaChannel* ch, i64 chat_id, const char* text, u32 text_len);

    /* Stop the channel gracefully. */
    void (*stop)(SeaChannel* ch);

    /* Destroy and free channel resources. */
    void (*destroy)(SeaChannel* ch);
} SeaChannelVTable;

/* ── Channel Structure ────────────────────────────────────── */

#define SEA_CHAN_NAME_MAX 32

struct SeaChannel {
    char              name[SEA_CHAN_NAME_MAX];  /* "telegram", "discord", etc. */
    SeaChanState      state;
    SeaBus*           bus;                      /* Shared message bus          */
    SeaArena*         arena;                    /* Per-channel arena           */
    const SeaChannelVTable* vtable;             /* Method dispatch             */
    void*             impl;                     /* Channel-specific data       */
    bool              enabled;                  /* From config                 */
};

/* ── Channel Manager ──────────────────────────────────────── */

#define SEA_MAX_CHANNELS 16

typedef struct {
    SeaChannel* channels[SEA_MAX_CHANNELS];
    u32         count;
    SeaBus*     bus;
    bool        running;
} SeaChannelManager;

/* Initialize the channel manager with a shared bus. */
SeaError sea_channel_manager_init(SeaChannelManager* mgr, SeaBus* bus);

/* Register a channel with the manager. Manager does NOT own the channel. */
SeaError sea_channel_manager_register(SeaChannelManager* mgr, SeaChannel* ch);

/* Start all enabled channels. Each channel polls in its own thread. */
SeaError sea_channel_manager_start_all(SeaChannelManager* mgr);

/* Stop all channels gracefully. */
void sea_channel_manager_stop_all(SeaChannelManager* mgr);

/* Get a channel by name. Returns NULL if not found. */
SeaChannel* sea_channel_manager_get(SeaChannelManager* mgr, const char* name);

/* Get list of enabled channel names (for status display). */
u32 sea_channel_manager_enabled_names(SeaChannelManager* mgr,
                                       const char** names, u32 max_names);

/* Outbound dispatcher: reads outbound bus messages and routes
 * them to the correct channel's send() method.
 * Call this in a loop (or dedicated thread). Non-blocking. */
u32 sea_channel_dispatch_outbound(SeaChannelManager* mgr);

/* ── Channel Helpers ──────────────────────────────────────── */

/* Initialize base channel fields. Call from channel init. */
void sea_channel_base_init(SeaChannel* ch, const char* name,
                            const SeaChannelVTable* vtable, void* impl);

#endif /* SEA_CHANNEL_H */
