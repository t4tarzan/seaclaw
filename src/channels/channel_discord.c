/*
 * channel_discord.c — Discord Bot Channel Adapter
 *
 * Implements the SeaChannel interface for Discord bots.
 * Uses Discord's REST API with long-polling via the Get Gateway
 * endpoint and Messages API. Does NOT use WebSocket (no libws dep).
 *
 * Approach:
 *   - On start: POST /channels/{id}/messages to verify token
 *   - On poll:  GET  /channels/{id}/messages?after={last_id}&limit=10
 *               Process new messages, publish to bus
 *   - On send:  POST /channels/{id}/messages with {"content": "..."}
 *
 * Required env / config:
 *   DISCORD_BOT_TOKEN   — "Bot <token>" from Discord Developer Portal
 *   DISCORD_CHANNEL_ID  — Numeric channel ID to listen/send in
 *
 * Rate limits: Discord allows ~5 req/s per channel. We poll every 2s.
 */

#include "seaclaw/sea_channel.h"
#include "seaclaw/sea_http.h"
#include "seaclaw/sea_log.h"
#include "seaclaw/sea_shield.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/* ── Constants ───────────────────────────────────────────── */

#define DISCORD_API_BASE   "https://discord.com/api/v10"
#define DISCORD_POLL_MS    2000      /* 2s between polls */
#define DISCORD_MSG_LIMIT  10        /* messages fetched per poll */
#define DISCORD_BUF_SIZE   (64*1024) /* 64KB response buffer */
#define DISCORD_BODY_SIZE  (8*1024)  /* 8KB send buffer */

/* ── Channel Data ────────────────────────────────────────── */

typedef struct {
    const char* bot_token;      /* Full auth header value: "Bot <token>" */
    const char* channel_id;     /* Discord channel snowflake (string) */
    char        auth_header[256]; /* "Authorization: Bot <token>" */
    u64         last_message_id;  /* For pagination — only new messages */
    SeaArena    poll_arena;
} DiscordChannelData;

/* File-scope bus reference (same pattern as channel_telegram.c) */
static SeaBus*     s_dc_bus     = NULL;
static SeaChannel* s_dc_channel = NULL;

/* ── Minimal JSON helpers ────────────────────────────────── */

/* Extract first occurrence of "id":"<value>" from JSON, write to out.
 * Returns pointer past the match or NULL. */
static const char* json_extract_str(const char* json, const char* key,
                                     char* out, size_t out_max) {
    if (!json || !key || !out) return NULL;

    /* Build pattern: "key":"  */
    char pat[128];
    snprintf(pat, sizeof(pat), "\"%s\":\"", key);

    const char* p = strstr(json, pat);
    if (!p) return NULL;
    p += strlen(pat);

    size_t i = 0;
    while (*p && *p != '"' && i < out_max - 1) {
        out[i++] = *p++;
    }
    out[i] = '\0';
    return (*p == '"') ? p + 1 : NULL;
}

/* Parse a u64 snowflake from JSON string field. Returns 0 on failure. */
static u64 json_extract_snowflake(const char* json, const char* key) {
    char buf[32];
    if (!json_extract_str(json, key, buf, sizeof(buf))) return 0;
    return (u64)strtoull(buf, NULL, 10);
}

/* Extract the "content" field value from a Discord message JSON object.
 * Discord escapes unicode but we accept raw UTF-8 here. */
static const char* json_extract_content(const char* json, char* out, size_t out_max) {
    return json_extract_str(json, "content", out, out_max);
}

/* Check if the author is a bot: "bot":true in the author object. */
static bool json_author_is_bot(const char* json) {
    /* Look for "author":{..."bot":true...} */
    const char* author = strstr(json, "\"author\":");
    if (!author) return false;
    /* Find the next closing brace for author object */
    const char* bot_flag = strstr(author, "\"bot\":true");
    if (!bot_flag) return false;
    /* Make sure it's inside the author object, not a nested one */
    const char* end_author = strchr(author + 9, '}');
    return (end_author && bot_flag < end_author);
}

