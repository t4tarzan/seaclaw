/*
 * sea_arena.h — The Memory Notebook
 *
 * Arena allocator: one big block, bump pointer, instant reset.
 * Zero memory leaks. Zero pauses. Absolute predictability.
 *
 * "Open the notebook. Write sequentially. Rip out the page."
 */

#ifndef SEA_ARENA_H
#define SEA_ARENA_H

#include "sea_types.h"

typedef struct {
    u8* base;          /* The notebook paper                    */
    u64 size;          /* Total capacity in bytes               */
    u64 offset;        /* Current writing position (bump ptr)   */
    u64 high_water;    /* Peak usage tracker                    */
} SeaArena;

/* Create arena with given capacity. Returns SEA_OK or SEA_ERR_OOM. */
SeaError sea_arena_create(SeaArena* arena, u64 size);

/* Destroy arena and free backing memory. */
void sea_arena_destroy(SeaArena* arena);

/* Allocate `size` bytes from arena, aligned to `align`.
 * Returns NULL if arena is full. */
void* sea_arena_alloc(SeaArena* arena, u64 size, u64 align);

/* Convenience: allocate with default alignment (8 bytes). */
static inline void* sea_arena_push(SeaArena* arena, u64 size) {
    return sea_arena_alloc(arena, size, 8);
}

/* Copy a C string into the arena. Returns SeaSlice pointing into arena. */
SeaSlice sea_arena_push_cstr(SeaArena* arena, const char* cstr);

/* Copy raw bytes into the arena. Returns pointer or NULL. */
void* sea_arena_push_bytes(SeaArena* arena, const void* data, u64 len);

/* Reset arena — instant, one pointer move. Zero residue. */
static inline void sea_arena_reset(SeaArena* arena) {
    arena->offset = 0;
}

/* Query: bytes used */
static inline u64 sea_arena_used(const SeaArena* arena) {
    return arena->offset;
}

/* Query: bytes remaining */
static inline u64 sea_arena_remaining(const SeaArena* arena) {
    return arena->size - arena->offset;
}

/* Query: usage percentage (0.0 - 100.0) */
static inline f64 sea_arena_usage_pct(const SeaArena* arena) {
    if (arena->size == 0) return 0.0;
    return (f64)arena->offset / (f64)arena->size * 100.0;
}

#endif /* SEA_ARENA_H */
