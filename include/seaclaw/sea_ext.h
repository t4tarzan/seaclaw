/*
 * sea_ext.h — Extension Point Interface
 *
 * Trait-like structs for extending SeaClaw at compile time.
 * Extensions register tools, channels, and memory backends
 * through a uniform interface. All extensions are statically
 * compiled — no dynamic loading, no dlopen, no eval.
 *
 * "Extend the machine, but never break the cage."
 */

#ifndef SEA_EXT_H
#define SEA_EXT_H

#include "sea_types.h"
#include "sea_arena.h"

/* ── Extension Metadata ───────────────────────────────────── */

#define SEA_EXT_NAME_MAX    32
#define SEA_EXT_VERSION_MAX 16
#define SEA_EXT_MAX         32

typedef enum {
    SEA_EXT_TYPE_TOOL     = 0,  /* Adds tools to the registry */
    SEA_EXT_TYPE_CHANNEL  = 1,  /* Adds a communication channel */
    SEA_EXT_TYPE_MEMORY   = 2,  /* Adds a memory/recall backend */
    SEA_EXT_TYPE_PROVIDER = 3,  /* Adds an LLM provider */
} SeaExtType;

/* ── Extension Trait (vtable) ─────────────────────────────── */

typedef struct SeaExtension {
    /* Identity */
    char        name[SEA_EXT_NAME_MAX];
    char        version[SEA_EXT_VERSION_MAX];
    SeaExtType  type;
    bool        enabled;

    /* Lifecycle — called by SeaClaw core */
    SeaError  (*init)(struct SeaExtension* self, SeaArena* arena);
    void      (*destroy)(struct SeaExtension* self);

    /* Health check — returns 0-100 score */
    i32       (*health)(const struct SeaExtension* self);

    /* Opaque extension-specific data */
    void*       data;
} SeaExtension;

/* ── Extension Registry ───────────────────────────────────── */

typedef struct {
    SeaExtension*  extensions[SEA_EXT_MAX];
    u32            count;
} SeaExtRegistry;

/* Initialize the extension registry. */
void sea_ext_init(SeaExtRegistry* reg);

/* Register an extension. Returns SEA_OK or SEA_ERR_FULL. */
SeaError sea_ext_register(SeaExtRegistry* reg, SeaExtension* ext);

/* Find extension by name. Returns NULL if not found. */
SeaExtension* sea_ext_find(const SeaExtRegistry* reg, const char* name);

/* Initialize all registered extensions. */
SeaError sea_ext_init_all(SeaExtRegistry* reg, SeaArena* arena);

/* Destroy all registered extensions. */
void sea_ext_destroy_all(SeaExtRegistry* reg);

/* Get count of registered extensions. */
u32 sea_ext_count(const SeaExtRegistry* reg);

/* Get count by type. */
u32 sea_ext_count_by_type(const SeaExtRegistry* reg, SeaExtType type);

/* Compute aggregate health score (0-100). */
i32 sea_ext_health(const SeaExtRegistry* reg);

/* List extensions (for /extensions command). */
void sea_ext_list(const SeaExtRegistry* reg);

#endif /* SEA_EXT_H */
