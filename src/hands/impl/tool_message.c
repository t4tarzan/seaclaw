/*
 * tool_message.c — Send a message to any channel/chat from agent
 *
 * Args: <channel:chat_id> <message>
 * Publishes an outbound message to the bus for delivery.
 * If no bus is available, prints to stdout.
 *
 * Example: message telegram:12345 Hello from the agent!
 */

#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_bus.h"
#include "seaclaw/sea_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* External bus from main.c (may be NULL) */
extern SeaBus* s_bus_ptr;

SeaError tool_message(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT(
            "Usage: message <channel:chat_id> <text>\n"
            "Example: message telegram:12345 Hello from the agent!");
        return SEA_OK;
    }

    char buf[4096];
    u32 len = args.len < sizeof(buf) - 1 ? args.len : (u32)(sizeof(buf) - 1);
    memcpy(buf, args.data, len);
    buf[len] = '\0';

    /* Parse: <target> <message> */
    char* space = strchr(buf, ' ');
    if (!space) {
        *output = SEA_SLICE_LIT("Error: expected <channel:chat_id> <message>");
        return SEA_OK;
    }
    *space = '\0';
    char* target = buf;
    char* message = space + 1;
    while (*message == ' ') message++;

    if (strlen(message) == 0) {
        *output = SEA_SLICE_LIT("Error: empty message");
        return SEA_OK;
    }

    /* Parse channel:chat_id */
    char channel[32] = "stdout";
    i64 chat_id = 0;
    const char* colon = strchr(target, ':');
    if (colon) {
        u32 clen = (u32)(colon - target);
        if (clen > 0 && clen < sizeof(channel)) {
            memcpy(channel, target, clen);
            channel[clen] = '\0';
        }
        chat_id = atoll(colon + 1);
    }

    /* If bus is available, publish outbound */
    if (s_bus_ptr) {
        sea_bus_publish_outbound(s_bus_ptr, channel, chat_id,
                                 message, (u32)strlen(message));
        char* result = (char*)sea_arena_alloc(arena, 256, 1);
        if (!result) return SEA_ERR_ARENA_FULL;
        int rlen = snprintf(result, 256,
            "Message queued for %s:%lld (%zu bytes)",
            channel, (long long)chat_id, strlen(message));
        output->data = (const u8*)result;
        output->len = (u32)rlen;
    } else {
        /* No bus — print to stdout as fallback */
        printf("[MSG → %s:%lld] %s\n", channel, (long long)chat_id, message);
        char* result = (char*)sea_arena_alloc(arena, 256, 1);
        if (!result) return SEA_ERR_ARENA_FULL;
        int rlen = snprintf(result, 256,
            "Message printed to stdout (no bus): %s:%lld",
            channel, (long long)chat_id);
        output->data = (const u8*)result;
        output->len = (u32)rlen;
    }

    SEA_LOG_INFO("HANDS", "Message sent to %s:%lld", channel, (long long)chat_id);
    return SEA_OK;
}
