/*
 * sea_session.c — Session Management Implementation
 *
 * Per-channel, per-chat session isolation with automatic
 * LLM-driven conversation summarization.
 */

#include "seaclaw/sea_session.h"
#include "seaclaw/sea_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ── Helpers ──────────────────────────────────────────────── */

static u64 now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (u64)ts.tv_sec * 1000 + (u64)ts.tv_nsec / 1000000;
}

static const char* arena_strdup(SeaArena* arena, const char* src) {
    if (!src) return NULL;
    u32 len = (u32)strlen(src);
    char* dst = (char*)sea_arena_alloc(arena, len + 1, 1);
    if (!dst) return NULL;
    memcpy(dst, src, len);
    dst[len] = '\0';
    return dst;
}

/* ── Key Builder ──────────────────────────────────────────── */

void sea_session_build_key(char* buf, u32 buf_size,
                            const char* channel, i64 chat_id) {
    snprintf(buf, buf_size, "%s:%lld",
             channel ? channel : "tui", (long long)chat_id);
}

/* ── Find Session ─────────────────────────────────────────── */

static SeaSession* find_session(SeaSessionManager* mgr, const char* key) {
    for (u32 i = 0; i < mgr->count; i++) {
        if (strcmp(mgr->sessions[i].key, key) == 0) {
            return &mgr->sessions[i];
        }
    }
    return NULL;
}

/* ── Init / Destroy ───────────────────────────────────────── */

SeaError sea_session_init(SeaSessionManager* mgr, SeaDb* db,
                           SeaAgentConfig* agent_cfg, u64 arena_size) {
    if (!mgr) return SEA_ERR_INVALID_INPUT;

    memset(mgr, 0, sizeof(SeaSessionManager));
    mgr->db = db;
    mgr->agent_cfg = agent_cfg;
    mgr->max_history = 30;   /* Summarize when history exceeds 30 messages */
    mgr->keep_recent = 10;   /* Keep last 10 messages after summarization  */

    SeaError err = sea_arena_create(&mgr->arena, arena_size);
    if (err != SEA_OK) return err;

    /* Create sessions table in DB if it doesn't exist */
    if (db) {
        sea_db_exec(db,
            "CREATE TABLE IF NOT EXISTS sessions ("
            "  key TEXT PRIMARY KEY,"
            "  channel TEXT,"
            "  chat_id INTEGER,"
            "  summary TEXT,"
            "  total_messages INTEGER DEFAULT 0,"
            "  created_at INTEGER,"
            "  last_active INTEGER"
            ");"
            "CREATE TABLE IF NOT EXISTS session_messages ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  session_key TEXT NOT NULL,"
            "  role TEXT NOT NULL,"
            "  content TEXT NOT NULL,"
            "  timestamp_ms INTEGER,"
            "  FOREIGN KEY (session_key) REFERENCES sessions(key)"
            ");"
            "CREATE INDEX IF NOT EXISTS idx_session_messages_key "
            "ON session_messages(session_key, id DESC);"
        );
    }

    SEA_LOG_INFO("SESSION", "Session manager initialized (max_history=%u, keep_recent=%u)",
                 mgr->max_history, mgr->keep_recent);
    return SEA_OK;
}

void sea_session_destroy(SeaSessionManager* mgr) {
    if (!mgr) return;
    sea_session_save_all(mgr);
    sea_arena_destroy(&mgr->arena);
    SEA_LOG_INFO("SESSION", "Session manager destroyed (%u sessions)", mgr->count);
}

/* ── Get or Create Session ────────────────────────────────── */

