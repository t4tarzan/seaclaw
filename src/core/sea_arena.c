/*
 * sea_arena.c — Arena allocator implementation
 *
 * The Memory Notebook: one mmap'd block, bump pointer, instant reset.
 */

#include "seaclaw/sea_arena.h"
#include <sys/mman.h>
#include <string.h>

SeaError sea_arena_create(SeaArena* arena, u64 size) {
    if (!arena || size == 0) return SEA_ERR_OOM;

    void* base = mmap(NULL, size,
                      PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS,
                      -1, 0);

    if (base == MAP_FAILED) return SEA_ERR_OOM;

    arena->base       = (u8*)base;
    arena->size       = size;
    arena->offset     = 0;
    arena->high_water = 0;

    return SEA_OK;
}

void sea_arena_destroy(SeaArena* arena) {
    if (!arena || !arena->base) return;
    munmap(arena->base, arena->size);
    arena->base       = NULL;
    arena->size       = 0;
    arena->offset     = 0;
    arena->high_water = 0;
}

void* sea_arena_alloc(SeaArena* arena, u64 size, u64 align) {
    if (!arena || !arena->base || size == 0) return NULL;

    /* Align the current offset */
    u64 aligned = (arena->offset + (align - 1)) & ~(align - 1);

    if (aligned + size > arena->size) return NULL;  /* Arena full */

    void* ptr = arena->base + aligned;
    arena->offset = aligned + size;

    /* Track peak usage */
    if (arena->offset > arena->high_water) {
        arena->high_water = arena->offset;
    }

    return ptr;
}

SeaSlice sea_arena_push_cstr(SeaArena* arena, const char* cstr) {
    if (!cstr) return SEA_SLICE_EMPTY;

    u32 len = 0;
    while (cstr[len]) len++;

    u8* dst = (u8*)sea_arena_alloc(arena, len + 1, 1);
    if (!dst) return SEA_SLICE_EMPTY;

    memcpy(dst, cstr, len);
    dst[len] = '\0';

    return (SeaSlice){ .data = dst, .len = len };
}

void* sea_arena_push_bytes(SeaArena* arena, const void* data, u64 len) {
    if (!data || len == 0) return NULL;

    void* dst = sea_arena_alloc(arena, len, 1);
    if (!dst) return NULL;

    memcpy(dst, data, len);
    return dst;
}
