/*
 * test_channel.c — Tests for Channel Manager
 *
 * Tests: sea_channel.h (base init, manager init, register, get, enabled names,
 *        stop, dispatch outbound with no messages)
 */

#include "seaclaw/sea_types.h"
#include "seaclaw/sea_channel.h"
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

/* ── Dummy VTable for testing ────────────────────────────── */

static SeaError dummy_init(SeaChannel* ch, SeaBus* bus, SeaArena* arena) {
    (void)ch; (void)bus; (void)arena;
    return SEA_OK;
}

static SeaError dummy_start(SeaChannel* ch) {
    (void)ch;
    return SEA_OK;
}

static SeaError dummy_send(SeaChannel* ch, i64 chat_id, const char* text, u32 len) {
    (void)ch; (void)chat_id; (void)text; (void)len;
    return SEA_OK;
}

static void dummy_stop(SeaChannel* ch) { (void)ch; }
static void dummy_destroy(SeaChannel* ch) { (void)ch; }

static const SeaChannelVTable s_dummy_vtable = {
    .init    = dummy_init,
    .start   = dummy_start,
    .poll    = NULL,
    .send    = dummy_send,
    .stop    = dummy_stop,
    .destroy = dummy_destroy,
};

/* ── Tests ────────────────────────────────────────────────── */

static void test_channel_base_init(void) {
    TEST("channel_base_init_sets_fields");
    SeaChannel ch;
    sea_channel_base_init(&ch, "test-chan", &s_dummy_vtable, NULL);
    if (strcmp(ch.name, "test-chan") != 0) { FAIL("name mismatch"); return; }
    if (ch.vtable != &s_dummy_vtable) { FAIL("vtable mismatch"); return; }
    if (ch.state != SEA_CHAN_STOPPED) { FAIL("state not stopped"); return; }
    if (!ch.enabled) { FAIL("should be enabled"); return; }
    PASS();
}

static void test_channel_base_init_null_name(void) {
    TEST("channel_base_init_null_name_uses_unknown");
    SeaChannel ch;
    sea_channel_base_init(&ch, NULL, &s_dummy_vtable, NULL);
    if (strcmp(ch.name, "unknown") != 0) { FAIL("expected 'unknown'"); return; }
    PASS();
}

static void test_channel_base_init_null_ch(void) {
    TEST("channel_base_init_null_ch_no_crash");
    sea_channel_base_init(NULL, "x", NULL, NULL);
    PASS();
}

static void test_manager_init(void) {
    TEST("channel_manager_init");
    SeaChannelManager mgr;
    SeaBus bus;
    sea_bus_init(&bus, 4096);
    SeaError err = sea_channel_manager_init(&mgr, &bus);
    if (err != SEA_OK) { FAIL("init failed"); return; }
    if (mgr.bus != &bus) { FAIL("wrong bus"); return; }
    if (mgr.count != 0) { FAIL("count not 0"); return; }
    if (mgr.running) { FAIL("should not be running"); return; }
    sea_bus_destroy(&bus);
    PASS();
}

static void test_manager_init_null(void) {
    TEST("channel_manager_init_null_returns_error");
    SeaError err = sea_channel_manager_init(NULL, NULL);
    if (err == SEA_OK) { FAIL("should fail"); return; }
    PASS();
}

static void test_manager_register(void) {
    TEST("channel_manager_register");
    SeaChannelManager mgr;
    SeaBus bus;
    sea_bus_init(&bus, 4096);
    sea_channel_manager_init(&mgr, &bus);

    SeaChannel ch;
    sea_channel_base_init(&ch, "telegram", &s_dummy_vtable, NULL);
    SeaError err = sea_channel_manager_register(&mgr, &ch);
    if (err != SEA_OK) { FAIL("register failed"); sea_bus_destroy(&bus); return; }
    if (mgr.count != 1) { FAIL("count not 1"); sea_bus_destroy(&bus); return; }
    if (ch.bus != &bus) { FAIL("bus not wired"); sea_bus_destroy(&bus); return; }
    sea_bus_destroy(&bus);
    PASS();
}

static void test_manager_register_null(void) {
    TEST("channel_manager_register_null_fails");
    SeaChannelManager mgr;
    SeaBus bus;
    sea_bus_init(&bus, 4096);
    sea_channel_manager_init(&mgr, &bus);
    SeaError err = sea_channel_manager_register(&mgr, NULL);
    if (err == SEA_OK) { FAIL("should fail"); sea_bus_destroy(&bus); return; }
    sea_bus_destroy(&bus);
    PASS();
}