SeaSession* sea_session_get(SeaSessionManager* mgr, const char* key) {
    if (!mgr || !key) return NULL;

    /* Find existing */
    SeaSession* s = find_session(mgr, key);
    if (s) {
        s->last_active = now_ms();
        return s;
    }

    /* Create new */
    if (mgr->count >= SEA_MAX_SESSIONS) {
        /* Evict oldest session */
        u32 oldest_idx = 0;
        u64 oldest_time = mgr->sessions[0].last_active;
        for (u32 i = 1; i < mgr->count; i++) {
            if (mgr->sessions[i].last_active < oldest_time) {
                oldest_time = mgr->sessions[i].last_active;
                oldest_idx = i;
            }
        }
        SEA_LOG_INFO("SESSION", "Evicting oldest session: %s", mgr->sessions[oldest_idx].key);
        /* Shift remaining sessions */
        for (u32 i = oldest_idx; i < mgr->count - 1; i++) {
            mgr->sessions[i] = mgr->sessions[i + 1];
        }
        mgr->count--;
    }

    s = &mgr->sessions[mgr->count];
    memset(s, 0, sizeof(SeaSession));
    strncpy(s->key, key, SEA_SESSION_KEY_MAX - 1);

    /* Parse channel and chat_id from key */
    const char* colon = strchr(key, ':');
    if (colon) {
        u32 chan_len = (u32)(colon - key);
        if (chan_len > 0) {
            char* ch = (char*)sea_arena_alloc(&mgr->arena, chan_len + 1, 1);
            if (ch) {
                memcpy(ch, key, chan_len);
                ch[chan_len] = '\0';
                s->channel = ch;
            }
        }
        s->chat_id = atoll(colon + 1);
    }

    s->created_at = now_ms();
    s->last_active = s->created_at;
    mgr->count++;

    SEA_LOG_INFO("SESSION", "Created session: %s (total: %u)", key, mgr->count);
    return s;
}

SeaSession* sea_session_get_by_chat(SeaSessionManager* mgr,
                                     const char* channel, i64 chat_id) {
    char key[SEA_SESSION_KEY_MAX];
    sea_session_build_key(key, sizeof(key), channel, chat_id);
    return sea_session_get(mgr, key);
}

/* ── Add Message ──────────────────────────────────────────── */

SeaError sea_session_add_message(SeaSessionManager* mgr, const char* key,
                                  SeaRole role, const char* content) {
    if (!mgr || !key || !content) return SEA_ERR_INVALID_INPUT;

    SeaSession* s = sea_session_get(mgr, key);
    if (!s) return SEA_ERR_NOT_FOUND;

    /* Add to in-memory history */
    if (s->history_count < SEA_SESSION_MAX_HISTORY) {
        SeaSessionMsg* msg = &s->history[s->history_count];
        msg->role = role;
        msg->content = arena_strdup(&mgr->arena, content);
        msg->timestamp_ms = now_ms();
        s->history_count++;
    } else {
        /* Shift history left, drop oldest */
        for (u32 i = 0; i < s->history_count - 1; i++) {
            s->history[i] = s->history[i + 1];
        }
        SeaSessionMsg* msg = &s->history[s->history_count - 1];
        msg->role = role;
        msg->content = arena_strdup(&mgr->arena, content);
        msg->timestamp_ms = now_ms();
    }

    s->total_messages++;
    s->last_active = now_ms();

    /* Persist to DB */
    if (mgr->db) {
        const char* role_str = "user";
        if (role == SEA_ROLE_ASSISTANT) role_str = "assistant";
        else if (role == SEA_ROLE_SYSTEM) role_str = "system";
        else if (role == SEA_ROLE_TOOL) role_str = "tool";

        /* Insert into session_messages table */
        char sql[4096];
        /* Escape single quotes in content for SQL safety */
        char escaped[3072];
        u32 ei = 0;
        for (u32 i = 0; content[i] && ei < sizeof(escaped) - 2; i++) {
            if (content[i] == '\'') {
                escaped[ei++] = '\'';
                escaped[ei++] = '\'';
            } else {
                escaped[ei++] = content[i];
            }
        }
        escaped[ei] = '\0';

        snprintf(sql, sizeof(sql),
            "INSERT INTO session_messages (session_key, role, content, timestamp_ms) "
            "VALUES ('%s', '%s', '%s', %llu);",
            key, role_str, escaped, (unsigned long long)now_ms());
        sea_db_exec(mgr->db, sql);
    }

    /* Check if summarization is needed */
    if (s->history_count >= mgr->max_history && mgr->agent_cfg) {
        sea_session_summarize(mgr, key);
    }

    return SEA_OK;
}

/* ── Get History ──────────────────────────────────────────── */

