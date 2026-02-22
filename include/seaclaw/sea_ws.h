/*
 * sea_ws.h — WebSocket Channel Adapter
 *
 * Minimal WebSocket server for LAN-accessible agent communication.
 * Listens on a configurable port, accepts WS connections, and
 * routes messages through the bus like any other channel.
 *
 * Uses raw TCP sockets + RFC 6455 handshake (no external deps).
 * Supports text frames only (no binary, no extensions).
 *
 * Inspired by MimicLaw's WebSocket gateway on port 18789.
 *
 * "The Vault speaks to anyone on the network who knows the port."
 */

#ifndef SEA_WS_H
#define SEA_WS_H

#include "sea_types.h"
#include "sea_arena.h"
#include "sea_bus.h"
#include "sea_channel.h"

/* ── Configuration ────────────────────────────────────────── */

#define SEA_WS_DEFAULT_PORT     18789
#define SEA_WS_MAX_CLIENTS      16
#define SEA_WS_MAX_FRAME_SIZE   (64 * 1024)  /* 64KB max message */
#define SEA_WS_BACKLOG          4

/* ── Client State ─────────────────────────────────────────── */

typedef enum {
    SEA_WS_CLIENT_NONE = 0,
    SEA_WS_CLIENT_HANDSHAKE,
    SEA_WS_CLIENT_OPEN,
    SEA_WS_CLIENT_CLOSING,
} SeaWsClientState;

typedef struct {
    int                 fd;
    SeaWsClientState    state;
    i64                 chat_id;        /* Derived from fd for bus routing */
    char                addr[64];       /* Client IP:port string */
    u64                 connected_at;
    u64                 last_msg_at;
    u32                 msg_count;
} SeaWsClient;

/* ── WebSocket Server ─────────────────────────────────────── */

typedef struct {
    int             listen_fd;
    u16             port;
    SeaWsClient     clients[SEA_WS_MAX_CLIENTS];
    u32             client_count;
    SeaBus*         bus;
    SeaArena        arena;
    bool            running;
    u64             total_connections;
    u64             total_messages;
} SeaWsServer;

/* ── API ──────────────────────────────────────────────────── */

/* Initialize the WebSocket server. Does NOT start listening yet. */
SeaError sea_ws_init(SeaWsServer* ws, u16 port, SeaBus* bus);

/* Destroy the server and close all connections. */
void sea_ws_destroy(SeaWsServer* ws);

/* Start listening on the configured port. Non-blocking. */
SeaError sea_ws_listen(SeaWsServer* ws);

/* Poll for new connections and incoming messages.
 * Call this in a loop (or dedicated thread). Non-blocking.
 * Returns number of messages processed. */
u32 sea_ws_poll(SeaWsServer* ws);

/* Send a text message to a specific client by chat_id. */
SeaError sea_ws_send(SeaWsServer* ws, i64 chat_id,
                      const char* text, u32 text_len);

/* Broadcast a text message to all connected clients. */
u32 sea_ws_broadcast(SeaWsServer* ws, const char* text, u32 text_len);

/* Close a specific client connection. */
void sea_ws_close_client(SeaWsServer* ws, i64 chat_id);

/* Get connected client count. */
u32 sea_ws_client_count(const SeaWsServer* ws);

/* ── Channel Adapter ──────────────────────────────────────── */

/* Create a SeaChannel wrapping this WebSocket server.
 * The channel can be registered with the channel manager. */
SeaError sea_ws_channel_create(SeaChannel* ch, SeaWsServer* ws);

#endif /* SEA_WS_H */
