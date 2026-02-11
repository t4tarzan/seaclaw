# Sea-Claw Sovereign System

> *"We stop building software that breaks. We start building logic that survives."*

A sovereign AI agent platform written in pure C11. 116 tests. 13,400+ lines. 56 tools. No garbage collector. No runtime. No excuses.

**By [One Convergence](https://oneconvergence.com)** — 25+ years of security & networking infrastructure.

**Docs:** [seaclaw.virtualgpt.cloud](https://seaclaw.virtualgpt.cloud) &nbsp;|&nbsp; **Install:** `curl -fsSL https://raw.githubusercontent.com/t4tarzan/seaclaw/main/install.sh | bash`

---

## What Is This?

Sea-Claw is a sovereign computing engine — a single binary that runs an AI agent with:

- **Arena Allocation** — No malloc, no free, no leaks. Memory is a notebook: write forward, rip out the page.
- **Zero-Copy JSON Parser** — Parses directly into the input buffer. 3 μs per parse. No copies.
- **Grammar Shield** — Every byte is validated against a charset bitmap before it touches the engine. Shell injection, SQL injection, XSS — all rejected at the byte level in < 1 μs.
- **Message Bus** — Thread-safe pub/sub bus decouples channels from the AI brain. Multi-channel ready.
- **Session Management** — Per-channel, per-chat session isolation with LLM-driven auto-summarization.
- **Long-Term Memory** — Markdown-based memory files: identity, daily notes, bootstrap files. Survives restarts.
- **Skills System** — Markdown-based plugins loaded from `~/.seaclaw/skills/`. YAML frontmatter + prompt body.
- **Cron Scheduler** — Persistent background jobs with `@every`, `@once`, and cron expressions. SQLite-backed.
- **Telegram Bot** — Long-polling Telegram integration. Commands dispatched through the same shield → tool pipeline.
- **LLM Agent** — Routes natural language to OpenRouter/OpenAI/Gemini/Anthropic/local APIs. Multi-provider fallback chain. Parses tool calls, executes them, loops until done.
- **Usage Tracking** — Token consumption per provider, per day. SQLite-persisted audit trail.
- **Agent-to-Agent (A2A)** — Delegate tasks to remote agents via HTTP JSON-RPC. Shield-verified results.
- **SQLite Database** — Embedded ledger for config, tasks, trajectory, chat history. Single file, WAL mode.
- **Static Tool Registry** — 56 tools compiled in. No dynamic loading. No eval. No surprises.

## Architecture

```
┌──────────────────────────────────────────────────────────┐
│                    Sea-Claw Binary                       │
├──────────┬──────────┬──────────┬─────────┬───────────────┤
│ Substrate│  Senses  │  Shield  │  Brain  │    Hands      │
│(Arena,DB)│(JSON,HTTP)│(Grammar)│ (Agent) │  (56 Tools)   │
├──────────┴──────────┴──────────┴─────────┴───────────────┤
│  Bus ─── Session ─── Memory ─── Cron ─── Skills ─── A2A │
├──────────────────────┬───────────────────────────────────┤
│   TUI Mode           │  Telegram Mode + Gateway Mode    │
│  Interactive CLI      │  Long-polling bot + Bus-based    │
│  --doctor / --onboard │  Multi-channel ready             │
└──────────────────────┴───────────────────────────────────┘
```

### The Five Pillars

| Pillar | Directory | Purpose |
|--------|-----------|---------|
| **Substrate** | `src/core/` | Arena allocator, logging, SQLite DB, config |
| **Senses** | `src/senses/` | JSON parser, HTTP client |
| **Shield** | `src/shield/` | Byte-level grammar validation |
| **Brain** | `src/brain/` | LLM agent loop with tool calling + fallback |
| **Hands** | `src/hands/` | 56 tools: file, shell, web, search, math, text, hash, DNS, SSL, weather, cron, memory, spawn |
| **Bus** | `src/bus/` | Thread-safe message bus (pub/sub, inbound/outbound queues) |
| **Channels** | `src/channels/` | Channel abstraction + Telegram channel adapter |
| **Session** | `src/session/` | Per-chat session isolation, LLM-driven summarization |
| **Memory** | `src/memory/` | Long-term markdown memory: identity, daily notes, bootstrap files |
| **Cron** | `src/cron/` | Persistent job scheduler (@every, @once, cron expressions) |
| **Skills** | `src/skills/` | Markdown-based skill plugins with YAML frontmatter |
| **Usage** | `src/usage/` | Token tracking per provider, per day |
| **A2A** | `src/a2a/` | Agent-to-Agent delegation protocol |
| **Telegram** | `src/telegram/` | Bot interface (Mirror pattern) |

## Install

### One-Line Install (Linux)

```bash
curl -fsSL https://raw.githubusercontent.com/t4tarzan/seaclaw/main/install.sh | bash
```

This installs dependencies, clones the repo, builds a release binary, runs all 116 tests, and installs `sea_claw` to `/usr/local/bin`. Supports apt, dnf, yum, pacman, and apk.

### Manual Build

```bash
# Requirements: gcc, make, libcurl4-openssl-dev, libsqlite3-dev
sudo apt install build-essential libcurl4-openssl-dev libsqlite3-dev

git clone https://github.com/t4tarzan/seaclaw.git
cd seaclaw

# Debug build (with AddressSanitizer)
make

# Release build
make release

# Run tests (116 tests, 10 suites)
make test

# First-run setup wizard
./sea_claw --onboard

# Diagnose config
./sea_claw --doctor

# Install to /usr/local/bin
sudo make install
```

### Run — Interactive Mode

```bash
./sea_claw
```

Commands:
- `/help` — Full command reference
- `/status` — System status (arena usage, uptime, tools)
- `/tools` — List all 56 registered tools
- `/exec <tool> <args>` — Execute a tool
- `/tasks` — List tasks from database
- `/quit` — Exit

Natural language input is validated through the Shield, then routed to the LLM agent with conversation memory.

### Configuration

```bash
cp config/config.example.json config.json
# Edit config.json with your API keys
./sea_claw --config config.json
```

Config fields: `telegram_token`, `telegram_chat_id`, `db_path`, `log_level`, `arena_size_mb`, `llm_provider` (`openai`/`anthropic`/`local`), `llm_api_key`, `llm_model`, `llm_api_url`, `llm_fallbacks` (array of fallback providers).

### Run — Telegram Bot Mode

```bash
./sea_claw --telegram YOUR_BOT_TOKEN --chat YOUR_CHAT_ID
```

- `--config <path>` — Config file (default: `config.json`)
- `--telegram <token>` — Bot token from @BotFather
- `--chat <id>` — Restrict to a single chat (0 = allow all)
- `--db <path>` — Database file (default: `seaclaw.db`)

### Telegram Slash Commands

| Command | Description |
|---------|-------------|
| `/help` | Full command reference |
| `/status` | System status & memory |
| `/tools` | List all 56 tools |
| `/task list` | List tasks |
| `/task create <title>` | Create a task |
| `/task done <id>` | Complete a task |
| `/file read <path>` | Read a file |
| `/file write <path>\|<content>` | Write a file |
| `/shell <command>` | Run shell command |
| `/web <url>` | Fetch URL content |
| `/session clear` | Clear chat history |
| `/model` | Show current LLM model |
| `/audit` | View recent audit trail |
| `/delegate <url> <task>` | Delegate to remote agent |
| `/exec <tool> <args>` | Raw tool execution |

Or just type naturally — the bot uses AI + tools with conversation memory.

## Test Results

```
Sea-Claw Arena Tests:     9 passed  (1M allocs in 30ms, 1M resets in 7ms)
Sea-Claw JSON Tests:     17 passed  (100K parses in 300ms, 3 μs/parse)
Sea-Claw Shield Tests:   19 passed  (1M validations in 557ms, 0.6 μs/check)
Sea-Claw DB Tests:       10 passed  (SQLite WAL mode, CRUD + persistence)
Sea-Claw Config Tests:    6 passed  (JSON loader, defaults, partial configs)
Sea-Claw Bus Tests:      10 passed  (pub/sub, FIFO, concurrent, timeout)
Sea-Claw Session Tests:  11 passed  (isolation, history, summarization, DB)
Sea-Claw Memory Tests:    8 passed  (workspace, bootstrap, daily notes, context)
Sea-Claw Cron Tests:     14 passed  (schedule parsing, CRUD, tick, one-shot)
Sea-Claw Skill Tests:    12 passed  (parse, load, registry, enable/disable)
─────────────────────────────────────────────
Total: 116 passed, 0 failed (10 suites)
```

## Security Model

The Shield validates every input at the byte level:

| Grammar | Allows | Blocks |
|---------|--------|--------|
| `SAFE_TEXT` | Printable ASCII + UTF-8 | Control chars, null bytes |
| `NUMERIC` | `0-9 . - + e E` | Everything else |
| `FILENAME` | `a-z A-Z 0-9 . - _ /` | Spaces, special chars |
| `URL` | RFC 3986 charset | Non-URL bytes |
| `COMMAND` | `/ a-z 0-9 space . _ -` | Shell metacharacters |

**Injection detection** catches: `$()`, backticks, `&&`, `||`, `;`, `|`, `../`, `<script>`, `javascript:`, `eval()`, SQL keywords (`DROP TABLE`, `UNION SELECT`, `OR 1=1`).

## Project Structure

```
seaclaw/
├── include/seaclaw/       # Public headers
│   ├── sea_types.h        # Fixed-width types, SeaSlice, SeaError
│   ├── sea_arena.h        # Arena allocator API
│   ├── sea_log.h          # Structured logging
│   ├── sea_json.h         # Zero-copy JSON parser
│   ├── sea_http.h         # Minimal HTTP client (+ auth headers)
│   ├── sea_shield.h       # Grammar filter
│   ├── sea_db.h           # SQLite embedded database
│   ├── sea_config.h       # JSON config loader
│   ├── sea_agent.h        # LLM agent loop + fallback
│   ├── sea_a2a.h          # Agent-to-Agent protocol
│   ├── sea_bus.h          # Thread-safe message bus
│   ├── sea_channel.h      # Channel abstraction
│   ├── sea_session.h      # Session management
│   ├── sea_memory.h       # Long-term memory
│   ├── sea_cron.h         # Cron scheduler
│   ├── sea_skill.h        # Skills plugin system
│   ├── sea_usage.h        # Usage tracking
│   ├── sea_telegram.h     # Telegram bot interface
│   └── sea_tools.h        # Static tool registry
├── src/
│   ├── core/              # Substrate: arena, logging, DB, config
│   ├── senses/            # I/O: JSON parser, HTTP client
│   ├── shield/            # Grammar validation
│   ├── brain/             # LLM agent with tool calling + fallback
│   ├── bus/               # Thread-safe pub/sub message bus
│   ├── channels/          # Channel manager + Telegram adapter
│   ├── session/           # Per-chat session isolation
│   ├── memory/            # Long-term markdown memory
│   ├── cron/              # Persistent job scheduler
│   ├── skills/            # Markdown skill plugins
│   ├── usage/             # Token usage tracking
│   ├── a2a/               # Agent-to-Agent delegation protocol
│   ├── telegram/          # Telegram bot
│   ├── hands/             # Tool registry + 56 implementations
│   └── main.c             # Event loop + config + agent wiring
├── tests/                 # 116 tests across 10 suites
├── config/                # Example config files
├── Makefile               # Build system
└── dist/                  # Release binary
```

## Design Principles

1. **Zero-copy** — Data is never copied unless absolutely necessary. JSON values point into the original buffer.
2. **Arena allocation** — All memory comes from a fixed arena. Reset is instant. Leaks are impossible.
3. **Grammar-first security** — Input is validated at the byte level before parsing. If it doesn't fit the shape, it's rejected.
4. **Static registry** — Tools are compiled in. No dynamic loading, no plugins, no attack surface.
5. **Mirror pattern** — The UI (TUI or Telegram) reflects engine state. It never calculates.

## Stats

| Metric | Value |
|--------|-------|
| Total source lines | 13,400+ |
| Source files | 95 |
| External dependencies | libcurl, libsqlite3 |
| C standard | C11 |
| Tests | 116 (10 suites, all passing) |
| Tools | 56 (file, shell, web, search, text, data, hash, DNS, SSL, weather, math, cron, memory, spawn, message) |
| Binary size | ~3 MB (debug), ~1.5 MB (release) |
| Startup time | < 1 ms |
| Peak memory | ~16 MB (idle) |
| Telegram commands | 15 |
| JSON parse speed | 3 μs/parse |
| Shield check speed | 0.6 μs/check |
| Arena alloc speed | 30 ns/alloc |
| LLM providers | OpenRouter, OpenAI, Gemini, Anthropic, Local |
| Fallback chain | Up to 4 providers |
| CLI commands | `--doctor`, `--onboard`, `--gateway` |
| A2A protocol | HTTP JSON-RPC with Shield verification |
| Documentation | [seaclaw.virtualgpt.cloud](https://seaclaw.virtualgpt.cloud) |

## License

MIT

---

*The Cloud is Burning. The Vault Stands.*