/* ── HTTP helpers using curl via popen ───────────────────── */

/* Execute a curl command and capture output into buf (max buf_max bytes).
 * Returns number of bytes read, or -1 on error. */
static int discord_curl(const char* cmd, char* buf, size_t buf_max) {
    FILE* p = popen(cmd, "r");
    if (!p) return -1;

    size_t total = 0;
    size_t n;
    while (total < buf_max - 1 &&
           (n = fread(buf + total, 1, buf_max - 1 - total, p)) > 0) {
        total += n;
    }
    buf[total] = '\0';
    pclose(p);
    return (int)total;
}

/* Build curl GET command for Discord API.
 * auth_header: "Bot <token>" */
static void discord_build_get(char* cmd, size_t cmd_max,
                               const char* auth_header, const char* path) {
    snprintf(cmd, cmd_max,
        "curl -sf -m 10 "
        "-H 'Authorization: %s' "
        "-H 'Content-Type: application/json' "
        "'%s%s'",
        auth_header, DISCORD_API_BASE, path);
}

/* Build curl POST command for Discord API. */
static void discord_build_post(char* cmd, size_t cmd_max,
                                const char* auth_header, const char* path,
                                const char* json_body) {
    snprintf(cmd, cmd_max,
        "curl -sf -m 10 "
        "-X POST "
        "-H 'Authorization: %s' "
        "-H 'Content-Type: application/json' "
        "-d '%s' "
        "'%s%s'",
        auth_header, json_body, DISCORD_API_BASE, path);
}

/* ── VTable Methods ──────────────────────────────────────── */

static SeaError discord_init(SeaChannel* ch, SeaBus* bus, SeaArena* arena) {
    (void)arena;
    DiscordChannelData* d = (DiscordChannelData*)ch->impl;
    if (!d || !d->bot_token || !d->bot_token[0]) {
        SEA_LOG_WARN("DISCORD", "No bot token configured");
        ch->state = SEA_CHAN_ERROR;
        return SEA_ERR_CONFIG;
    }
    if (!d->channel_id || !d->channel_id[0]) {
        SEA_LOG_WARN("DISCORD", "No channel ID configured");
        ch->state = SEA_CHAN_ERROR;
        return SEA_ERR_CONFIG;
    }

    /* Build auth header value */
    snprintf(d->auth_header, sizeof(d->auth_header), "Bot %s", d->bot_token);

    if (sea_arena_create(&d->poll_arena, DISCORD_BUF_SIZE) != SEA_OK) {
        return SEA_ERR_OOM;
    }

    s_dc_bus     = bus;
    s_dc_channel = ch;

    ch->state = SEA_CHAN_STOPPED;
    SEA_LOG_INFO("DISCORD", "Channel initialized (channel_id=%s)", d->channel_id);
    return SEA_OK;
}

static SeaError discord_start(SeaChannel* ch) {
    DiscordChannelData* d = (DiscordChannelData*)ch->impl;

    /* Verify token by fetching channel info */
    char path[128];
    snprintf(path, sizeof(path), "/channels/%s", d->channel_id);

    char cmd[512];
    discord_build_get(cmd, sizeof(cmd), d->auth_header, path);

    char buf[DISCORD_BUF_SIZE];
    int n = discord_curl(cmd, buf, sizeof(buf));
    if (n <= 0) {
        SEA_LOG_ERROR("DISCORD", "Failed to reach Discord API");
        ch->state = SEA_CHAN_ERROR;
        return SEA_ERR_IO;
    }

    /* Check for error response */
    if (strstr(buf, "\"code\":") && strstr(buf, "\"message\":")) {
        SEA_LOG_ERROR("DISCORD", "Discord API error: %.120s", buf);
        ch->state = SEA_CHAN_ERROR;
        return SEA_ERR_CONFIG;
    }

    /* Seed last_message_id from the latest message so we don't replay history */
    char msgs_path[160];
    snprintf(msgs_path, sizeof(msgs_path), "/channels/%s/messages?limit=1", d->channel_id);
    discord_build_get(cmd, sizeof(cmd), d->auth_header, msgs_path);
    n = discord_curl(cmd, buf, sizeof(buf));
    if (n > 0) {
        u64 seed_id = json_extract_snowflake(buf, "id");
        if (seed_id > 0) d->last_message_id = seed_id;
    }

    ch->state = SEA_CHAN_RUNNING;
    SEA_LOG_INFO("DISCORD", "Bot connected (channel=%s, last_id=%llu)",
                 d->channel_id, (unsigned long long)d->last_message_id);
    return SEA_OK;
}

