/*
 * sea_agent.c — The Brain
 *
 * Routes natural language to LLM APIs (OpenAI-compatible),
 * parses tool call responses, executes tools, loops until done.
 * All allocations in arena.
 */

#include "seaclaw/sea_agent.h"
#include "seaclaw/sea_http.h"
#include "seaclaw/sea_json.h"
#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_shield.h"
#include "seaclaw/sea_db.h"
#include "seaclaw/sea_log.h"
#include "seaclaw/sea_memory.h"
#include "seaclaw/sea_recall.h"
#include "seaclaw/sea_pii.h"

#include <stdio.h>
#include <string.h>

/* External handles from main.c */
extern SeaDb* s_db;
extern SeaMemory* s_memory;
extern SeaRecall* s_recall;

/* ── Defaults ─────────────────────────────────────────────── */

static const char* DEFAULT_SYSTEM_PROMPT =
    "You are Sea-Claw, a sovereign AI agent running as a compact C binary. "
    "Answer the user's questions directly in plain text. "
    "Do NOT use tools unless the user explicitly asks for a system operation. "
    "Only use a tool if the user's request cannot be answered from your knowledge. "
    "Available tools (use ONLY when needed):\n";

static const char* MEMORY_INSTRUCTIONS =
    "\n## Memory\n"
    "You have persistent memory across conversations via the 'recall' tool.\n"
    "- When the user tells you their name, preferences, or important facts, "
    "use: recall remember user 8 <fact>\n"
    "- When the user states a preference, use: recall remember preference 7 <pref>\n"
    "- To recall relevant context, use: recall recall <query>\n"
    "- Important facts get importance 8-10, casual facts 3-5.\n"
    "- Do NOT announce that you are saving memory. Just do it silently.\n"
    "- Below is your current memory context (relevant facts for this query):\n";

void sea_agent_defaults(SeaAgentConfig* cfg) {
    if (!cfg) return;
    if (!cfg->api_url) {
        switch (cfg->provider) {
            case SEA_LLM_OPENAI:     cfg->api_url = "https://api.openai.com/v1/chat/completions"; break;
            case SEA_LLM_ANTHROPIC:  cfg->api_url = "https://api.anthropic.com/v1/messages"; break;
            case SEA_LLM_GEMINI:     cfg->api_url = "https://generativelanguage.googleapis.com/v1beta/openai/chat/completions"; break;
            case SEA_LLM_OPENROUTER: cfg->api_url = "https://openrouter.ai/api/v1/chat/completions"; break;
            case SEA_LLM_LOCAL:      cfg->api_url = "http://localhost:11434/v1/chat/completions"; break;
            case SEA_LLM_ZAI:        cfg->api_url = "https://api.z.ai/api/coding/paas/v4/chat/completions"; break;
        }
    }
    if (!cfg->model) {
        switch (cfg->provider) {
            case SEA_LLM_OPENAI:     cfg->model = "gpt-4o-mini"; break;
            case SEA_LLM_ANTHROPIC:  cfg->model = "claude-3-haiku-20240307"; break;
            case SEA_LLM_GEMINI:     cfg->model = "gemini-2.0-flash"; break;
            case SEA_LLM_OPENROUTER: cfg->model = "moonshotai/kimi-k2.5"; break;
            case SEA_LLM_LOCAL:      cfg->model = "llama3"; break;
            case SEA_LLM_ZAI:        cfg->model = "glm-5"; break;
        }
    }
    if (cfg->max_tokens == 0) cfg->max_tokens = 4096;
    if (cfg->temperature == 0.0) cfg->temperature = 0.7;
    if (cfg->max_tool_rounds == 0) cfg->max_tool_rounds = 5;

    /* Apply think level overrides */
    sea_agent_set_think_level(cfg, cfg->think_level);
}

void sea_agent_init(SeaAgentConfig* cfg) {
    sea_agent_defaults(cfg);
    const char* prov_name = "Unknown";
    switch (cfg->provider) {
        case SEA_LLM_OPENAI:     prov_name = "OpenAI"; break;
        case SEA_LLM_ANTHROPIC:  prov_name = "Anthropic"; break;
        case SEA_LLM_GEMINI:     prov_name = "Gemini"; break;
        case SEA_LLM_OPENROUTER: prov_name = "OpenRouter"; break;
        case SEA_LLM_LOCAL:      prov_name = "Local"; break;
        case SEA_LLM_ZAI:        prov_name = "Z.AI"; break;
    }
    SEA_LOG_INFO("AGENT", "Provider: %s, Model: %s", prov_name, cfg->model);
}

