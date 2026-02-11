/*
 * channel_telegram.c — Telegram Channel Adapter
 *
 * Wraps the existing sea_telegram.h polling API into the
 * abstract SeaChannel interface. Messages flow through the bus
 * instead of being handled directly.
 */

#include "seaclaw/sea_channel.h"
#include "seaclaw/sea_telegram.h"
#include "seaclaw/sea_log.h"
#include "seaclaw/sea_shield.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ── Telegram Channel Data ────────────────────────────────── */

typedef struct {
    SeaTelegram  tg;
    const char*  bot_token;
    i64          allowed_chat_id;
    SeaArena     poll_arena;   /* Per-poll arena for temporary data */
} TelegramChannelData;

/* ── Bus-based Telegram handler ───────────────────────────── */
/*
 * This handler is called by sea_telegram_poll when a message arrives.
 * Instead of processing it directly, we publish it to the bus.
 * The response is set to empty — actual responses come from the
 * outbound bus and are sent by the channel's send() method.
 */

/* We need a reference to the bus from within the callback.
 * Store it in a file-scoped variable set during init. */
static SeaBus*    s_tg_bus = NULL;
static SeaChannel* s_tg_channel = NULL;

static SeaError tg_bus_handler(i64 chat_id, SeaSlice text,
                                SeaArena* arena, SeaSlice* response) {
    (void)arena;

    if (!s_tg_bus || !text.data || text.len == 0) {
        *response = SEA_SLICE_LIT("Internal error.");
        return SEA_OK;
    }

    /* Shield check before publishing to bus */
    if (sea_shield_detect_injection(text)) {
        *response = SEA_SLICE_LIT("Rejected: injection detected.");
        SEA_LOG_WARN("TELEGRAM", "Injection detected from chat %lld", (long long)chat_id);
        return SEA_OK;
    }

    /* Build sender_id string */
    char sender[32];
    snprintf(sender, sizeof(sender), "%lld", (long long)chat_id);

    /* Publish to bus — the agent loop will process it */
    SeaError err = sea_bus_publish_inbound(s_tg_bus, SEA_MSG_USER,
                                           "telegram", sender, chat_id,
                                           (const char*)text.data, text.len);
    if (err != SEA_OK) {
        *response = SEA_SLICE_LIT("Error: message queue full.");
        return SEA_OK;
    }

    /* Don't set a response here — the agent will publish outbound
     * which the channel's send() method will deliver.
     * Set empty response so sea_telegram_poll doesn't send anything. */
    *response = SEA_SLICE_EMPTY;
    return SEA_OK;
}

/* ── VTable Methods ───────────────────────────────────────── */

static SeaError tg_init(SeaChannel* ch, SeaBus* bus, SeaArena* arena) {
    (void)arena;
    TelegramChannelData* data = (TelegramChannelData*)ch->impl;
    if (!data || !data->bot_token) return SEA_ERR_CONFIG;

    /* Store bus reference for the callback */
    s_tg_bus = bus;
    s_tg_channel = ch;

    /* Create per-poll arena */
    SeaError err = sea_arena_create(&data->poll_arena, 512 * 1024); /* 512KB */
    if (err != SEA_OK) return err;

    /* Initialize the underlying Telegram bot */
    err = sea_telegram_init(&data->tg, data->bot_token,
                             data->allowed_chat_id, tg_bus_handler,
                             &data->poll_arena);
    if (err != SEA_OK) {
        sea_arena_destroy(&data->poll_arena);
        return err;
    }

    return SEA_OK;
}

static SeaError tg_start(SeaChannel* ch) {
    TelegramChannelData* data = (TelegramChannelData*)ch->impl;

    /* Test connection */
    SeaError err = sea_telegram_get_me(&data->tg, &data->poll_arena);
    sea_arena_reset(&data->poll_arena);
    if (err != SEA_OK) {
        SEA_LOG_ERROR("TELEGRAM", "Connection test failed: %s", sea_error_str(err));
        return err;
    }

    SEA_LOG_INFO("TELEGRAM", "Bot connected successfully");
    return SEA_OK;
}

static SeaError tg_poll(SeaChannel* ch) {
    TelegramChannelData* data = (TelegramChannelData*)ch->impl;

    SeaError err = sea_telegram_poll(&data->tg);
    sea_arena_reset(&data->poll_arena);
    return err;
}

static SeaError tg_send(SeaChannel* ch, i64 chat_id, const char* text, u32 text_len) {
    TelegramChannelData* data = (TelegramChannelData*)ch->impl;

    if (text_len > 0) {
        SeaSlice slice = { .data = (const u8*)text, .len = text_len };
        return sea_telegram_send_slice(&data->tg, chat_id, slice);
    }
    return SEA_OK;
}

static void tg_stop(SeaChannel* ch) {
    TelegramChannelData* data = (TelegramChannelData*)ch->impl;
    data->tg.running = false;
}

static void tg_destroy(SeaChannel* ch) {
    TelegramChannelData* data = (TelegramChannelData*)ch->impl;
    if (data) {
        sea_arena_destroy(&data->poll_arena);
    }
    s_tg_bus = NULL;
    s_tg_channel = NULL;
}

/* ── VTable ───────────────────────────────────────────────── */

static const SeaChannelVTable s_telegram_vtable = {
    .init    = tg_init,
    .start   = tg_start,
    .poll    = tg_poll,
    .send    = tg_send,
    .stop    = tg_stop,
    .destroy = tg_destroy,
};

/* ── Public: Create Telegram Channel ──────────────────────── */

SeaChannel* sea_channel_telegram_create(const char* bot_token,
                                         i64 allowed_chat_id) {
    if (!bot_token) return NULL;

    /* Allocate channel + data together */
    SeaChannel* ch = (SeaChannel*)calloc(1, sizeof(SeaChannel));
    TelegramChannelData* data = (TelegramChannelData*)calloc(1, sizeof(TelegramChannelData));
    if (!ch || !data) {
        free(ch);
        free(data);
        return NULL;
    }

    data->bot_token = bot_token;
    data->allowed_chat_id = allowed_chat_id;

    sea_channel_base_init(ch, "telegram", &s_telegram_vtable, data);
    return ch;
}

/* Free a Telegram channel created with sea_channel_telegram_create. */
void sea_channel_telegram_destroy(SeaChannel* ch) {
    if (!ch) return;
    if (ch->vtable && ch->vtable->destroy) {
        ch->vtable->destroy(ch);
    }
    free(ch->impl);
    free(ch);
}
