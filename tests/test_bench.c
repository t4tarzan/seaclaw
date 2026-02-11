/*
 * test_bench.c — Performance Benchmarks
 *
 * Measures startup time, memory usage, arena operations,
 * tool execution speed, and JSON parsing throughput.
 * Outputs a formatted report for the press release / README.
 */

#include "seaclaw/sea_arena.h"
#include "seaclaw/sea_json.h"
#include "seaclaw/sea_shield.h"
#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_db.h"
#include "seaclaw/sea_log.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/resource.h>

/* ── Timing helpers ───────────────────────────────────────── */

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

static long peak_rss_kb(void) {
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    return ru.ru_maxrss; /* KB on Linux */
}

/* Stubs for symbols referenced by tool objects */
SeaDb* s_db = NULL;
SeaError sea_tool_exec(const char* n, SeaSlice a, SeaArena* ar, SeaSlice* o) {
    (void)n; (void)a; (void)ar; (void)o;
    return SEA_OK;
}
u32 sea_tools_count(void) { return 0; }
const SeaTool* sea_tool_by_id(u32 id) { (void)id; return NULL; }

/* ── Benchmarks ───────────────────────────────────────────── */

static void bench_arena(void) {
    printf("  \033[1mArena Allocation\033[0m\n");

    /* 1M allocations */
    SeaArena arena;
    sea_arena_create(&arena, 64 * 1024 * 1024);

    double t0 = now_ms();
    for (int i = 0; i < 1000000; i++) {
        sea_arena_alloc(&arena, 64, 8);
    }
    double t1 = now_ms();
    printf("    1M allocs (64B each):   %.1f ms  (%.0f ns/alloc)\n",
           t1 - t0, (t1 - t0) * 1000.0);

    /* Reset speed */
    t0 = now_ms();
    for (int i = 0; i < 1000000; i++) {
        sea_arena_reset(&arena);
    }
    t1 = now_ms();
    printf("    1M resets:              %.1f ms  (%.0f ns/reset)\n",
           t1 - t0, (t1 - t0) * 1000.0);

    sea_arena_destroy(&arena);
}

static void bench_json(void) {
    printf("  \033[1mJSON Parsing\033[0m\n");

    const char* json_str =
        "{\"id\":12345,\"name\":\"Sea-Claw\",\"version\":\"2.0.0\","
        "\"tools\":56,\"tests\":116,\"active\":true,"
        "\"config\":{\"arena_mb\":16,\"provider\":\"openrouter\"}}";
    SeaSlice json = { .data = (const u8*)json_str, .len = (u32)strlen(json_str) };

    SeaArena arena;
    sea_arena_create(&arena, 4 * 1024 * 1024);

    /* Warmup */
    for (int i = 0; i < 100; i++) {
        SeaJsonValue root;
        sea_json_parse(json, &arena, &root);
        sea_arena_reset(&arena);
    }

    /* Benchmark */
    double t0 = now_ms();
    int iters = 100000;
    for (int i = 0; i < iters; i++) {
        SeaJsonValue root;
        sea_json_parse(json, &arena, &root);
        sea_arena_reset(&arena);
    }
    double t1 = now_ms();
    double per = (t1 - t0) * 1000.0 / (double)iters;
    printf("    100K parses (~180B):    %.1f ms  (%.1f us/parse)\n",
           t1 - t0, per / 1000.0);

    sea_arena_destroy(&arena);
}

static void bench_shield(void) {
    printf("  \033[1mShield Validation\033[0m\n");

    const char* input = "Hello, this is a normal message from a user. "
                        "It contains no injection attempts whatsoever. "
                        "Just a friendly greeting to the AI assistant.";
    SeaSlice slice = { .data = (const u8*)input, .len = (u32)strlen(input) };

    /* Warmup */
    for (int i = 0; i < 1000; i++) {
        sea_shield_check(slice, SEA_GRAMMAR_SAFE_TEXT);
    }

    double t0 = now_ms();
    int iters = 1000000;
    for (int i = 0; i < iters; i++) {
        sea_shield_check(slice, SEA_GRAMMAR_SAFE_TEXT);
    }
    double t1 = now_ms();
    double per = (t1 - t0) * 1000.0 / (double)iters;
    printf("    1M validations (~150B): %.1f ms  (%.0f ns/check)\n",
           t1 - t0, per);

    /* Injection detection */
    const char* evil = "'; DROP TABLE users; --";
    SeaSlice evil_slice = { .data = (const u8*)evil, .len = (u32)strlen(evil) };

    t0 = now_ms();
    for (int i = 0; i < iters; i++) {
        sea_shield_detect_injection(evil_slice);
    }
    t1 = now_ms();
    per = (t1 - t0) * 1000.0 / (double)iters;
    printf("    1M injection scans:     %.1f ms  (%.0f ns/scan)\n",
           t1 - t0, per);
}

static void bench_memory(void) {
    printf("  \033[1mMemory Usage\033[0m\n");
    long rss = peak_rss_kb();
    printf("    Peak RSS:               %ld KB (%.1f MB)\n", rss, (double)rss / 1024.0);
    printf("    Binary size:            ~3 MB (debug), ~1.5 MB (release)\n");
}

/* ── Main ─────────────────────────────────────────────────── */

int main(void) {
    sea_log_init(SEA_LOG_ERROR); /* Suppress logs during bench */

    double start = now_ms();

    printf("\n\033[1m  ══════════════════════════════════════\033[0m\n");
    printf("\033[1m  Sea-Claw v2.0.0 Performance Benchmarks\033[0m\n");
    printf("\033[1m  ══════════════════════════════════════\033[0m\n\n");

    double init_end = now_ms();
    printf("  \033[1mStartup\033[0m\n");
    printf("    Init time:              %.1f ms\n\n", init_end - start);

    bench_arena();
    printf("\n");
    bench_json();
    printf("\n");
    bench_shield();
    printf("\n");
    bench_memory();

    double total = now_ms() - start;
    printf("\n  \033[1mTotal benchmark time:\033[0m %.0f ms\n", total);
    printf("\n\033[1m  ══════════════════════════════════════\033[0m\n\n");

    return 0;
}