/* ── Think Level ──────────────────────────────────────── */

void sea_agent_set_think_level(SeaAgentConfig* cfg, SeaThinkLevel level) {
    if (!cfg) return;
    cfg->think_level = level;
    switch (level) {
        case SEA_THINK_OFF:
            cfg->temperature = 0.3;
            cfg->max_tokens = 1024;
            break;
        case SEA_THINK_LOW:
            cfg->temperature = 0.5;
            cfg->max_tokens = 2048;
            break;
        case SEA_THINK_MEDIUM:
            cfg->temperature = 0.7;
            cfg->max_tokens = 4096;
            break;
        case SEA_THINK_HIGH:
            cfg->temperature = 0.9;
            cfg->max_tokens = 8192;
            break;
    }
    SEA_LOG_INFO("AGENT", "Think level: %s (temp=%.1f, max_tokens=%u)",
                 sea_agent_think_level_name(level), cfg->temperature, cfg->max_tokens);
}

const char* sea_agent_think_level_name(SeaThinkLevel level) {
    switch (level) {
        case SEA_THINK_OFF:    return "off";
        case SEA_THINK_LOW:    return "low";
        case SEA_THINK_MEDIUM: return "medium";
        case SEA_THINK_HIGH:   return "high";
        default:               return "unknown";
    }
}

/* ── Model Hot-Swap ───────────────────────────────────── */

void sea_agent_set_model(SeaAgentConfig* cfg, const char* model) {
    if (!cfg || !model) return;
    cfg->model = model;
    SEA_LOG_INFO("AGENT", "Model hot-swapped to: %s", model);
}

void sea_agent_set_provider(SeaAgentConfig* cfg, SeaLlmProvider provider,
                             const char* api_key, const char* api_url) {
    if (!cfg) return;
    cfg->provider = provider;
    if (api_key) cfg->api_key = api_key;
    if (api_url) cfg->api_url = api_url;
    sea_agent_defaults(cfg);
    SEA_LOG_INFO("AGENT", "Provider hot-swapped to: %s (%s)", cfg->api_url, cfg->model);
}

/* ── String builder helpers ─────────────────────────────── */

typedef struct {
    char*     buf;
    u32       len;
    u32       cap;
    SeaArena* arena;
} StrBuf;

static StrBuf strbuf_new(SeaArena* arena, u32 initial_cap) {
    StrBuf sb = { .arena = arena, .len = 0, .cap = initial_cap };
    sb.buf = (char*)sea_arena_alloc(arena, initial_cap, 1);
    if (sb.buf) sb.buf[0] = '\0';
    return sb;
}

static void strbuf_append(StrBuf* sb, const char* s) {
    if (!sb->buf || !s) return;
    u32 slen = (u32)strlen(s);
    if (sb->len + slen >= sb->cap) {
        u32 new_cap = sb->cap * 2;
        while (new_cap <= sb->len + slen) new_cap *= 2;
        char* new_buf = (char*)sea_arena_alloc(sb->arena, new_cap, 1);
        if (!new_buf) return;
        memcpy(new_buf, sb->buf, sb->len);
        sb->buf = new_buf;
        sb->cap = new_cap;
    }
    memcpy(sb->buf + sb->len, s, slen);
    sb->len += slen;
    sb->buf[sb->len] = '\0';
}

/* ── JSON escape ──────────────────────────────────────────── */

static void strbuf_append_json_escaped(StrBuf* sb, const char* s) {
    if (!s) return;
    for (const char* p = s; *p; p++) {
        switch (*p) {
            case '"':  strbuf_append(sb, "\\\""); break;
            case '\\': strbuf_append(sb, "\\\\"); break;
            case '\n': strbuf_append(sb, "\\n");  break;
            case '\r': strbuf_append(sb, "\\r");  break;
            case '\t': strbuf_append(sb, "\\t");  break;
            default: {
                char c[2] = { *p, '\0' };
                strbuf_append(sb, c);
            }
        }
    }
}

/* ── Build system prompt with tool descriptions ───────────── */

