# Changelog

## v2.0.0 — February 2026

### New Modules

- **Message Bus** (`sea_bus.h/c`) — Thread-safe pub/sub message bus with inbound/outbound queues, session key generation, and configurable timeouts. Foundation for multi-channel architecture.

- **Channel Architecture** (`sea_channel.h/c`) — Abstract channel interface with lifecycle management. Telegram channel adapter (`channel_telegram.c`) bridges the bus to Telegram Bot API.

- **Session Management** (`sea_session.h/c`) — Per-channel, per-chat session isolation. In-memory history ring buffer (50 msgs max), LLM-driven auto-summarization, SQLite persistence (`sessions` + `session_messages` tables).

- **Long-Term Memory** (`sea_memory.h/c`) — File-based markdown memory system at `~/.seaclaw/`. Bootstrap files (IDENTITY.md, SOUL.md, USER.md, AGENTS.md, MEMORY.md), daily notes with monthly directories, context builder for system prompt injection.

- **Cron Scheduler** (`sea_cron.h/c`) — Persistent background job scheduler. Schedule types: `@every` (interval), `@once` (one-shot), standard cron expressions. Job types: shell command, tool call, bus message. SQLite-backed (`cron_jobs` + `cron_log` tables).

- **Skills System** (`sea_skill.h/c`) — Markdown-based skill plugins with YAML frontmatter. Directory scanning, trigger-based lookup, enable/disable at runtime, prompt builder for LLM injection.

- **Usage Tracking** (`sea_usage.h/c`) — Token consumption tracking per provider, per day. Rolling 30-day window. SQLite-persisted (`usage_stats` table). Human-readable summary formatter.

### New Tools (56 total, +6 new)

| # | Tool | Description |
|---|------|-------------|
| 51 | `edit_file` | Surgical find-and-replace within files |
| 52 | `cron_manage` | Create/list/remove/pause/resume cron jobs |
| 53 | `memory_manage` | Read/write/append long-term memory and daily notes |
| 54 | `web_search` | Brave Search API integration |
| 55 | `spawn` | Spawn focused sub-agent from natural language |
| 56 | `message` | Send message to any channel/chat via bus |

### New CLI Commands

- `--doctor` — Full diagnostic report: config, LLM provider, API keys, Telegram status, fallbacks, environment variables, database health, skills directory, tool count. Color-coded indicators.

- `--onboard` — Interactive first-run setup wizard. Prompts for LLM provider, API key, model, and optional Telegram credentials. Generates `config.json`.

### New HTTP API

- `sea_http_get_auth()` — HTTP GET with custom auth header. Used by Brave Search API integration.

### Architecture Changes

- `s_agent_cfg` made non-static for sub-agent spawning via `tool_spawn`
- `s_bus_ptr` global for tool-level bus access via `tool_message`
- `s_cron`, `s_memory`, `s_usage` globals wired to tools and initialized at startup
- Skills registry loaded from `~/.seaclaw/skills/` at startup
- All new subsystems cleaned up on shutdown

### Test Suite

- **116 tests** across **10 suites** (was 61 tests, 5 suites)
- New suites: Bus (10), Session (11), Memory (8), Cron (14), Skill (12)
- Performance benchmark suite (`test_bench.c`)

### Stats

| Metric | v1.0 | v2.0 |
|--------|------|------|
| Source lines | 9,159 | 13,400+ |
| Source files | 74 | 95 |
| Tools | 50 | 56 |
| Tests | 61 | 116 |
| Test suites | 5 | 10 |
| Headers | 12 | 20 |

---

## v1.0.0 — February 2026

Initial release. 50 tools, 5 LLM providers, Telegram bot, A2A protocol, SQLite persistence, arena allocation, grammar shield, zero-copy JSON parser.