static SeaError discord_poll(SeaChannel* ch) {
    DiscordChannelData* d = (DiscordChannelData*)ch->impl;

    /* Sleep between polls to respect Discord rate limits */
    usleep(DISCORD_POLL_MS * 1000);

    /* GET /channels/{id}/messages?after={last_id}&limit=N */
    char path[256];
    if (d->last_message_id > 0) {
        snprintf(path, sizeof(path),
            "/channels/%s/messages?after=%llu&limit=%d",
            d->channel_id, (unsigned long long)d->last_message_id, DISCORD_MSG_LIMIT);
    } else {
        snprintf(path, sizeof(path),
            "/channels/%s/messages?limit=%d", d->channel_id, DISCORD_MSG_LIMIT);
    }

    char cmd[512];
    discord_build_get(cmd, sizeof(cmd), d->auth_header, path);

    char buf[DISCORD_BUF_SIZE];
    int n = discord_curl(cmd, buf, sizeof(buf));
    if (n <= 0) return SEA_ERR_IO;

    /* Empty array — no new messages */
    if (strncmp(buf, "[]", 2) == 0) return SEA_ERR_TIMEOUT;

    /* Parse array of message objects.
     * Discord returns newest-first when using after=, so iterate in order. */
    const char* p = buf;
    int msg_count = 0;

    while ((p = strstr(p, "{\"id\":\"")) != NULL) {
        /* Extract message fields */
        char msg_id_str[32]  = {0};
        char content[4096]   = {0};

        const char* obj_start = p;

        json_extract_str(obj_start, "id", msg_id_str, sizeof(msg_id_str));
        u64 msg_id = (u64)strtoull(msg_id_str, NULL, 10);

        /* Skip bot messages to prevent echo loops */
        if (json_author_is_bot(obj_start)) {
            if (msg_id > d->last_message_id) d->last_message_id = msg_id;
            p++;
            continue;
        }

        json_extract_content(obj_start, content, sizeof(content));

        if (msg_id > d->last_message_id) d->last_message_id = msg_id;

        if (!content[0]) { p++; continue; }

        /* Shield check */
        SeaSlice text_slice = { .data = (const u8*)content, .len = (u32)strlen(content) };
        if (sea_shield_detect_injection(text_slice)) {
            SEA_LOG_WARN("DISCORD", "Injection detected, dropping message %llu",
                         (unsigned long long)msg_id);
            p++;
            continue;
        }

        /* Publish to bus — agent loop processes it, sends reply via discord_send */
        char sender[32];
        snprintf(sender, sizeof(sender), "%llu", (unsigned long long)msg_id);
        i64 chat_id = (i64)(msg_id & 0x7FFFFFFFFFFFFFFF);

        if (s_dc_bus) {
            sea_bus_publish_inbound(s_dc_bus, SEA_MSG_USER,
                                    "discord", sender, chat_id,
                                    content, (u32)strlen(content));
            msg_count++;
        }
        p++;
    }

    return (msg_count > 0) ? SEA_OK : SEA_ERR_TIMEOUT;
}