const char* sea_agent_build_system_prompt(SeaArena* arena) {
    StrBuf sb = strbuf_new(arena, 2048);
    strbuf_append(&sb, DEFAULT_SYSTEM_PROMPT);

    u32 count = sea_tools_count();
    for (u32 i = 0; i < count; i++) {
        const SeaTool* t = sea_tool_by_id(i);
        if (t) {
            strbuf_append(&sb, "- ");
            strbuf_append(&sb, t->name);
            strbuf_append(&sb, ": ");
            strbuf_append(&sb, t->description);
            strbuf_append(&sb, "\n");
        }
    }

    strbuf_append(&sb,
        "\nTo call a tool, include this exact JSON in your response:\n"
        "{\"tool_call\": {\"name\": \"tool_name\", \"args\": \"arguments\"}}\n"
        "After the tool result is returned, provide your final answer to the user.");

    return sb.buf;
}

/* ── Build OpenAI-compatible request JSON ─────────────────── */

static const char* build_request_json(SeaAgentConfig* cfg,
                                       const char* system_prompt,
                                       SeaChatMsg* history, u32 history_count,
                                       const char* user_input,
                                       SeaArena* arena) {
    StrBuf sb = strbuf_new(arena, 4096);

    strbuf_append(&sb, "{\"model\":\"");
    strbuf_append(&sb, cfg->model);
    strbuf_append(&sb, "\",\"max_tokens\":");

    char num[32];
    snprintf(num, sizeof(num), "%u", cfg->max_tokens);
    strbuf_append(&sb, num);

    snprintf(num, sizeof(num), ",\"temperature\":%.1f", cfg->temperature);
    strbuf_append(&sb, num);

    strbuf_append(&sb, ",\"messages\":[");

    /* System message */
    strbuf_append(&sb, "{\"role\":\"system\",\"content\":\"");
    strbuf_append_json_escaped(&sb, system_prompt);
    strbuf_append(&sb, "\"}");

    /* History */
    for (u32 i = 0; i < history_count; i++) {
        strbuf_append(&sb, ",{\"role\":\"");
        switch (history[i].role) {
            case SEA_ROLE_USER:      strbuf_append(&sb, "user"); break;
            case SEA_ROLE_ASSISTANT: strbuf_append(&sb, "assistant"); break;
            case SEA_ROLE_TOOL:      strbuf_append(&sb, "user"); break; /* tool results as user */
            default:                 strbuf_append(&sb, "user"); break;
        }
        strbuf_append(&sb, "\",\"content\":\"");
        strbuf_append_json_escaped(&sb, history[i].content);
        strbuf_append(&sb, "\"}");
    }

    /* Current user message */
    strbuf_append(&sb, ",{\"role\":\"user\",\"content\":\"");
    strbuf_append_json_escaped(&sb, user_input);
    strbuf_append(&sb, "\"}]}");

    return sb.buf;
}

/* ── Parse tool call from response ────────────────────────── */

typedef struct {
    bool        has_tool_call;
    const char* tool_name;
    const char* tool_args;
    const char* text;
} ParsedResponse;

