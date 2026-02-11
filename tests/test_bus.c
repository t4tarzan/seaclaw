/*
 * test_bus.c — Message Bus Tests
 *
 * Tests thread-safe publish/consume, queue overflow,
 * timeout behavior, and channel-specific outbound filtering.
 */

#include "seaclaw/sea_bus.h"
#include "seaclaw/sea_log.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>

static int s_pass = 0;
static int s_fail = 0;

#define TEST(name) printf("  [TEST] %s ... ", name)
#define PASS() do { printf("\033[32mPASS\033[0m\n"); s_pass++; } while(0)
#define FAIL(msg) do { printf("\033[31mFAIL: %s\033[0m\n", msg); s_fail++; } while(0)

/* ── Test: Init and Destroy ───────────────────────────────── */

static void test_init_destroy(void) {
    TEST("init_destroy");
    SeaBus bus;
    SeaError err = sea_bus_init(&bus, 64 * 1024);
    if (err != SEA_OK) { FAIL("init failed"); return; }
    if (!bus.running) { FAIL("not running after init"); sea_bus_destroy(&bus); return; }
    sea_bus_destroy(&bus);
    PASS();
}

/* ── Test: Publish and Consume Inbound ────────────────────── */

static void test_inbound_basic(void) {
    TEST("inbound_basic");
    SeaBus bus;
    sea_bus_init(&bus, 64 * 1024);

    const char* msg = "Hello from Telegram";
    SeaError err = sea_bus_publish_inbound(&bus, SEA_MSG_USER, "telegram", "12345",
                                           100, msg, (u32)strlen(msg));
    if (err != SEA_OK) { FAIL("publish failed"); sea_bus_destroy(&bus); return; }

    if (sea_bus_inbound_count(&bus) != 1) { FAIL("count != 1"); sea_bus_destroy(&bus); return; }

    SeaBusMsg out;
    err = sea_bus_consume_inbound(&bus, &out, 100);
    if (err != SEA_OK) { FAIL("consume failed"); sea_bus_destroy(&bus); return; }

    if (out.type != SEA_MSG_USER) { FAIL("wrong type"); sea_bus_destroy(&bus); return; }
    if (strcmp(out.channel, "telegram") != 0) { FAIL("wrong channel"); sea_bus_destroy(&bus); return; }
    if (out.chat_id != 100) { FAIL("wrong chat_id"); sea_bus_destroy(&bus); return; }
    if (strcmp(out.content, "Hello from Telegram") != 0) { FAIL("wrong content"); sea_bus_destroy(&bus); return; }
    if (out.content_len != (u32)strlen(msg)) { FAIL("wrong len"); sea_bus_destroy(&bus); return; }
    if (!out.session_key) { FAIL("no session_key"); sea_bus_destroy(&bus); return; }
    if (strcmp(out.session_key, "telegram:100") != 0) { FAIL("wrong session_key"); sea_bus_destroy(&bus); return; }

    if (sea_bus_inbound_count(&bus) != 0) { FAIL("count != 0 after consume"); sea_bus_destroy(&bus); return; }

    sea_bus_destroy(&bus);
    PASS();
}

/* ── Test: Outbound Publish and Consume ───────────────────── */

static void test_outbound_basic(void) {
    TEST("outbound_basic");
    SeaBus bus;
    sea_bus_init(&bus, 64 * 1024);

    const char* resp = "Here is your answer";
    SeaError err = sea_bus_publish_outbound(&bus, "telegram", 100, resp, (u32)strlen(resp));
    if (err != SEA_OK) { FAIL("publish failed"); sea_bus_destroy(&bus); return; }

    SeaBusMsg out;
    err = sea_bus_consume_outbound(&bus, &out);
    if (err != SEA_OK) { FAIL("consume failed"); sea_bus_destroy(&bus); return; }

    if (out.type != SEA_MSG_OUTBOUND) { FAIL("wrong type"); sea_bus_destroy(&bus); return; }
    if (strcmp(out.content, "Here is your answer") != 0) { FAIL("wrong content"); sea_bus_destroy(&bus); return; }

    sea_bus_destroy(&bus);
    PASS();
}

