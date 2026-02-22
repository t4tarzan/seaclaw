/*
 * sea_ws.c — WebSocket Channel Adapter Implementation
 *
 * Minimal RFC 6455 WebSocket server using raw POSIX sockets.
 * No external dependencies. Text frames only.
 */

#include "seaclaw/sea_ws.h"
#include "seaclaw/sea_log.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* ── Helpers ──────────────────────────────────────────────── */

static u64 now_epoch(void) { return (u64)time(NULL); }

static void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/* Minimal SHA-1 for WebSocket handshake (RFC 6455 §4.2.2) */
/* We use a simplified approach: compute SHA-1 of the accept key */

/* SHA-1 implementation (minimal, for handshake only) */
static void sha1_block(u32 state[5], const u8 block[64]) {
    u32 w[80];
    for (int i = 0; i < 16; i++) {
        w[i] = ((u32)block[i*4] << 24) | ((u32)block[i*4+1] << 16) |
               ((u32)block[i*4+2] << 8) | (u32)block[i*4+3];
    }
    for (int i = 16; i < 80; i++) {
        u32 t = w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16];
        w[i] = (t << 1) | (t >> 31);
    }
    u32 a = state[0], b = state[1], c = state[2], d = state[3], e = state[4];
    for (int i = 0; i < 80; i++) {
        u32 f, k;
        if (i < 20)      { f = (b & c) | ((~b) & d); k = 0x5A827999; }
        else if (i < 40) { f = b ^ c ^ d;             k = 0x6ED9EBA1; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
        else              { f = b ^ c ^ d;             k = 0xCA62C1D6; }
        u32 temp = ((a << 5) | (a >> 27)) + f + e + k + w[i];
        e = d; d = c; c = (b << 30) | (b >> 2); b = a; a = temp;
    }
    state[0] += a; state[1] += b; state[2] += c; state[3] += d; state[4] += e;
}

static void sha1(const u8* data, u32 len, u8 out[20]) {
    u32 state[5] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 };
    u32 i;
    for (i = 0; i + 64 <= len; i += 64) sha1_block(state, data + i);
    u8 block[64];
    u32 rem = len - i;
    memcpy(block, data + i, rem);
    block[rem++] = 0x80;
    if (rem > 56) {
        memset(block + rem, 0, 64 - rem);
        sha1_block(state, block);
        rem = 0;
    }
    memset(block + rem, 0, 56 - rem);
    u64 bits = (u64)len * 8;
    for (int j = 7; j >= 0; j--) block[56 + (7 - j)] = (u8)(bits >> (j * 8));
    sha1_block(state, block);
    for (int j = 0; j < 5; j++) {
        out[j*4]   = (u8)(state[j] >> 24);
        out[j*4+1] = (u8)(state[j] >> 16);
        out[j*4+2] = (u8)(state[j] >> 8);
        out[j*4+3] = (u8)(state[j]);
    }
}

static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void base64_encode(const u8* in, u32 in_len, char* out) {
    u32 i, j = 0;
    for (i = 0; i + 2 < in_len; i += 3) {
        out[j++] = b64_table[(in[i] >> 2) & 0x3F];
        out[j++] = b64_table[((in[i] & 0x3) << 4) | ((in[i+1] >> 4) & 0xF)];
        out[j++] = b64_table[((in[i+1] & 0xF) << 2) | ((in[i+2] >> 6) & 0x3)];
        out[j++] = b64_table[in[i+2] & 0x3F];
    }
    if (i < in_len) {
        out[j++] = b64_table[(in[i] >> 2) & 0x3F];
        if (i + 1 < in_len) {
            out[j++] = b64_table[((in[i] & 0x3) << 4) | ((in[i+1] >> 4) & 0xF)];
            out[j++] = b64_table[((in[i+1] & 0xF) << 2)];
        } else {
            out[j++] = b64_table[((in[i] & 0x3) << 4)];
            out[j++] = '=';
        }
        out[j++] = '=';
    }
    out[j] = '\0';
}

/* ── Init / Destroy ───────────────────────────────────────── */

SeaError sea_ws_init(SeaWsServer* ws, u16 port, SeaBus* bus) {
    if (!ws) return SEA_ERR_INVALID_INPUT;

    memset(ws, 0, sizeof(*ws));
    ws->listen_fd = -1;
    ws->port = port > 0 ? port : SEA_WS_DEFAULT_PORT;
    ws->bus = bus;
    ws->running = false;

    sea_arena_create(&ws->arena, 64 * 1024); /* 64KB for message buffers */

    for (u32 i = 0; i < SEA_WS_MAX_CLIENTS; i++) {
        ws->clients[i].fd = -1;
        ws->clients[i].state = SEA_WS_CLIENT_NONE;
    }

    SEA_LOG_INFO("WS", "WebSocket server initialized (port %u)", ws->port);
    return SEA_OK;
}