static ParsedResponse parse_llm_response(const char* body, u32 body_len,
                                          SeaArena* arena) {
    ParsedResponse pr = { .has_tool_call = false, .tool_name = NULL,
                          .tool_args = NULL, .text = NULL };

    SeaSlice input = { .data = (const u8*)body, .len = body_len };
    SeaJsonValue root;
    if (sea_json_parse(input, arena, &root) != SEA_OK) {
        SEA_LOG_ERROR("AGENT", "Failed to parse LLM response JSON");
        return pr;
    }

    /* Extract choices[0].message.content */
    const SeaJsonValue* choices = sea_json_get(&root, "choices");
    if (!choices || choices->type != SEA_JSON_ARRAY || choices->array.count == 0) {
        SEA_LOG_ERROR("AGENT", "No choices in LLM response");
        return pr;
    }

    const SeaJsonValue* first = &choices->array.items[0];
    const SeaJsonValue* message = sea_json_get(first, "message");
    if (!message) {
        SEA_LOG_ERROR("AGENT", "No message in choice");
        return pr;
    }

    SeaSlice content_slice = sea_json_get_string(message, "content");
    if (content_slice.len == 0) {
        pr.text = "";
        return pr;
    }

    /* Copy content to null-terminated string */
    char* content = (char*)sea_arena_alloc(arena, content_slice.len + 1, 1);
    if (!content) return pr;
    memcpy(content, content_slice.data, content_slice.len);
    content[content_slice.len] = '\0';
    pr.text = content;

    /* The zero-copy JSON parser returns slices into the original buffer,
     * so string content still has escaped quotes (\"). We need to unescape
     * the content before searching for tool_call JSON blocks. */
    char* unescaped = (char*)sea_arena_alloc(arena, content_slice.len + 1, 1);
    if (unescaped) {
        u32 ui = 0;
        for (u32 ci = 0; ci < content_slice.len; ci++) {
            if (content[ci] == '\\' && ci + 1 < content_slice.len) {
                char next = content[ci + 1];
                if (next == '"')       { unescaped[ui++] = '"';  ci++; }
                else if (next == '\\') { unescaped[ui++] = '\\'; ci++; }
                else if (next == 'n')  { unescaped[ui++] = '\n'; ci++; }
                else if (next == 'r')  { unescaped[ui++] = '\r'; ci++; }
                else if (next == 't')  { unescaped[ui++] = '\t'; ci++; }
                else                   { unescaped[ui++] = content[ci]; }
            } else {
                unescaped[ui++] = content[ci];
            }
        }
        unescaped[ui] = '\0';
        pr.text = unescaped;
    }

    /* Check if content contains a tool_call JSON block */
    const char* search = pr.text;
    const char* tc_start = strstr(search, "{\"tool_call\"");
    if (!tc_start) tc_start = strstr(search, "{ \"tool_call\"");
    if (tc_start) {
        /* Find the matching closing brace */
        int depth = 0;
        const char* p = tc_start;
        const char* end = NULL;
        for (; *p; p++) {
            if (*p == '{') depth++;
            else if (*p == '}') {
                depth--;
                if (depth == 0) { end = p + 1; break; }
            }
        }

        if (end) {
            u32 tc_len = (u32)(end - tc_start);
            SeaSlice tc_input = { .data = (const u8*)tc_start, .len = tc_len };
            SeaJsonValue tc_root;
            if (sea_json_parse(tc_input, arena, &tc_root) == SEA_OK) {
                const SeaJsonValue* tc = sea_json_get(&tc_root, "tool_call");
                if (tc) {
                    SeaSlice name_sl = sea_json_get_string(tc, "name");
                    SeaSlice args_sl = sea_json_get_string(tc, "args");

                    if (name_sl.len > 0) {
                        char* name = (char*)sea_arena_alloc(arena, name_sl.len + 1, 1);
                        if (name) {
                            memcpy(name, name_sl.data, name_sl.len);
                            name[name_sl.len] = '\0';
                            pr.tool_name = name;
                        }

                        char* args = (char*)sea_arena_alloc(arena, args_sl.len + 1, 1);
                        if (args) {
                            memcpy(args, args_sl.data, args_sl.len);
                            args[args_sl.len] = '\0';
                            pr.tool_args = args;
                        }

                        pr.has_tool_call = true;
                        SEA_LOG_INFO("AGENT", "Detected tool call: %s(%s)",
                                     pr.tool_name, pr.tool_args ? pr.tool_args : "");
                    }
                }
            }
        }
    }

    return pr;
}

/* ── Build auth header ────────────────────────────────────── */

static const char* build_auth_header(SeaAgentConfig* cfg, SeaArena* arena) {
    if (!cfg->api_key || !cfg->api_key[0]) return NULL;

    const char* prefix;
    if (cfg->provider == SEA_LLM_ANTHROPIC) {
        prefix = "x-api-key: ";
    } else {
        prefix = "Authorization: Bearer ";
    }

    u32 plen = (u32)strlen(prefix);
    u32 klen = (u32)strlen(cfg->api_key);
    char* hdr = (char*)sea_arena_alloc(arena, plen + klen + 1, 1);
    if (!hdr) return NULL;
    memcpy(hdr, prefix, plen);
    memcpy(hdr + plen, cfg->api_key, klen);
    hdr[plen + klen] = '\0';
    return hdr;
}

/* ── Main agent chat loop ─────────────────────────────────── */