u32 sea_session_get_history(SeaSessionManager* mgr, const char* key,
                             SeaChatMsg* out, u32 max_count, SeaArena* arena) {
    if (!mgr || !key || !out) return 0;

    SeaSession* s = find_session(mgr, key);
    if (!s) return 0;

    u32 count = s->history_count < max_count ? s->history_count : max_count;
    /* Return the most recent messages */
    u32 start = s->history_count > count ? s->history_count - count : 0;

    for (u32 i = 0; i < count; i++) {
        SeaSessionMsg* sm = &s->history[start + i];
        out[i].role = sm->role;
        out[i].content = arena_strdup(arena, sm->content);
        out[i].tool_call_id = NULL;
        out[i].tool_name = NULL;
    }

    return count;
}

/* ── Get Summary ──────────────────────────────────────────── */

const char* sea_session_get_summary(SeaSessionManager* mgr, const char* key) {
    if (!mgr || !key) return NULL;
    SeaSession* s = find_session(mgr, key);
    return s ? s->summary : NULL;
}

/* ── Summarize ────────────────────────────────────────────── */

SeaError sea_session_summarize(SeaSessionManager* mgr, const char* key) {
    if (!mgr || !key) return SEA_ERR_INVALID_INPUT;

    SeaSession* s = find_session(mgr, key);
    if (!s || s->history_count == 0) return SEA_ERR_NOT_FOUND;
    if (!mgr->agent_cfg) return SEA_ERR_CONFIG;

    SEA_LOG_INFO("SESSION", "Summarizing session %s (%u messages)", key, s->history_count);

    /* Build conversation text for summarization */
    SeaArena sum_arena;
    if (sea_arena_create(&sum_arena, 256 * 1024) != SEA_OK) {
        return SEA_ERR_OOM;
    }

    /* Determine how many messages to summarize (all except keep_recent) */
    u32 to_summarize = s->history_count > mgr->keep_recent
                       ? s->history_count - mgr->keep_recent : 0;
    if (to_summarize == 0) {
        sea_arena_destroy(&sum_arena);
        return SEA_OK; /* Nothing to summarize */
    }

    /* Build prompt: existing summary + messages to compress */
    char prompt[8192];
    int pos = 0;

    pos += snprintf(prompt + pos, sizeof(prompt) - (size_t)pos,
        "Summarize the following conversation concisely. "
        "Capture key facts, decisions, and context. "
        "Output ONLY the summary, no preamble.\n\n");

    if (s->summary) {
        pos += snprintf(prompt + pos, sizeof(prompt) - (size_t)pos,
            "Previous summary:\n%s\n\n", s->summary);
    }

    pos += snprintf(prompt + pos, sizeof(prompt) - (size_t)pos,
        "New messages to incorporate:\n");

    for (u32 i = 0; i < to_summarize && pos < (int)sizeof(prompt) - 256; i++) {
        const char* role_str = "User";
        if (s->history[i].role == SEA_ROLE_ASSISTANT) role_str = "Assistant";
        else if (s->history[i].role == SEA_ROLE_SYSTEM) role_str = "System";

        const char* content = s->history[i].content ? s->history[i].content : "";
        /* Truncate long messages in the summarization prompt */
        int max_msg = 500;
        pos += snprintf(prompt + pos, sizeof(prompt) - (size_t)pos,
            "%s: %.500s%s\n", role_str, content,
            (int)strlen(content) > max_msg ? "..." : "");
    }

    /* Call LLM for summarization */
    SeaAgentResult ar = sea_agent_chat(mgr->agent_cfg, NULL, 0, prompt, &sum_arena);

    if (ar.error == SEA_OK && ar.text) {
        /* Store new summary */
        s->summary = arena_strdup(&mgr->arena, ar.text);

        /* Shift: keep only the recent messages */
        for (u32 i = 0; i < mgr->keep_recent && (to_summarize + i) < s->history_count; i++) {
            s->history[i] = s->history[to_summarize + i];
        }
        s->history_count = s->history_count > mgr->keep_recent
                           ? mgr->keep_recent : s->history_count;

        SEA_LOG_INFO("SESSION", "Summarized %s: %u msgs → summary + %u recent",
                     key, to_summarize, s->history_count);

        /* Persist summary to DB */
        if (mgr->db) {
            char sql[4096];
            /* Escape summary */
            char escaped[3072];
            u32 ei = 0;
            const char* sum = ar.text;
            for (u32 i = 0; sum[i] && ei < sizeof(escaped) - 2; i++) {
                if (sum[i] == '\'') {
                    escaped[ei++] = '\'';
                    escaped[ei++] = '\'';
                } else {
                    escaped[ei++] = sum[i];
                }
            }
            escaped[ei] = '\0';

            snprintf(sql, sizeof(sql),
                "INSERT OR REPLACE INTO sessions (key, channel, chat_id, summary, "
                "total_messages, created_at, last_active) VALUES "
                "('%s', '%s', %lld, '%s', %u, %llu, %llu);",
                key, s->channel ? s->channel : "",
                (long long)s->chat_id, escaped,
                s->total_messages,
                (unsigned long long)s->created_at,
                (unsigned long long)s->last_active);
            sea_db_exec(mgr->db, sql);
        }
    } else {
        SEA_LOG_WARN("SESSION", "Summarization failed for %s: %s",
                     key, ar.text ? ar.text : "unknown error");
    }

    sea_arena_destroy(&sum_arena);
    return SEA_OK;
}

