/*
 * tool_spawn.c — Spawn a sub-agent from natural language
 *
 * Args: <task description>
 * Creates a one-shot agent call with a focused system prompt,
 * executes it, and returns the result. Useful for delegation.
 *
 * Example: spawn Summarize the file /tmp/report.txt in 3 bullet points
 */

#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_agent.h"
#include "seaclaw/sea_log.h"

#include <stdio.h>
#include <string.h>

/* External agent config from main.c */
extern SeaAgentConfig s_agent_cfg;

SeaError tool_spawn(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Usage: spawn <task description>\n"
            "Example: spawn Summarize the key points from this conversation");
        return SEA_OK;
    }

    /* Null-terminate the task */
    char task[2048];
    u32 tlen = args.len < sizeof(task) - 1 ? args.len : (u32)(sizeof(task) - 1);
    memcpy(task, args.data, tlen);
    task[tlen] = '\0';

    /* Build a focused system prompt for the sub-agent */
    char system_prompt[512];
    snprintf(system_prompt, sizeof(system_prompt),
        "You are a focused sub-agent. Complete the following task concisely. "
        "Do NOT use tools unless absolutely necessary. "
        "Return only the result, no preamble.");

    /* Create a temporary chat history with the system prompt + task */
    SeaChatMsg history[1];
    history[0].role = SEA_ROLE_SYSTEM;
    history[0].content = system_prompt;
    history[0].tool_call_id = NULL;
    history[0].tool_name = NULL;

    SEA_LOG_INFO("HANDS", "Spawning sub-agent for: %.80s%s",
                 task, tlen > 80 ? "..." : "");

    /* Call the agent */
    SeaAgentResult result = sea_agent_chat(&s_agent_cfg, history, 1, task, arena);

    if (result.error != SEA_OK) {
        char err_msg[256];
        int elen = snprintf(err_msg, sizeof(err_msg),
            "Sub-agent error: %s", result.text ? result.text : "unknown");
        u8* dst = (u8*)sea_arena_push_bytes(arena, err_msg, (u64)elen);
        if (dst) { output->data = dst; output->len = (u32)elen; }
        return SEA_OK;
    }

    if (result.text) {
        u32 rlen = (u32)strlen(result.text);
        u8* dst = (u8*)sea_arena_push_bytes(arena, (const u8*)result.text, rlen);
        if (dst) { output->data = dst; output->len = rlen; }
    } else {
        *output = SEA_SLICE_LIT("(sub-agent returned empty response)");
    }

    SEA_LOG_INFO("HANDS", "Sub-agent completed (tokens: %u)", result.tokens_used);
    return SEA_OK;
}
