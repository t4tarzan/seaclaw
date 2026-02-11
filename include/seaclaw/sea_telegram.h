/*
 * sea_telegram.h — Telegram Bot Interface
 *
 * Long-polling bot that receives commands via Telegram,
 * dispatches them through the agent, and sends responses back.
 * The Telegram interface is a Mirror — it reflects engine state,
 * never calculates.
 */

#ifndef SEA_TELEGRAM_H
#define SEA_TELEGRAM_H

#include "sea_types.h"
#include "sea_arena.h"

/* Callback: called when a message arrives from Telegram.
 * Receives chat_id, user text, and an arena for the response.
 * Must write response text into `response` slice. */
typedef SeaError (*SeaTelegramHandler)(i64 chat_id, SeaSlice text,
                                       SeaArena* arena, SeaSlice* response);

typedef struct {
    const char*        bot_token;
    i64                allowed_chat_id;  /* 0 = allow all */
    SeaTelegramHandler handler;
    SeaArena*          arena;
    i64                last_update_id;
    bool               running;
} SeaTelegram;

/* Initialize Telegram bot. Does NOT start polling. */
SeaError sea_telegram_init(SeaTelegram* tg, const char* bot_token,
                           i64 allowed_chat_id, SeaTelegramHandler handler,
                           SeaArena* arena);

/* Poll for updates once. Call this in the event loop. */
SeaError sea_telegram_poll(SeaTelegram* tg);

/* Send a text message to a chat */
SeaError sea_telegram_send(SeaTelegram* tg, i64 chat_id, const char* text);

/* Send a text message with a SeaSlice (may not be null-terminated) */
SeaError sea_telegram_send_slice(SeaTelegram* tg, i64 chat_id, SeaSlice text);

/* Get bot info (test connection) */
SeaError sea_telegram_get_me(SeaTelegram* tg, SeaArena* arena);

#endif /* SEA_TELEGRAM_H */
