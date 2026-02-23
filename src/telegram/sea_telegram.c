/*
 * sea_telegram.c — Telegram Bot implementation
 *
 * Long-polling via Telegram Bot API. Uses sea_http for HTTPS calls
 * and sea_json for parsing responses. All allocations in arena.
 */

#include "seaclaw/sea_telegram.h"
#include "seaclaw/sea_http.h"
#include "seaclaw/sea_json.h"
#include "seaclaw/sea_log.h"

#include <stdio.h>
#include <string.h>

#define TG_API_BASE "https://api.telegram.org/bot"
#define TG_URL_BUF  512
#define TG_MSG_BUF  4096

/* ── URL builder ──────────────────────────────────────────── */

static void build_url(char* buf, u32 bufsize, const char* token, const char* method) {
    snprintf(buf, bufsize, "%s%s/%s", TG_API_BASE, token, method);
}

/* ── Public API ───────────────────────────────────────────── */

SeaError sea_telegram_init(SeaTelegram* tg, const char* bot_token,
                           i64 allowed_chat_id, SeaTelegramHandler handler,
                           SeaArena* arena) {
    if (!tg || !bot_token || !handler || !arena) return SEA_ERR_CONFIG;

    tg->bot_token       = bot_token;
    tg->allowed_chat_id = allowed_chat_id;
    tg->handler         = handler;
    tg->arena           = arena;
    tg->last_update_id  = 0;
    tg->running         = true;

    SEA_LOG_INFO("TELEGRAM", "Bot initialized. Allowed chat: %lld",
                 (long long)allowed_chat_id);
    return SEA_OK;
}

SeaError sea_telegram_get_me(SeaTelegram* tg, SeaArena* arena) {
    char url[TG_URL_BUF];
    build_url(url, sizeof(url), tg->bot_token, "getMe");

    SeaHttpResponse resp;
    SeaError err = sea_http_get(url, arena, &resp);
    if (err != SEA_OK) return err;

    if (resp.status_code != 200) {
        SEA_LOG_ERROR("TELEGRAM", "getMe failed: HTTP %d", resp.status_code);
        return SEA_ERR_CONNECT;
    }

    SeaJsonValue json;
    err = sea_json_parse(resp.body, arena, &json);
    if (err != SEA_OK) return err;

    const SeaJsonValue* result = sea_json_get(&json, "result");
    if (!result) {
        SEA_LOG_ERROR("TELEGRAM", "getMe: no result field");
        return SEA_ERR_PARSE;
    }

    SeaSlice username = sea_json_get_string(result, "username");
    if (username.len > 0) {
        SEA_LOG_INFO("TELEGRAM", "Connected as @%.*s", (int)username.len, (const char*)username.data);
    }

    return SEA_OK;
}

SeaError sea_telegram_send(SeaTelegram* tg, i64 chat_id, const char* text) {
    if (!tg || !text) return SEA_ERR_IO;

    char url[TG_URL_BUF];
    build_url(url, sizeof(url), tg->bot_token, "sendMessage");

    /* Build JSON body */
    char body[TG_MSG_BUF];
    int len = snprintf(body, sizeof(body),
        "{\"chat_id\":%lld,\"text\":\"%s\",\"parse_mode\":\"Markdown\"}",
        (long long)chat_id, text);

    if (len <= 0 || (u32)len >= sizeof(body)) return SEA_ERR_OOM;

    SeaSlice json_body = { .data = (const u8*)body, .len = (u32)len };

    SeaHttpResponse resp;
    u64 saved_offset = tg->arena->offset;
    SeaError err = sea_http_post_json(url, json_body, tg->arena, &resp);

    if (err != SEA_OK) {
        SEA_LOG_ERROR("TELEGRAM", "sendMessage failed: %s", sea_error_str(err));
    } else if (resp.status_code != 200) {
        SEA_LOG_WARN("TELEGRAM", "sendMessage HTTP %d: %.*s",
                     resp.status_code, (int)resp.body.len, (const char*)resp.body.data);
    }

    /* Reset arena to before this call */
    tg->arena->offset = saved_offset;
    return err;
}

SeaError sea_telegram_send_slice(SeaTelegram* tg, i64 chat_id, SeaSlice text) {
    if (!tg || text.len == 0) return SEA_ERR_IO;

    /* Copy to null-terminated buffer */
    char buf[TG_MSG_BUF];
    u32 copy_len = text.len;
    if (copy_len >= sizeof(buf) - 1) copy_len = sizeof(buf) - 2;
    memcpy(buf, text.data, copy_len);
    buf[copy_len] = '\0';

    return sea_telegram_send(tg, chat_id, buf);
}

