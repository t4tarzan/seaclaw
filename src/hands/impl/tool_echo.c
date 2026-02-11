/*
 * tool_echo.c — Simple echo tool for testing the registry
 */

#include "seaclaw/sea_tools.h"
#include <string.h>

SeaError tool_echo(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("(empty)");
        return SEA_OK;
    }

    /* Copy args into arena so output owns the data */
    u8* buf = (u8*)sea_arena_push_bytes(arena, args.data, args.len);
    if (!buf) return SEA_ERR_ARENA_FULL;

    output->data = buf;
    output->len  = args.len;
    return SEA_OK;
}