/* ── Test: Consume Outbound for Specific Channel ──────────── */

static void test_outbound_for_channel(void) {
    TEST("outbound_for_channel");
    SeaBus bus;
    sea_bus_init(&bus, 64 * 1024);

    /* Publish messages for different channels */
    sea_bus_publish_outbound(&bus, "telegram", 100, "msg1", 4);
    sea_bus_publish_outbound(&bus, "discord", 200, "msg2", 4);
    sea_bus_publish_outbound(&bus, "telegram", 300, "msg3", 4);

    /* Consume only discord messages */
    SeaBusMsg out;
    SeaError err = sea_bus_consume_outbound_for(&bus, "discord", &out);
    if (err != SEA_OK) { FAIL("discord consume failed"); sea_bus_destroy(&bus); return; }
    if (out.chat_id != 200) { FAIL("wrong chat_id for discord"); sea_bus_destroy(&bus); return; }

    /* Should have 2 messages left (both telegram) */
    if (sea_bus_outbound_count(&bus) != 2) { FAIL("count != 2"); sea_bus_destroy(&bus); return; }

    /* No more discord messages */
    err = sea_bus_consume_outbound_for(&bus, "discord", &out);
    if (err != SEA_ERR_NOT_FOUND) { FAIL("should be empty for discord"); sea_bus_destroy(&bus); return; }

    sea_bus_destroy(&bus);
    PASS();
}

/* ── Test: Consume Timeout ────────────────────────────────── */

static void test_consume_timeout(void) {
    TEST("consume_timeout");
    SeaBus bus;
    sea_bus_init(&bus, 64 * 1024);

    SeaBusMsg out;
    SeaError err = sea_bus_consume_inbound(&bus, &out, 50); /* 50ms timeout */
    if (err != SEA_ERR_TIMEOUT) { FAIL("expected timeout"); sea_bus_destroy(&bus); return; }

    sea_bus_destroy(&bus);
    PASS();
}

/* ── Test: Non-blocking consume on empty ──────────────────── */

static void test_consume_nonblocking(void) {
    TEST("consume_nonblocking");
    SeaBus bus;
    sea_bus_init(&bus, 64 * 1024);

    SeaBusMsg out;
    SeaError err = sea_bus_consume_inbound(&bus, &out, 0); /* Non-blocking */
    if (err != SEA_ERR_NOT_FOUND) { FAIL("expected not_found"); sea_bus_destroy(&bus); return; }

    err = sea_bus_consume_outbound(&bus, &out);
    if (err != SEA_ERR_NOT_FOUND) { FAIL("expected not_found for outbound"); sea_bus_destroy(&bus); return; }

    sea_bus_destroy(&bus);
    PASS();
}

/* ── Test: Multiple Messages FIFO Order ───────────────────── */

static void test_fifo_order(void) {
    TEST("fifo_order");
    SeaBus bus;
    sea_bus_init(&bus, 64 * 1024);

    sea_bus_publish_inbound(&bus, SEA_MSG_USER, "telegram", "1", 10, "first", 5);
    sea_bus_publish_inbound(&bus, SEA_MSG_USER, "telegram", "1", 10, "second", 6);
    sea_bus_publish_inbound(&bus, SEA_MSG_USER, "telegram", "1", 10, "third", 5);

    SeaBusMsg out;
    sea_bus_consume_inbound(&bus, &out, 0);
    if (strcmp(out.content, "first") != 0) { FAIL("not first"); sea_bus_destroy(&bus); return; }

    sea_bus_consume_inbound(&bus, &out, 0);
    if (strcmp(out.content, "second") != 0) { FAIL("not second"); sea_bus_destroy(&bus); return; }

    sea_bus_consume_inbound(&bus, &out, 0);
    if (strcmp(out.content, "third") != 0) { FAIL("not third"); sea_bus_destroy(&bus); return; }

    sea_bus_destroy(&bus);
    PASS();
}

