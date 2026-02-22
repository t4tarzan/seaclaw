/*
 * sea_auth.h — Authentication & Token Framework
 *
 * Bearer token auth with permissions bitmask.
 * Tokens are stored in SQLite, validated per-request.
 * Used for Gateway API, A2A delegation, and remote skill install.
 *
 * "Trust is earned, verified, and revoked."
 */

#ifndef SEA_AUTH_H
#define SEA_AUTH_H

#include "sea_types.h"
#include "sea_arena.h"

/* ── Permission Bitmask ───────────────────────────────────── */

typedef enum {
    SEA_PERM_NONE       = 0,
    SEA_PERM_CHAT       = (1 << 0),   /* Send/receive chat messages */
    SEA_PERM_TOOLS      = (1 << 1),   /* Execute tools */
    SEA_PERM_SHELL      = (1 << 2),   /* Run shell commands */
    SEA_PERM_FILES      = (1 << 3),   /* Read/write files */
    SEA_PERM_NETWORK    = (1 << 4),   /* HTTP requests, DNS, etc. */
    SEA_PERM_ADMIN      = (1 << 5),   /* Config changes, token management */
    SEA_PERM_DELEGATE   = (1 << 6),   /* Delegate to Agent Zero / A2A */
    SEA_PERM_SKILLS     = (1 << 7),   /* Install/manage skills */
    SEA_PERM_ALL        = 0xFF,
} SeaPerm;

/* ── Token Structure ──────────────────────────────────────── */

#define SEA_TOKEN_LEN           64
#define SEA_TOKEN_LABEL_MAX     64
#define SEA_AUTH_MAX_TOKENS     32
#define SEA_AUTH_MAX_ALLOWED_TOOLS 16
#define SEA_AUTH_TOOL_NAME_MAX  64

typedef struct {
    char    token[SEA_TOKEN_LEN + 1];
    char    label[SEA_TOKEN_LABEL_MAX];
    u32     permissions;    /* Bitmask of SeaPerm */
    i64     created_at;     /* Unix timestamp */
    i64     expires_at;     /* 0 = never expires */
    bool    revoked;
    /* Tool allowlist: if allowed_tool_count > 0, only these tools can be called.
     * Empty list = all tools allowed (subject to SEA_PERM_TOOLS). */
    char    allowed_tools[SEA_AUTH_MAX_ALLOWED_TOOLS][SEA_AUTH_TOOL_NAME_MAX];
    u32     allowed_tool_count;
} SeaAuthToken;

/* ── Auth Manager ─────────────────────────────────────────── */

typedef struct {
    SeaAuthToken  tokens[SEA_AUTH_MAX_TOKENS];
    u32           count;
    bool          enabled;  /* If false, all requests are allowed (dev mode) */
} SeaAuth;

/* ── API ──────────────────────────────────────────────────── */

/* Initialize the auth manager. */
void sea_auth_init(SeaAuth* auth, bool enabled);

/* Generate a new token with given label and permissions.
 * Token string is written to out_token (must be SEA_TOKEN_LEN+1 bytes).
 * Returns SEA_OK or SEA_ERR_FULL. */
SeaError sea_auth_create_token(SeaAuth* auth, const char* label,
                                u32 permissions, i64 expires_at,
                                char* out_token);

/* Validate a Bearer token. Returns the permissions bitmask, or 0 if invalid. */
u32 sea_auth_validate(const SeaAuth* auth, const char* token);

/* Check if a token has a specific permission. */
bool sea_auth_has_perm(const SeaAuth* auth, const char* token, SeaPerm perm);

/* Revoke a token by its string. Returns SEA_OK or SEA_ERR_NOT_FOUND. */
SeaError sea_auth_revoke(SeaAuth* auth, const char* token);

/* List all tokens (labels + permissions, NOT the token strings).
 * Returns count written to out array. */
u32 sea_auth_list(const SeaAuth* auth, SeaAuthToken* out, u32 max);

/* Get count of active (non-revoked) tokens. */
u32 sea_auth_active_count(const SeaAuth* auth);

/* ── Tool Allowlist ───────────────────────────────────────── */

/* Add a tool to a token's allowlist. */
SeaError sea_auth_allow_tool(SeaAuth* auth, const char* token, const char* tool_name);

/* Check if a token is allowed to call a specific tool.
 * Returns true if: (1) token has SEA_PERM_TOOLS, AND
 *                  (2) allowlist is empty OR tool is in allowlist. */
bool sea_auth_can_call_tool(const SeaAuth* auth, const char* token,
                             const char* tool_name);

#endif /* SEA_AUTH_H */