static SeaError discord_send(SeaChannel* ch, i64 chat_id, const char* text, u32 text_len) {
    (void)chat_id; /* Discord sends to the configured channel */
    DiscordChannelData* d = (DiscordChannelData*)ch->impl;
    if (!d || !text || text_len == 0) return SEA_OK;

    /* Build JSON: {"content": "..."} with escaping.
     * Discord max message length is 2000 chars. */
    char body[DISCORD_BODY_SIZE];
    u32 bi = 0;
    bi += (u32)snprintf(body + bi, sizeof(body) - bi, "{\"content\":\"");

    u32 chars = 0;
    for (u32 i = 0; i < text_len && bi < sizeof(body) - 16 && chars < 1990; i++, chars++) {
        char c = text[i];
        if      (c == '"')  { body[bi++] = '\\'; body[bi++] = '"';  }
        else if (c == '\\') { body[bi++] = '\\'; body[bi++] = '\\'; }
        else if (c == '\n') { body[bi++] = '\\'; body[bi++] = 'n';  }
        else if (c == '\r') { /* skip */ }
        else if (c == '\t') { body[bi++] = '\\'; body[bi++] = 't';  }
        else if (c == '\'') { body[bi++] = '\\'; body[bi++] = '\''; }
        else                { body[bi++] = c; }
    }
    bi += (u32)snprintf(body + bi, sizeof(body) - bi, "\"}");

    char path[128];
    snprintf(path, sizeof(path), "/channels/%s/messages", d->channel_id);

    char cmd[DISCORD_BODY_SIZE + 512];
    discord_build_post(cmd, sizeof(cmd), d->auth_header, path, body);

    char resp[4096];
    int n = discord_curl(cmd, resp, sizeof(resp));
    if (n <= 0) {
        SEA_LOG_ERROR("DISCORD", "Failed to send message");
        return SEA_ERR_IO;
    }

    /* Check for error */
    if (strstr(resp, "\"code\":") && strstr(resp, "\"message\":")) {
        SEA_LOG_WARN("DISCORD", "Send error: %.120s", resp);
        return SEA_ERR_IO;
    }

    SEA_LOG_DEBUG("DISCORD", "Sent %u chars to channel %s", chars, d->channel_id);
    return SEA_OK;
}

static void discord_stop(SeaChannel* ch) {
    ch->state = SEA_CHAN_STOPPED;
    SEA_LOG_INFO("DISCORD", "Channel stopped");
}

static void discord_destroy(SeaChannel* ch) {
    DiscordChannelData* d = (DiscordChannelData*)ch->impl;
    if (d) {
        sea_arena_destroy(&d->poll_arena);
        free(d);
        ch->impl = NULL;
    }
    s_dc_bus     = NULL;
    s_dc_channel = NULL;
}

/* ── VTable ──────────────────────────────────────────────── */

static const SeaChannelVTable s_discord_vtable = {
    .init    = discord_init,
    .start   = discord_start,
    .poll    = discord_poll,
    .send    = discord_send,
    .stop    = discord_stop,
    .destroy = discord_destroy,
};

/* ── Public Constructor ──────────────────────────────────── */

SeaChannel* sea_channel_discord_create(const char* bot_token,
                                        const char* channel_id) {
    if (!bot_token || !bot_token[0]) return NULL;
    if (!channel_id || !channel_id[0]) return NULL;

    SeaChannel* ch = calloc(1, sizeof(SeaChannel));
    if (!ch) return NULL;

    DiscordChannelData* d = calloc(1, sizeof(DiscordChannelData));
    if (!d) { free(ch); return NULL; }

    d->bot_token  = bot_token;
    d->channel_id = channel_id;

    sea_channel_base_init(ch, "discord", &s_discord_vtable, d);
    ch->enabled = true;

    return ch;
}

void sea_channel_discord_destroy(SeaChannel* ch) {
    if (!ch) return;
    if (ch->vtable && ch->vtable->destroy) ch->vtable->destroy(ch);
    free(ch);
}