SeaAgentResult sea_agent_chat(SeaAgentConfig* cfg,
                              SeaChatMsg* history, u32 history_count,
                              const char* user_input,
                              SeaArena* arena) {
    SeaAgentResult result = { .text = NULL, .tool_calls = 0,
                              .tokens_used = 0, .error = SEA_OK };

    if (!cfg || !user_input || !arena) {
        result.error = SEA_ERR_CONFIG;
        return result;
    }

    if (!cfg->api_key && cfg->provider != SEA_LLM_LOCAL) {
        result.error = SEA_ERR_CONFIG;
        result.text = "No API key configured. Set telegram_token in config.json or use --config.";
        SEA_LOG_ERROR("AGENT", "No API key configured");
        return result;
    }

    /* Build system prompt with tool descriptions */
    const char* base_prompt = cfg->system_prompt
        ? cfg->system_prompt
        : sea_agent_build_system_prompt(arena);

    /* Inject memory context: SOUL + USER from files, recall facts from DB */
    const char* system_prompt = base_prompt;
    {
        StrBuf mp = strbuf_new(arena, 8192);
        strbuf_append(&mp, base_prompt);

        /* Bootstrap identity from markdown files (compact) */
        if (s_memory) {
            const char* soul = sea_memory_read_bootstrap(s_memory, "SOUL.md");
            if (soul) {
                strbuf_append(&mp, "\n## Personality\n");
                strbuf_append(&mp, soul);
                strbuf_append(&mp, "\n");
            }
            const char* user_profile = sea_memory_read_bootstrap(s_memory, "USER.md");
            if (user_profile) {
                strbuf_append(&mp, "\n## User Profile\n");
                strbuf_append(&mp, user_profile);
                strbuf_append(&mp, "\n");
            }
        }

        /* Memory instructions + relevant facts from recall DB */
        strbuf_append(&mp, MEMORY_INSTRUCTIONS);
        if (s_recall) {
            const char* recall_ctx = sea_recall_build_context(s_recall, user_input, arena);
            if (recall_ctx) {
                strbuf_append(&mp, recall_ctx);
            } else {
                strbuf_append(&mp, "(No relevant facts stored yet.)\n");
            }
        }

        system_prompt = mp.buf;
    }

    /* Build auth header */
    const char* auth_hdr = build_auth_header(cfg, arena);

    /* Accumulate tool messages for multi-round */
    #define MAX_EXTRA_MSGS 16
    SeaChatMsg extra_msgs[MAX_EXTRA_MSGS];
    u32 extra_count = 0;

    const char* current_input = user_input;

    for (u32 round = 0; round < cfg->max_tool_rounds; round++) {
        /* Build combined history: original + extra tool messages */
        u32 total_hist = history_count + extra_count;
        SeaChatMsg* combined = NULL;
        if (total_hist > 0) {
            combined = (SeaChatMsg*)sea_arena_alloc(arena,
                total_hist * sizeof(SeaChatMsg), 8);
            if (combined) {
                if (history_count > 0)
                    memcpy(combined, history, history_count * sizeof(SeaChatMsg));
                if (extra_count > 0)
                    memcpy(combined + history_count, extra_msgs,
                           extra_count * sizeof(SeaChatMsg));
            }
        }

        /* Build request JSON */
        const char* req_json = build_request_json(cfg, system_prompt,
            combined, total_hist, current_input, arena);
        if (!req_json) {
            result.error = SEA_ERR_OOM;
            result.text = "Failed to build request";
            return result;
        }

        SEA_LOG_INFO("AGENT", "Round %u: sending %u bytes to %s",
                     round + 1, (u32)strlen(req_json), cfg->api_url);

        /* Make HTTP request — with fallback chain */
        SeaSlice body = { .data = (const u8*)req_json, .len = (u32)strlen(req_json) };
        SeaHttpResponse resp;
        SeaError err = SEA_ERR_IO;
        bool got_response = false;

        /* Try primary provider */
        if (auth_hdr) {
            err = sea_http_post_json_auth(cfg->api_url, body, auth_hdr, arena, &resp);
        } else {
            err = sea_http_post_json(cfg->api_url, body, arena, &resp);
        }
        if (err == SEA_OK && resp.status_code == 200) {
            got_response = true;
        } else {
            SEA_LOG_WARN("AGENT", "Primary provider failed (err=%d, http=%d), trying fallbacks...",
                         err, (err == SEA_OK) ? resp.status_code : 0);
            if (err == SEA_OK && resp.body.len > 0) {
                SEA_LOG_WARN("AGENT", "Error body: %.*s",
                             (int)(resp.body.len > 200 ? 200 : resp.body.len),
                             (const char*)resp.body.data);
            }
        }

        /* Try fallback providers */
        for (u32 fb = 0; fb < cfg->fallback_count && !got_response; fb++) {
            SeaLlmFallback* f = &cfg->fallbacks[fb];

            /* Build fallback-specific config for request JSON */
            SeaAgentConfig fb_cfg = *cfg;
            fb_cfg.provider = f->provider;
            fb_cfg.api_key  = f->api_key;
            fb_cfg.api_url  = f->api_url;
            fb_cfg.model    = f->model;
            sea_agent_defaults(&fb_cfg);

            const char* fb_json = build_request_json(&fb_cfg, system_prompt,
                combined, total_hist, current_input, arena);
            if (!fb_json) continue;

            const char* fb_auth = build_auth_header(&fb_cfg, arena);
            SeaSlice fb_body = { .data = (const u8*)fb_json, .len = (u32)strlen(fb_json) };

            SEA_LOG_INFO("AGENT", "Fallback %u: trying %s (%s)",
                         fb + 1, fb_cfg.api_url, fb_cfg.model);

            if (fb_auth) {
                err = sea_http_post_json_auth(fb_cfg.api_url, fb_body, fb_auth, arena, &resp);
            } else {
                err = sea_http_post_json(fb_cfg.api_url, fb_body, arena, &resp);
            }

            if (err == SEA_OK && resp.status_code == 200) {
                got_response = true;
                SEA_LOG_INFO("AGENT", "Fallback %u succeeded (%s)", fb + 1, fb_cfg.model);
            } else {
                SEA_LOG_WARN("AGENT", "Fallback %u failed (err=%d, http=%d)",
                             fb + 1, err, (err == SEA_OK) ? resp.status_code : 0);
            }
        }

        if (!got_response) {
            result.error = err;
            if (err == SEA_OK && resp.status_code != 200) {
                char* err_text = (char*)sea_arena_alloc(arena, resp.body.len + 64, 1);
                if (err_text) {
                    snprintf(err_text, resp.body.len + 64,
                             "LLM API error (HTTP %d): %.*s",
                             resp.status_code, (int)resp.body.len, (const char*)resp.body.data);
                    result.text = err_text;
                } else {
                    result.text = "LLM API error";
                }
            } else {
                result.text = "All LLM providers failed";
            }
            SEA_LOG_ERROR("AGENT", "All providers exhausted");
            return result;
        }

        /* Parse response */
        ParsedResponse pr = parse_llm_response(
            (const char*)resp.body.data, resp.body.len, arena);

        if (!pr.has_tool_call) {
            /* No tool call — we have the final answer */
            /* Shield-verify output before returning */
            if (pr.text && strlen(pr.text) > 0) {
                SeaSlice out_slice = { .data = (const u8*)pr.text,
                                       .len = (u32)strlen(pr.text) };
                if (sea_shield_detect_output_injection(out_slice)) {
                    SEA_LOG_WARN("AGENT", "Shield REJECTED LLM output (injection)");
                    result.text = "[Output rejected by Shield: potential injection detected]";
                    result.error = SEA_ERR_INVALID_INPUT;
                    return result;
                }
            }
            /* PII Firewall: redact PII from output if enabled */
            if (cfg->pii_categories && pr.text && strlen(pr.text) > 0) {
                SeaSlice pii_slice = { .data = (const u8*)pr.text,
                                       .len = (u32)strlen(pr.text) };
                const char* redacted = sea_pii_redact(pii_slice, cfg->pii_categories, arena);
                if (redacted) {
                    pr.text = redacted;
                }
            }
            result.text = pr.text ? pr.text : "";
            return result;
        }

        /* Tool call requested */
        result.tool_calls++;
        SEA_LOG_INFO("AGENT", "Tool call: %s(%s)",
                     pr.tool_name ? pr.tool_name : "?",
                     pr.tool_args ? pr.tool_args : "");

        /* Validate tool name through shield */
        if (pr.tool_name) {
            SeaSlice name_slice = {
                .data = (const u8*)pr.tool_name,
                .len = (u32)strlen(pr.tool_name)
            };
            SeaShieldResult sr = sea_shield_validate(name_slice, SEA_GRAMMAR_COMMAND);
            if (!sr.valid) {
                result.text = "Tool name rejected by Shield.";
                result.error = SEA_ERR_INVALID_INPUT;
                return result;
            }
        }

        /* Execute tool */
        SeaSlice tool_args = {
            .data = (const u8*)(pr.tool_args ? pr.tool_args : ""),
            .len = pr.tool_args ? (u32)strlen(pr.tool_args) : 0
        };
        SeaSlice tool_output;
        SeaError tool_err = sea_tool_exec(pr.tool_name, tool_args, arena, &tool_output);

        /* Audit: log tool execution */
        if (s_db && pr.tool_name) {
            char audit[512];
            snprintf(audit, sizeof(audit), "tool=%s args=%.*s status=%s",
                     pr.tool_name,
                     pr.tool_args ? (int)strlen(pr.tool_args) : 0,
                     pr.tool_args ? pr.tool_args : "",
                     tool_err == SEA_OK ? "ok" : "error");
            sea_db_log_event(s_db, "tool_exec", pr.tool_name, audit);
        }

        /* Build tool result message for next round */
        if (extra_count < MAX_EXTRA_MSGS - 1) {
            /* Add assistant message with tool call */
            extra_msgs[extra_count].role = SEA_ROLE_ASSISTANT;
            extra_msgs[extra_count].content = pr.text;
            extra_msgs[extra_count].tool_call_id = NULL;
            extra_msgs[extra_count].tool_name = NULL;
            extra_count++;

            /* Add tool result */
            char* tool_result = (char*)sea_arena_alloc(arena, tool_output.len + 128, 1);
            if (tool_result) {
                if (tool_err == SEA_OK) {
                    snprintf(tool_result, tool_output.len + 128,
                             "Tool '%s' returned: %.*s",
                             pr.tool_name,
                             (int)tool_output.len, (const char*)tool_output.data);
                } else {
                    snprintf(tool_result, 128,
                             "Tool '%s' failed with error %d",
                             pr.tool_name, tool_err);
                }
                extra_msgs[extra_count].role = SEA_ROLE_TOOL;
                extra_msgs[extra_count].content = tool_result;
                extra_msgs[extra_count].tool_call_id = NULL;
                extra_msgs[extra_count].tool_name = pr.tool_name;
                extra_count++;
            }
        }

        /* Next round uses the tool result as context */
        current_input = "Please provide your final answer based on the tool result above.";
    }

    /* Exhausted tool rounds */
    result.text = "Reached maximum tool call rounds.";
    result.error = SEA_ERR_TIMEOUT;
    return result;
}

