/*
 * test_ws.c — Tests for WebSocket Channel Adapter
 *
 * Tests: sea_ws.h (init, destroy, client count, channel adapter creation)
 * Note: Actual WebSocket connections require a running server, so we test
 * the initialization and teardown paths only.
 */

#include "seaclaw/sea_types.h"
#include "seaclaw/sea_ws.h"
#include "seaclaw/sea_bus.h"
#include "seaclaw/sea_arena.h"
#include "seaclaw/sea_log.h"

#include <stdio.h>
#include <string.h>

static u32 s_pass = 0;
static u32 s_fail = 0;

#define TEST(name) \
    do { printf("  %-48s ", name); } while(0)

#define PASS() \
    do { printf("\033[32mPASS\033[0m\n"); s_pass++; } while(0)

#define FAIL(msg) \
    do { printf("\033[31mFAIL\033[0m (%s)\n", msg); s_fail++; } while(0)

/* ── Tests ────────────────────────────────────────────────── */

static void test_ws_init(void) {
    TEST("ws_init_sets_port_and_bus");
    SeaWsServer ws;
    SeaBus bus;
    sea_bus_init(&bus, 4096);
    SeaError err = sea_ws_init(&ws, 19000, &bus);
    if (err != SEA_OK) { FAIL("init failed"); return; }
    if (ws.port != 19000) { FAIL("wrong port"); sea_ws_destroy(&ws); return; }
    if (ws.bus != &bus) { FAIL("wrong bus"); sea_ws_destroy(&ws); return; }
    if (ws.running) { FAIL("should not be running yet"); sea_ws_destroy(&ws); return; }
    sea_ws_destroy(&ws);
    PASS();
}

static void test_ws_init_default_port(void) {
    TEST("ws_default_port_is_18789");
    if (SEA_WS_DEFAULT_PORT != 18789) { FAIL("wrong default"); return; }
    PASS();
}

static void test_ws_client_count_zero(void) {
    TEST("ws_client_count_zero_after_init");
    SeaWsServer ws;
    SeaBus bus;
    sea_bus_init(&bus, 4096);
    sea_ws_init(&ws, 19001, &bus);
    u32 count = sea_ws_client_count(&ws);
    if (count != 0) { FAIL("should be 0"); sea_ws_destroy(&ws); return; }
    sea_ws_destroy(&ws);
    PASS();
}

static void test_ws_destroy_safe(void) {
    TEST("ws_destroy_after_init_no_crash");
    SeaWsServer ws;
    SeaBus bus;
    sea_bus_init(&bus, 4096);
    sea_ws_init(&ws, 19002, &bus);
    sea_ws_destroy(&ws);
    /* Should not crash */
    PASS();
}

static void test_ws_channel_create(void) {
    TEST("ws_channel_create_succeeds");
    SeaWsServer ws;
    SeaBus bus;
    sea_bus_init(&bus, 4096);
    sea_ws_init(&ws, 19003, &bus);
    SeaChannel ch;
    SeaError err = sea_ws_channel_create(&ch, &ws);
    if (err != SEA_OK) { FAIL("channel create failed"); sea_ws_destroy(&ws); return; }
    if (!ch.vtable) { FAIL("vtable is NULL"); sea_ws_destroy(&ws); return; }
    sea_ws_destroy(&ws);
    PASS();
}

static void test_ws_broadcast_no_clients(void) {
    TEST("ws_broadcast_zero_clients_returns_0");
    SeaWsServer ws;
    SeaBus bus;
    sea_bus_init(&bus, 4096);
    sea_ws_init(&ws, 19004, &bus);
    u32 sent = sea_ws_broadcast(&ws, "hello", 5);
    if (sent != 0) { FAIL("should send to 0"); sea_ws_destroy(&ws); return; }
    sea_ws_destroy(&ws);
    PASS();
}

static void test_ws_max_clients(void) {
    TEST("ws_max_clients_is_16");
    if (SEA_WS_MAX_CLIENTS != 16) { FAIL("wrong max"); return; }
    PASS();
}

static void test_ws_max_frame_size(void) {
    TEST("ws_max_frame_size_is_64k");
    if (SEA_WS_MAX_FRAME_SIZE != 64 * 1024) { FAIL("wrong max frame"); return; }
    PASS();
}

/* ── Main ─────────────────────────────────────────────────── */

int main(void) {
    sea_log_init(SEA_LOG_WARN);

    printf("\n  \033[1mtest_ws\033[0m\n");

    test_ws_init();
    test_ws_init_default_port();
    test_ws_client_count_zero();
    test_ws_destroy_safe();
    test_ws_channel_create();
    test_ws_broadcast_no_clients();
    test_ws_max_clients();
    test_ws_max_frame_size();

    printf("  Results: %u passed, %u failed\n\n", s_pass, s_fail);
    return s_fail > 0 ? 1 : 0;
}