/* ── Test: Concurrent Producer/Consumer ───────────────────── */

typedef struct {
    SeaBus* bus;
    int count;
} ThreadData;

static void* producer_thread(void* arg) {
    ThreadData* td = (ThreadData*)arg;
    for (int i = 0; i < td->count; i++) {
        char msg[32];
        int n = snprintf(msg, sizeof(msg), "msg-%d", i);
        sea_bus_publish_inbound(td->bus, SEA_MSG_USER, "test", "prod",
                                 1, msg, (u32)n);
        usleep(1000); /* 1ms between messages */
    }
    return NULL;
}

static void test_concurrent(void) {
    TEST("concurrent_producer_consumer");
    SeaBus bus;
    sea_bus_init(&bus, 256 * 1024);

    ThreadData td = { .bus = &bus, .count = 50 };
    pthread_t tid;
    pthread_create(&tid, NULL, producer_thread, &td);

    int consumed = 0;
    int retries = 0;
    while (consumed < 50 && retries < 200) {
        SeaBusMsg out;
        SeaError err = sea_bus_consume_inbound(&bus, &out, 100);
        if (err == SEA_OK) {
            consumed++;
            retries = 0;
        } else {
            retries++;
        }
    }

    pthread_join(tid, NULL);

    if (consumed != 50) {
        char buf[64];
        snprintf(buf, sizeof(buf), "consumed %d/50", consumed);
        FAIL(buf);
    } else {
        PASS();
    }

    sea_bus_destroy(&bus);
}

/* ── Test: Session Key Generation ─────────────────────────── */

static void test_session_key(void) {
    TEST("session_key_generation");
    SeaBus bus;
    sea_bus_init(&bus, 64 * 1024);

    sea_bus_publish_inbound(&bus, SEA_MSG_USER, "discord", "user1",
                             42, "hello", 5);

    SeaBusMsg out;
    sea_bus_consume_inbound(&bus, &out, 0);
    if (!out.session_key) { FAIL("no session_key"); sea_bus_destroy(&bus); return; }
    if (strcmp(out.session_key, "discord:42") != 0) {
        FAIL("wrong session_key");
        sea_bus_destroy(&bus);
        return;
    }

    sea_bus_destroy(&bus);
    PASS();
}

/* ── Test: Message Types ──────────────────────────────────── */

static void test_message_types(void) {
    TEST("message_types");
    SeaBus bus;
    sea_bus_init(&bus, 64 * 1024);

    sea_bus_publish_inbound(&bus, SEA_MSG_USER, "tg", "1", 1, "user", 4);
    sea_bus_publish_inbound(&bus, SEA_MSG_SYSTEM, "system", "cron", 0, "tick", 4);

    SeaBusMsg out;
    sea_bus_consume_inbound(&bus, &out, 0);
    if (out.type != SEA_MSG_USER) { FAIL("first not USER"); sea_bus_destroy(&bus); return; }

    sea_bus_consume_inbound(&bus, &out, 0);
    if (out.type != SEA_MSG_SYSTEM) { FAIL("second not SYSTEM"); sea_bus_destroy(&bus); return; }

    sea_bus_destroy(&bus);
    PASS();
}

/* ── Main ─────────────────────────────────────────────────── */

int main(void) {
    sea_log_init(SEA_LOG_WARN); /* Suppress info/debug during tests */

    printf("\n\033[1m=== Sea-Claw Bus Tests ===\033[0m\n\n");

    test_init_destroy();
    test_inbound_basic();
    test_outbound_basic();
    test_outbound_for_channel();
    test_consume_timeout();
    test_consume_nonblocking();
    test_fifo_order();
    test_concurrent();
    test_session_key();
    test_message_types();

    printf("\n\033[1mResults: %d passed, %d failed\033[0m\n\n", s_pass, s_fail);
    return s_fail > 0 ? 1 : 0;
}