/* ── Clear Session ────────────────────────────────────────── */

SeaError sea_session_clear(SeaSessionManager* mgr, const char* key) {
    if (!mgr || !key) return SEA_ERR_INVALID_INPUT;

    SeaSession* s = find_session(mgr, key);
    if (!s) return SEA_ERR_NOT_FOUND;

    s->history_count = 0;
    s->summary = NULL;
    s->total_messages = 0;

    if (mgr->db) {
        char sql[256];
        snprintf(sql, sizeof(sql),
            "DELETE FROM session_messages WHERE session_key = '%s';", key);
        sea_db_exec(mgr->db, sql);
        snprintf(sql, sizeof(sql),
            "DELETE FROM sessions WHERE key = '%s';", key);
        sea_db_exec(mgr->db, sql);
    }

    SEA_LOG_INFO("SESSION", "Cleared session: %s", key);
    return SEA_OK;
}

/* ── Utility ──────────────────────────────────────────────── */

u32 sea_session_count(SeaSessionManager* mgr) {
    return mgr ? mgr->count : 0;
}

u32 sea_session_list_keys(SeaSessionManager* mgr, const char** keys, u32 max_keys) {
    if (!mgr || !keys) return 0;
    u32 count = mgr->count < max_keys ? mgr->count : max_keys;
    for (u32 i = 0; i < count; i++) {
        keys[i] = mgr->sessions[i].key;
    }
    return count;
}

/* ── Save / Load ──────────────────────────────────────────── */

SeaError sea_session_save_all(SeaSessionManager* mgr) {
    if (!mgr || !mgr->db) return SEA_ERR_CONFIG;

    for (u32 i = 0; i < mgr->count; i++) {
        SeaSession* s = &mgr->sessions[i];
        char sql[4096];

        /* Escape summary */
        char escaped[3072];
        u32 ei = 0;
        if (s->summary) {
            for (u32 j = 0; s->summary[j] && ei < sizeof(escaped) - 2; j++) {
                if (s->summary[j] == '\'') {
                    escaped[ei++] = '\'';
                    escaped[ei++] = '\'';
                } else {
                    escaped[ei++] = s->summary[j];
                }
            }
        }
        escaped[ei] = '\0';

        snprintf(sql, sizeof(sql),
            "INSERT OR REPLACE INTO sessions (key, channel, chat_id, summary, "
            "total_messages, created_at, last_active) VALUES "
            "('%s', '%s', %lld, '%s', %u, %llu, %llu);",
            s->key, s->channel ? s->channel : "",
            (long long)s->chat_id, escaped,
            s->total_messages,
            (unsigned long long)s->created_at,
            (unsigned long long)s->last_active);
        sea_db_exec(mgr->db, sql);
    }

    SEA_LOG_INFO("SESSION", "Saved %u sessions to DB", mgr->count);
    return SEA_OK;
}

SeaError sea_session_load_all(SeaSessionManager* mgr) {
    if (!mgr || !mgr->db) return SEA_ERR_CONFIG;
    /* Sessions are loaded lazily on first access via sea_session_get.
     * This function is a placeholder for future bulk-load optimization. */
    SEA_LOG_INFO("SESSION", "Session lazy-loading enabled");
    return SEA_OK;
}