void sea_ws_destroy(SeaWsServer* ws) {
    if (!ws) return;

    ws->running = false;

    /* Close all clients */
    for (u32 i = 0; i < SEA_WS_MAX_CLIENTS; i++) {
        if (ws->clients[i].fd >= 0) {
            close(ws->clients[i].fd);
            ws->clients[i].fd = -1;
        }
    }

    if (ws->listen_fd >= 0) {
        close(ws->listen_fd);
        ws->listen_fd = -1;
    }

    sea_arena_destroy(&ws->arena);
    SEA_LOG_INFO("WS", "WebSocket server destroyed");
}

/* ── Listen ───────────────────────────────────────────────── */

SeaError sea_ws_listen(SeaWsServer* ws) {
    if (!ws) return SEA_ERR_INVALID_INPUT;

    ws->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (ws->listen_fd < 0) {
        SEA_LOG_ERROR("WS", "socket() failed: %s", strerror(errno));
        return SEA_ERR_IO;
    }

    int opt = 1;
    setsockopt(ws->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    set_nonblocking(ws->listen_fd);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(ws->port);

    if (bind(ws->listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        SEA_LOG_ERROR("WS", "bind() failed on port %u: %s",
                      ws->port, strerror(errno));
        close(ws->listen_fd);
        ws->listen_fd = -1;
        return SEA_ERR_IO;
    }

    if (listen(ws->listen_fd, SEA_WS_BACKLOG) < 0) {
        SEA_LOG_ERROR("WS", "listen() failed: %s", strerror(errno));
        close(ws->listen_fd);
        ws->listen_fd = -1;
        return SEA_ERR_IO;
    }

    ws->running = true;
    SEA_LOG_INFO("WS", "Listening on ws://0.0.0.0:%u", ws->port);
    return SEA_OK;
}

/* ── Accept new connection ────────────────────────────────── */

static SeaWsClient* find_free_slot(SeaWsServer* ws) {
    for (u32 i = 0; i < SEA_WS_MAX_CLIENTS; i++) {
        if (ws->clients[i].state == SEA_WS_CLIENT_NONE) return &ws->clients[i];
    }
    return NULL;
}

static void accept_connection(SeaWsServer* ws) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client_fd = accept(ws->listen_fd, (struct sockaddr*)&client_addr, &addr_len);
    if (client_fd < 0) return;

    SeaWsClient* slot = find_free_slot(ws);
    if (!slot) {
        SEA_LOG_WARN("WS", "Max clients reached, rejecting connection");
        close(client_fd);
        return;
    }

    set_nonblocking(client_fd);
    memset(slot, 0, sizeof(*slot));
    slot->fd = client_fd;
    slot->state = SEA_WS_CLIENT_HANDSHAKE;
    slot->chat_id = (i64)client_fd; /* Use fd as chat_id */
    slot->connected_at = now_epoch();
    snprintf(slot->addr, sizeof(slot->addr), "%s:%d",
             inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    ws->client_count++;
    ws->total_connections++;

    SEA_LOG_INFO("WS", "New connection from %s (fd=%d, clients=%u)",
                 slot->addr, client_fd, ws->client_count);
}

/* ── WebSocket Handshake ──────────────────────────────────── */

static const char* WS_MAGIC = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

static bool do_handshake(SeaWsClient* client) {
    char buf[2048];
    ssize_t n = recv(client->fd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) return false;
    buf[n] = '\0';

    /* Find Sec-WebSocket-Key header */
    const char* key_hdr = strstr(buf, "Sec-WebSocket-Key: ");
    if (!key_hdr) return false;
    key_hdr += 19;

    char key[64];
    int ki = 0;
    while (*key_hdr && *key_hdr != '\r' && *key_hdr != '\n' && ki < 63) {
        key[ki++] = *key_hdr++;
    }
    key[ki] = '\0';

    /* Compute accept key: SHA1(key + magic) → base64 */
    char concat[128];
    snprintf(concat, sizeof(concat), "%s%s", key, WS_MAGIC);

    u8 hash[20];
    sha1((const u8*)concat, (u32)strlen(concat), hash);

    char accept[64];
    base64_encode(hash, 20, accept);

    /* Send response */
    char response[512];
    int rlen = snprintf(response, sizeof(response),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "\r\n", accept);

    send(client->fd, response, (size_t)rlen, 0);
    client->state = SEA_WS_CLIENT_OPEN;

    SEA_LOG_INFO("WS", "Handshake complete for %s", client->addr);
    return true;
}

/* ── Frame Parsing ────────────────────────────────────────── */

static ssize_t read_frame(SeaWsClient* client, char* out, u32 out_max) {
    u8 header[14]; /* Max header size */
    ssize_t n = recv(client->fd, header, 2, MSG_PEEK);
    if (n < 2) return -1;

    /* Actually read the 2-byte header */
    recv(client->fd, header, 2, 0);

    u8 opcode = header[0] & 0x0F;
    bool masked = (header[1] & 0x80) != 0;
    u64 payload_len = header[1] & 0x7F;

    if (opcode == 0x8) {
        /* Close frame */
        client->state = SEA_WS_CLIENT_CLOSING;
        return 0;
    }

    if (opcode == 0x9) {
        /* Ping → send Pong */
        u8 pong[2] = { 0x8A, 0x00 };
        send(client->fd, pong, 2, 0);
        return -1;
    }

    if (opcode != 0x1) return -1; /* Only text frames */

    /* Extended payload length */
    if (payload_len == 126) {
        u8 ext[2];
        if (recv(client->fd, ext, 2, 0) < 2) return -1;
        payload_len = ((u64)ext[0] << 8) | ext[1];
    } else if (payload_len == 127) {
        u8 ext[8];
        if (recv(client->fd, ext, 8, 0) < 8) return -1;
        payload_len = 0;
        for (int i = 0; i < 8; i++) payload_len = (payload_len << 8) | ext[i];
    }

    if (payload_len > out_max || payload_len > SEA_WS_MAX_FRAME_SIZE) return -1;

    /* Masking key */
    u8 mask[4] = {0};
    if (masked) {
        if (recv(client->fd, mask, 4, 0) < 4) return -1;
    }

    /* Payload */
    u32 remaining = (u32)payload_len;
    u32 pos = 0;
    while (remaining > 0) {
        ssize_t r = recv(client->fd, out + pos, remaining, 0);
        if (r <= 0) return -1;
        remaining -= (u32)r;
        pos += (u32)r;
    }

    /* Unmask */
    if (masked) {
        for (u32 i = 0; i < (u32)payload_len; i++) {
            out[i] ^= (char)mask[i % 4];
        }
    }

    out[payload_len] = '\0';
    return (ssize_t)payload_len;
}

/* ── Send Frame ───────────────────────────────────────────── */

static SeaError send_frame(int fd, const char* text, u32 text_len) {
    u8 header[10];
    u32 hlen = 0;

    header[0] = 0x81; /* FIN + text opcode */
    if (text_len < 126) {
        header[1] = (u8)text_len;
        hlen = 2;
    } else if (text_len < 65536) {
        header[1] = 126;
        header[2] = (u8)(text_len >> 8);
        header[3] = (u8)(text_len & 0xFF);
        hlen = 4;
    } else {
        header[1] = 127;
        memset(header + 2, 0, 4);
        header[6] = (u8)(text_len >> 24);
        header[7] = (u8)(text_len >> 16);
        header[8] = (u8)(text_len >> 8);
        header[9] = (u8)(text_len & 0xFF);
        hlen = 10;
    }

    if (send(fd, header, hlen, 0) < 0) return SEA_ERR_IO;
    if (send(fd, text, text_len, 0) < 0) return SEA_ERR_IO;
    return SEA_OK;
}

/* ── Poll ─────────────────────────────────────────────────── */

u32 sea_ws_poll(SeaWsServer* ws) {
    if (!ws || !ws->running || ws->listen_fd < 0) return 0;

    /* Build fd_set */
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(ws->listen_fd, &readfds);
    int maxfd = ws->listen_fd;

    for (u32 i = 0; i < SEA_WS_MAX_CLIENTS; i++) {
        if (ws->clients[i].fd >= 0) {
            FD_SET(ws->clients[i].fd, &readfds);
            if (ws->clients[i].fd > maxfd) maxfd = ws->clients[i].fd;
        }
    }

    struct timeval tv = { .tv_sec = 0, .tv_usec = 10000 }; /* 10ms timeout */
    int ready = select(maxfd + 1, &readfds, NULL, NULL, &tv);
    if (ready <= 0) return 0;

    u32 messages = 0;

    /* Accept new connections */
    if (FD_ISSET(ws->listen_fd, &readfds)) {
        accept_connection(ws);
    }

    /* Process client messages */
    for (u32 i = 0; i < SEA_WS_MAX_CLIENTS; i++) {
        SeaWsClient* c = &ws->clients[i];
        if (c->fd < 0 || !FD_ISSET(c->fd, &readfds)) continue;

        if (c->state == SEA_WS_CLIENT_HANDSHAKE) {
            if (!do_handshake(c)) {
                close(c->fd);
                c->fd = -1;
                c->state = SEA_WS_CLIENT_NONE;
                ws->client_count--;
            }
            continue;
        }

        if (c->state == SEA_WS_CLIENT_OPEN) {
            sea_arena_reset(&ws->arena);
            char msg[SEA_WS_MAX_FRAME_SIZE];
            ssize_t len = read_frame(c, msg, sizeof(msg) - 1);

            if (len > 0) {
                c->last_msg_at = now_epoch();
                c->msg_count++;
                ws->total_messages++;
                messages++;

                /* Publish to bus */
                if (ws->bus) {
                    sea_bus_publish_inbound(ws->bus, SEA_MSG_USER,
                                             "websocket", c->addr, c->chat_id,
                                             msg, (u32)len);
                }

                SEA_LOG_DEBUG("WS", "[%s] %.*s", c->addr, (int)len, msg);
            } else if (len == 0 || c->state == SEA_WS_CLIENT_CLOSING) {
                SEA_LOG_INFO("WS", "Client %s disconnected", c->addr);
                close(c->fd);
                c->fd = -1;
                c->state = SEA_WS_CLIENT_NONE;
                ws->client_count--;
            }
        }
    }

    return messages;
}

/* ── Send / Broadcast ─────────────────────────────────────── */

SeaError sea_ws_send(SeaWsServer* ws, i64 chat_id,
                      const char* text, u32 text_len) {
    if (!ws || !text) return SEA_ERR_INVALID_INPUT;

    for (u32 i = 0; i < SEA_WS_MAX_CLIENTS; i++) {
        if (ws->clients[i].chat_id == chat_id &&
            ws->clients[i].state == SEA_WS_CLIENT_OPEN) {
            return send_frame(ws->clients[i].fd, text, text_len);
        }
    }
    return SEA_ERR_NOT_FOUND;
}

u32 sea_ws_broadcast(SeaWsServer* ws, const char* text, u32 text_len) {
    if (!ws || !text) return 0;
    u32 sent = 0;
    for (u32 i = 0; i < SEA_WS_MAX_CLIENTS; i++) {
        if (ws->clients[i].state == SEA_WS_CLIENT_OPEN) {
            if (send_frame(ws->clients[i].fd, text, text_len) == SEA_OK) sent++;
        }
    }
    return sent;
}

void sea_ws_close_client(SeaWsServer* ws, i64 chat_id) {
    if (!ws) return;
    for (u32 i = 0; i < SEA_WS_MAX_CLIENTS; i++) {
        if (ws->clients[i].chat_id == chat_id && ws->clients[i].fd >= 0) {
            /* Send close frame */
            u8 close_frame[2] = { 0x88, 0x00 };
            send(ws->clients[i].fd, close_frame, 2, 0);
            close(ws->clients[i].fd);
            ws->clients[i].fd = -1;
            ws->clients[i].state = SEA_WS_CLIENT_NONE;
            ws->client_count--;
            break;
        }
    }
}

u32 sea_ws_client_count(const SeaWsServer* ws) {
    return ws ? ws->client_count : 0;
}

/* ── Channel Adapter ──────────────────────────────────────── */

static SeaError ws_chan_init(SeaChannel* ch, SeaBus* bus, SeaArena* arena) {
    (void)arena;
    SeaWsServer* ws = (SeaWsServer*)ch->impl;
    ws->bus = bus;
    return SEA_OK;
}

static SeaError ws_chan_start(SeaChannel* ch) {
    SeaWsServer* ws = (SeaWsServer*)ch->impl;
    return sea_ws_listen(ws);
}

static SeaError ws_chan_poll(SeaChannel* ch) {
    SeaWsServer* ws = (SeaWsServer*)ch->impl;
    u32 msgs = sea_ws_poll(ws);
    return msgs > 0 ? SEA_OK : SEA_ERR_TIMEOUT;
}

static SeaError ws_chan_send(SeaChannel* ch, i64 chat_id,
                              const char* text, u32 text_len) {
    SeaWsServer* ws = (SeaWsServer*)ch->impl;
    return sea_ws_send(ws, chat_id, text, text_len);
}

static void ws_chan_stop(SeaChannel* ch) {
    SeaWsServer* ws = (SeaWsServer*)ch->impl;
    ws->running = false;
}

static void ws_chan_destroy(SeaChannel* ch) {
    SeaWsServer* ws = (SeaWsServer*)ch->impl;
    sea_ws_destroy(ws);
}

static const SeaChannelVTable s_ws_vtable = {
    .init    = ws_chan_init,
    .start   = ws_chan_start,
    .poll    = ws_chan_poll,
    .send    = ws_chan_send,
    .stop    = ws_chan_stop,
    .destroy = ws_chan_destroy,
};

SeaError sea_ws_channel_create(SeaChannel* ch, SeaWsServer* ws) {
    if (!ch || !ws) return SEA_ERR_INVALID_INPUT;
    sea_channel_base_init(ch, "websocket", &s_ws_vtable, ws);
    ch->enabled = true;
    return SEA_OK;
}