static void test_manager_get(void) {
    TEST("channel_manager_get_by_name");
    SeaChannelManager mgr;
    SeaBus bus;
    sea_bus_init(&bus, 4096);
    sea_channel_manager_init(&mgr, &bus);

    SeaChannel ch1, ch2;
    sea_channel_base_init(&ch1, "telegram", &s_dummy_vtable, NULL);
    sea_channel_base_init(&ch2, "discord", &s_dummy_vtable, NULL);
    sea_channel_manager_register(&mgr, &ch1);
    sea_channel_manager_register(&mgr, &ch2);

    SeaChannel* found = sea_channel_manager_get(&mgr, "discord");
    if (!found) { FAIL("not found"); sea_bus_destroy(&bus); return; }
    if (strcmp(found->name, "discord") != 0) { FAIL("wrong channel"); sea_bus_destroy(&bus); return; }
    sea_bus_destroy(&bus);
    PASS();
}

static void test_manager_get_missing(void) {
    TEST("channel_manager_get_missing_returns_null");
    SeaChannelManager mgr;
    SeaBus bus;
    sea_bus_init(&bus, 4096);
    sea_channel_manager_init(&mgr, &bus);
    SeaChannel* found = sea_channel_manager_get(&mgr, "nonexistent");
    if (found != NULL) { FAIL("should be NULL"); sea_bus_destroy(&bus); return; }
    sea_bus_destroy(&bus);
    PASS();
}

static void test_manager_enabled_names(void) {
    TEST("channel_manager_enabled_names");
    SeaChannelManager mgr;
    SeaBus bus;
    sea_bus_init(&bus, 4096);
    sea_channel_manager_init(&mgr, &bus);

    SeaChannel ch1, ch2, ch3;
    sea_channel_base_init(&ch1, "telegram", &s_dummy_vtable, NULL);
    sea_channel_base_init(&ch2, "discord", &s_dummy_vtable, NULL);
    sea_channel_base_init(&ch3, "slack", &s_dummy_vtable, NULL);
    ch3.enabled = false;
    sea_channel_manager_register(&mgr, &ch1);
    sea_channel_manager_register(&mgr, &ch2);
    sea_channel_manager_register(&mgr, &ch3);

    const char* names[8];
    u32 count = sea_channel_manager_enabled_names(&mgr, names, 8);
    if (count != 2) { FAIL("expected 2 enabled"); sea_bus_destroy(&bus); return; }
    sea_bus_destroy(&bus);
    PASS();
}

static void test_manager_stop_all(void) {
    TEST("channel_manager_stop_all_no_crash");
    SeaChannelManager mgr;
    SeaBus bus;
    sea_bus_init(&bus, 4096);
    sea_channel_manager_init(&mgr, &bus);

    SeaChannel ch;
    sea_channel_base_init(&ch, "test", &s_dummy_vtable, NULL);
    sea_channel_manager_register(&mgr, &ch);
    sea_channel_manager_stop_all(&mgr);
    if (mgr.running) { FAIL("should not be running"); sea_bus_destroy(&bus); return; }
    sea_bus_destroy(&bus);
    PASS();
}

static void test_manager_stop_null(void) {
    TEST("channel_manager_stop_null_no_crash");
    sea_channel_manager_stop_all(NULL);
    PASS();
}

static void test_dispatch_outbound_empty(void) {
    TEST("channel_dispatch_outbound_empty_bus");
    SeaChannelManager mgr;
    SeaBus bus;
    sea_bus_init(&bus, 4096);
    sea_channel_manager_init(&mgr, &bus);
    u32 dispatched = sea_channel_dispatch_outbound(&mgr);
    if (dispatched != 0) { FAIL("should be 0"); sea_bus_destroy(&bus); return; }
    sea_bus_destroy(&bus);
    PASS();
}

static void test_max_channels(void) {
    TEST("max_channels_is_16");
    if (SEA_MAX_CHANNELS != 16) { FAIL("wrong max"); return; }
    PASS();
}

static void test_channel_states(void) {
    TEST("channel_state_enum_values");
    if (SEA_CHAN_STOPPED != 0) { FAIL("STOPPED != 0"); return; }
    if (SEA_CHAN_STARTING != 1) { FAIL("STARTING != 1"); return; }
    if (SEA_CHAN_RUNNING != 2) { FAIL("RUNNING != 2"); return; }
    if (SEA_CHAN_ERROR != 3) { FAIL("ERROR != 3"); return; }
    PASS();
}

/* ── Main ─────────────────────────────────────────────────── */

int main(void) {
    sea_log_init(SEA_LOG_WARN);

    printf("\n  \033[1mtest_channel\033[0m\n");

    test_channel_base_init();
    test_channel_base_init_null_name();
    test_channel_base_init_null_ch();
    test_manager_init();
    test_manager_init_null();
    test_manager_register();
    test_manager_register_null();
    test_manager_get();
    test_manager_get_missing();
    test_manager_enabled_names();
    test_manager_stop_all();
    test_manager_stop_null();
    test_dispatch_outbound_empty();
    test_max_channels();
    test_channel_states();

    printf("  Results: %u passed, %u failed\n\n", s_pass, s_fail);
    return s_fail > 0 ? 1 : 0;
}
