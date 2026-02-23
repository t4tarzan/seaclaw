/*
 * channel_slack.c — Slack Webhook Channel Adapter
 *
 * Outbound-only channel that sends messages to Slack via
 * Incoming Webhook URL. Inbound messages are not supported
 * (would require a Slack app with Events API + HTTP server).
 *
 * Usage: set SLACK_WEBHOOK_URL in .env or config.json
 */

#include "seaclaw/sea_channel.h"
#include "seaclaw/sea_http.h"
#include "seaclaw/sea_log.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ── Slack Channel Data ──────────────────────────────────── */

typedef struct {
    const char* webhook_url;   /* Slack Incoming Webhook URL */
    SeaArena    send_arena;    /* Per-send arena */
} SlackChannelData;

/* ── VTable Implementation ───────────────────────────────── */

static SeaError slack_init(SeaChannel* ch, SeaBus* bus, SeaArena* arena) {
    (void)arena;
    ch->bus = bus;

    SlackChannelData* d = (SlackChannelData*)ch->impl;
    if (!d || !d->webhook_url || !d->webhook_url[0]) {
        SEA_LOG_WARN("SLACK", "No webhook URL configured");
        ch->state = SEA_CHAN_ERROR;
        return SEA_ERR_CONFIG;
    }

    if (sea_arena_create(&d->send_arena, 8192) != SEA_OK) {
        return SEA_ERR_OOM;
    }

    ch->state = SEA_CHAN_STOPPED;
    SEA_LOG_INFO("SLACK", "Channel initialized (webhook: %.40s...)", d->webhook_url);
    return SEA_OK;
}

static SeaError slack_start(SeaChannel* ch) {
    ch->state = SEA_CHAN_RUNNING;
    SEA_LOG_INFO("SLACK", "Channel started (outbound-only)");
    return SEA_OK;
}

static SeaError slack_poll(SeaChannel* ch) {
    (void)ch;
    /* Slack webhook is outbound-only — no polling needed.
     * Return timeout to indicate "no messages" (normal). */
    return SEA_ERR_TIMEOUT;
}

static SeaError slack_send(SeaChannel* ch, i64 chat_id, const char* text, u32 text_len) {
    (void)chat_id; /* Slack webhook doesn't use chat_id */
    SlackChannelData* d = (SlackChannelData*)ch->impl;
    if (!d || !d->webhook_url) return SEA_ERR_CONFIG;

    /* Build JSON payload: {"text": "..."} with escaped text */
    char body[4096];
    u32 bi = 0;
    bi += (u32)snprintf(body + bi, sizeof(body) - bi, "{\"text\":\"");

    for (u32 i = 0; i < text_len && bi < sizeof(body) - 10; i++) {
        char c = text[i];
        if (c == '"')       { body[bi++] = '\\'; body[bi++] = '"'; }
        else if (c == '\\') { body[bi++] = '\\'; body[bi++] = '\\'; }
        else if (c == '\n') { body[bi++] = '\\'; body[bi++] = 'n'; }
        else if (c == '\r') { /* skip */ }
        else if (c == '\t') { body[bi++] = '\\'; body[bi++] = 't'; }
        else                { body[bi++] = c; }
    }
    bi += (u32)snprintf(body + bi, sizeof(body) - bi, "\"}");

    SeaSlice json_body = { .data = (const u8*)body, .len = bi };

    u64 saved = d->send_arena.offset;
    SeaHttpResponse resp;
    SeaError err = sea_http_post_json(d->webhook_url, json_body, &d->send_arena, &resp);
    d->send_arena.offset = saved;

    if (err != SEA_OK) {
        SEA_LOG_ERROR("SLACK", "Webhook POST failed: %s", sea_error_str(err));
        return err;
    }

    if (resp.status_code != 200) {
        SEA_LOG_WARN("SLACK", "Webhook HTTP %d", resp.status_code);
        return SEA_ERR_IO;
    }

    SEA_LOG_DEBUG("SLACK", "Sent %u bytes to webhook", text_len);
    return SEA_OK;
}

static void slack_stop(SeaChannel* ch) {
    ch->state = SEA_CHAN_STOPPED;
    SEA_LOG_INFO("SLACK", "Channel stopped");
}

static void slack_destroy(SeaChannel* ch) {
    SlackChannelData* d = (SlackChannelData*)ch->impl;
    if (d) {
        sea_arena_destroy(&d->send_arena);
        free(d);
        ch->impl = NULL;
    }
}

static const SeaChannelVTable s_slack_vtable = {
    .init    = slack_init,
    .start   = slack_start,
    .poll    = slack_poll,
    .send    = slack_send,
    .stop    = slack_stop,
    .destroy = slack_destroy,
};

/* ── Public Constructor ──────────────────────────────────── */

SeaChannel* sea_channel_slack_create(const char* webhook_url) {
    if (!webhook_url || !webhook_url[0]) return NULL;

    SeaChannel* ch = calloc(1, sizeof(SeaChannel));
    if (!ch) return NULL;

    SlackChannelData* d = calloc(1, sizeof(SlackChannelData));
    if (!d) { free(ch); return NULL; }

    d->webhook_url = webhook_url;

    sea_channel_base_init(ch, "slack", &s_slack_vtable, d);
    ch->enabled = true;

    return ch;
}

void sea_channel_slack_destroy(SeaChannel* ch) {
    if (!ch) return;
    if (ch->vtable && ch->vtable->destroy) ch->vtable->destroy(ch);
    free(ch);
}