/* ── Compact: summarize conversation history ──────────────── */

const char* sea_agent_compact(SeaAgentConfig* cfg,
                               SeaChatMsg* history, u32 history_count,
                               SeaArena* arena) {
    if (!cfg || !history || history_count == 0 || !arena) return NULL;

    /* Build a summarization request */
    StrBuf sb = strbuf_new(arena, 4096);
    strbuf_append(&sb, "Summarize the following conversation in 2-3 concise paragraphs. "
                       "Preserve key facts, decisions, and action items. "
                       "Do NOT include greetings or filler.\n\n");

    for (u32 i = 0; i < history_count; i++) {
        const char* role_str = "user";
        if (history[i].role == SEA_ROLE_ASSISTANT) role_str = "assistant";
        else if (history[i].role == SEA_ROLE_TOOL) role_str = "tool";
        strbuf_append(&sb, role_str);
        strbuf_append(&sb, ": ");
        if (history[i].content) strbuf_append(&sb, history[i].content);
        strbuf_append(&sb, "\n");
    }

    /* Use a temporary config with lower tokens for the summary */
    SeaAgentConfig compact_cfg = *cfg;
    compact_cfg.max_tokens = 512;
    compact_cfg.temperature = 0.3;
    compact_cfg.max_tool_rounds = 0; /* No tool calls during compaction */
    compact_cfg.stream_cb = NULL;    /* No streaming for compaction */

    SeaAgentResult ar = sea_agent_chat(&compact_cfg, NULL, 0, sb.buf, arena);

    if (ar.error == SEA_OK && ar.text && strlen(ar.text) > 0) {
        SEA_LOG_INFO("AGENT", "Compacted %u messages into summary (%u chars)",
                     history_count, (u32)strlen(ar.text));
        return ar.text;
    }

    SEA_LOG_WARN("AGENT", "Compaction failed: %s", ar.text ? ar.text : "unknown");
    return NULL;
}