SeaError sea_telegram_delete_webhook(SeaTelegram* tg) {
    if (!tg) return SEA_ERR_IO;

    char url[TG_URL_BUF];
    build_url(url, sizeof(url), tg->bot_token, "deleteWebhook?drop_pending_updates=true");

    u64 saved = tg->arena->offset;
    SeaHttpResponse resp;
    SeaError err = sea_http_get(url, tg->arena, &resp);
    tg->arena->offset = saved;

    if (err == SEA_OK && resp.status_code == 200) {
        SEA_LOG_INFO("TELEGRAM", "Webhook cleared (drop_pending_updates=true)");
    } else {
        SEA_LOG_WARN("TELEGRAM", "deleteWebhook failed (HTTP %d)",
                     err == SEA_OK ? resp.status_code : 0);
    }
    return err;
}

SeaError sea_telegram_poll(SeaTelegram* tg) {
    if (!tg || !tg->running) return SEA_ERR_IO;

    char url[TG_URL_BUF];
    char params[128];

    if (tg->last_update_id > 0) {
        snprintf(params, sizeof(params),
                 "getUpdates?offset=%lld&timeout=30&limit=10",
                 (long long)(tg->last_update_id + 1));
    } else {
        snprintf(params, sizeof(params),
                 "getUpdates?timeout=30&limit=10");
    }

    build_url(url, sizeof(url), tg->bot_token, params);

    u64 saved_offset = tg->arena->offset;

    SeaHttpResponse resp;
    SeaError err = sea_http_get(url, tg->arena, &resp);
    if (err != SEA_OK) {
        tg->arena->offset = saved_offset;
        return err;
    }

    if (resp.status_code != 200) {
        SEA_LOG_WARN("TELEGRAM", "getUpdates HTTP %d", resp.status_code);
        tg->arena->offset = saved_offset;
        return SEA_ERR_CONNECT;
    }

    /* Parse response */
    SeaJsonValue json;
    err = sea_json_parse(resp.body, tg->arena, &json);
    if (err != SEA_OK) {
        tg->arena->offset = saved_offset;
        return err;
    }

    const SeaJsonValue* result = sea_json_get(&json, "result");
    if (!result || result->type != SEA_JSON_ARRAY) {
        tg->arena->offset = saved_offset;
        return SEA_OK; /* No updates */
    }

    /* Process each update */
    for (u32 i = 0; i < result->array.count; i++) {
        const SeaJsonValue* update = &result->array.items[i];

        /* Track update_id */
        f64 uid = sea_json_get_number(update, "update_id", 0);
        i64 update_id = (i64)uid;
        if (update_id > tg->last_update_id) {
            tg->last_update_id = update_id;
        }

        /* Get message */
        const SeaJsonValue* message = sea_json_get(update, "message");
        if (!message) continue;

        /* Get chat_id */
        const SeaJsonValue* chat = sea_json_get(message, "chat");
        if (!chat) continue;
        i64 chat_id = (i64)sea_json_get_number(chat, "id", 0);

        /* Check allowed chat */
        if (tg->allowed_chat_id != 0 && chat_id != tg->allowed_chat_id) {
            SEA_LOG_WARN("TELEGRAM", "Rejected message from chat %lld (not allowed)",
                         (long long)chat_id);
            continue;
        }

        /* Get text */
        SeaSlice text = sea_json_get_string(message, "text");
        if (text.len == 0) continue;

        /* Get sender info for logging */
        const SeaJsonValue* from = sea_json_get(message, "from");
        SeaSlice fname = from ? sea_json_get_string(from, "first_name") : SEA_SLICE_EMPTY;

        SEA_LOG_INFO("TELEGRAM", "Message from %.*s (chat %lld): %.*s",
                     (int)fname.len, fname.data ? (const char*)fname.data : "?",
                     (long long)chat_id,
                     (int)(text.len > 80 ? 80 : text.len),
                     (const char*)text.data);

        /* Dispatch to handler */
        SeaSlice response = SEA_SLICE_EMPTY;
        err = tg->handler(chat_id, text, tg->arena, &response);

        if (err == SEA_OK && response.len > 0) {
            sea_telegram_send_slice(tg, chat_id, response);
        } else if (err != SEA_OK) {
            char errmsg[256];
            snprintf(errmsg, sizeof(errmsg), "Error: %s", sea_error_str(err));
            sea_telegram_send(tg, chat_id, errmsg);
        }
    }

    /* Reset arena after processing all updates */
    tg->arena->offset = saved_offset;
    return SEA_OK;
}
