/*
 * sea_session.h — Session Management
 *
 * Per-channel, per-chat session isolation with automatic
 * conversation summarization. Each session tracks its own
 * message history, summary, and metadata.
 *
 * Session keys are "channel:chat_id" (e.g. "telegram:12345").
 * Sessions are backed by SQLite for persistence across restarts.
 *
 * "Every conversation has its own room in the Vault."
 */

#ifndef SEA_SESSION_H
#define SEA_SESSION_H

#include "sea_types.h"
#include "sea_arena.h"
#include "sea_db.h"
#include "sea_agent.h"

/* ── Session Message ──────────────────────────────────────── */

typedef struct {
    SeaRole     role;
    const char* content;
    u64         timestamp_ms;
} SeaSessionMsg;

/* ── Session ──────────────────────────────────────────────── */

#define SEA_SESSION_MAX_HISTORY 50
#define SEA_SESSION_KEY_MAX     128

typedef struct {
    char          key[SEA_SESSION_KEY_MAX];   /* "channel:chat_id"          */
    const char*   channel;                    /* Channel name               */
    i64           chat_id;                    /* Chat ID                    */
    SeaSessionMsg history[SEA_SESSION_MAX_HISTORY];
    u32           history_count;
    const char*   summary;                    /* Compressed summary of old msgs */
    u32           total_messages;             /* Lifetime message count     */
    u64           created_at;                 /* First message timestamp    */
    u64           last_active;                /* Last message timestamp     */
} SeaSession;

/* ── Session Manager ──────────────────────────────────────── */

#define SEA_MAX_SESSIONS 64

typedef struct {
    SeaSession  sessions[SEA_MAX_SESSIONS];
    u32         count;
    SeaDb*      db;                          /* Backing store              */
    SeaArena    arena;                       /* Arena for session strings  */

    /* Summarization config */
    u32         max_history;                 /* Trigger summarize at this count */
    u32         keep_recent;                 /* Keep this many recent msgs     */
    SeaAgentConfig* agent_cfg;              /* For LLM-driven summarization   */
} SeaSessionManager;

/* ── API ──────────────────────────────────────────────────── */

/* Initialize session manager. */
SeaError sea_session_init(SeaSessionManager* mgr, SeaDb* db,
                           SeaAgentConfig* agent_cfg, u64 arena_size);

/* Destroy session manager and free resources. */
void sea_session_destroy(SeaSessionManager* mgr);

/* Get or create a session by key. Returns pointer to session. */
SeaSession* sea_session_get(SeaSessionManager* mgr, const char* key);

/* Get or create a session by channel + chat_id. */
SeaSession* sea_session_get_by_chat(SeaSessionManager* mgr,
                                     const char* channel, i64 chat_id);

/* Add a message to a session. Triggers summarization if threshold reached. */
SeaError sea_session_add_message(SeaSessionManager* mgr, const char* key,
                                  SeaRole role, const char* content);

/* Get the message history for a session as SeaChatMsg array.
 * Returns count. Messages are allocated in the provided arena. */
u32 sea_session_get_history(SeaSessionManager* mgr, const char* key,
                             SeaChatMsg* out, u32 max_count, SeaArena* arena);

/* Get the summary for a session. Returns NULL if no summary. */
const char* sea_session_get_summary(SeaSessionManager* mgr, const char* key);

/* Force summarization of a session's history. */
SeaError sea_session_summarize(SeaSessionManager* mgr, const char* key);

/* Clear a session's history and summary. */
SeaError sea_session_clear(SeaSessionManager* mgr, const char* key);

/* Get session count. */
u32 sea_session_count(SeaSessionManager* mgr);

/* List active session keys. Returns count. */
u32 sea_session_list_keys(SeaSessionManager* mgr, const char** keys, u32 max_keys);

/* Save all sessions to DB. */
SeaError sea_session_save_all(SeaSessionManager* mgr);

/* Load sessions from DB. */
SeaError sea_session_load_all(SeaSessionManager* mgr);

/* Build session key from channel + chat_id. Writes to buf. */
void sea_session_build_key(char* buf, u32 buf_size,
                            const char* channel, i64 chat_id);

#endif /* SEA_SESSION_H */
