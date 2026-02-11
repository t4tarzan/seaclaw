# Sea-Claw Sovereign System

> *"We stop building software that breaks. We start building logic that survives."*

A zero-dependency AI agent platform written in pure C11. 61 tests. 4,938 lines. No garbage collector. No runtime. No excuses.

---

## What Is This?

Sea-Claw is a sovereign computing engine — a single binary that runs an AI agent with:

- **Arena Allocation** — No malloc, no free, no leaks. Memory is a notebook: write forward, rip out the page.
- **Zero-Copy JSON Parser** — Parses directly into the input buffer. 7 μs per parse. No copies.
- **Grammar Shield** — Every byte is validated against a charset bitmap before it touches the engine. Shell injection, SQL injection, XSS — all rejected at the byte level in < 1 μs.
- **Telegram Bot** — Long-polling Telegram integration. Commands dispatched through the same shield → tool pipeline.
- **LLM Agent** — Routes natural language to OpenAI/Anthropic/local APIs. Multi-provider fallback chain. Parses tool calls, executes them, loops until done.
- **Conversation Memory** — Chat history persisted in SQLite. Last 20 messages loaded as context per chat.
- **Agent-to-Agent (A2A)** — Delegate tasks to remote agents (OpenClaw, Agent-0) via HTTP JSON-RPC. Shield-verified results.
- **SQLite Database** — Embedded ledger for config, tasks, trajectory, chat history. Single file, WAL mode.
- **Static Tool Registry** — 8 tools compiled in. No dynamic loading. No eval. No surprises.

## Architecture

```
┌───────────────────────────────────────────────────┐
│                Sea-Claw Binary                    │
├──────────┬──────────┬───────────┬────────┬────────┤
│ Substrate│  Senses  │  Shield   │ Brain  │ Hands  │
│(Arena,DB)│(JSON,HTTP)│(Grammar) │(Agent) │(Tools) │
├──────────┴──────────┴───────────┴────────┴────────┤
│       Config → Event Loop → Memory (SQLite)       │
├─────────────────────┬─────────────────────────────┤
│    TUI Mode         │   Telegram Mode (13 cmds)   │
│  Interactive CLI    │  Long-polling bot + A2A      │
└─────────────────────┴─────────────────────────────┘
```

### The Five Pillars

| Pillar | Directory | Purpose |
|--------|-----------|---------|
| **Substrate** | `src/core/` | Arena allocator, logging, SQLite DB, config |
| **Senses** | `src/senses/` | JSON parser, HTTP client |
| **Shield** | `src/shield/` | Byte-level grammar validation |
| **Brain** | `src/brain/` | LLM agent loop with tool calling + fallback |
| **Hands** | `src/hands/` | 8 tools: file, shell, web, task, db_query, echo, status |
| **A2A** | `src/a2a/` | Agent-to-Agent delegation protocol |
| **Telegram** | `src/telegram/` | Bot interface (Mirror pattern) |

## Quick Start

### Build

```bash
# Requirements: gcc, make, libcurl4-openssl-dev, libsqlite3-dev
sudo apt install build-essential libcurl4-openssl-dev libsqlite3-dev

# Debug build (with AddressSanitizer)
make

# Release build (39KB stripped binary)
make release

# Run tests (61 tests)
make test
```

### Run — Interactive Mode

```bash
./sea_claw
```

Commands:
- `/help` — Full command reference
- `/status` — System status (arena usage, uptime, tools)
- `/tools` — List registered tools
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
| `/tools` | List all 8 tools |
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
Sea-Claw Arena Tests:    9 passed  (1M allocs in 30ms, 1M resets in 26ms)
Sea-Claw JSON Tests:    17 passed  (10K parses in 55ms, 5.5 μs/parse)
Sea-Claw Shield Tests:  19 passed  (100K validations in 94ms, 0.94 μs/check)
Sea-Claw DB Tests:      10 passed  (SQLite WAL mode, CRUD + persistence)
Sea-Claw Config Tests:   6 passed  (JSON loader, defaults, partial configs)
─────────────────────────────────────────────
Total: 61 passed, 0 failed
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
│   ├── sea_telegram.h     # Telegram bot interface
│   └── sea_tools.h        # Static tool registry
├── src/
│   ├── core/              # Substrate: arena, logging, DB, config
│   ├── senses/            # I/O: JSON parser, HTTP client
│   ├── shield/            # Grammar validation
│   ├── brain/             # LLM agent with tool calling + fallback
│   ├── a2a/               # Agent-to-Agent delegation protocol
│   ├── telegram/          # Telegram bot
│   ├── hands/             # Tool registry + 8 implementations
│   └── main.c             # Event loop + config + agent wiring
├── tests/                 # 61 tests across 5 suites
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
| Total source lines | 4,938 |
| External dependencies | libcurl, libsqlite3 |
| C standard | C11 |
| Tests | 61 |
| Tools | 8 (file_read, file_write, shell_exec, web_fetch, task_manage, db_query, echo, system_status) |
| Telegram commands | 15 |
| JSON parse speed | 5.5 μs/parse |
| Shield check speed | 0.94 μs/check |
| Arena alloc speed | 30 ns/alloc |
| LLM providers | OpenAI, Anthropic, Local (Ollama/LM Studio) |
| Fallback chain | Up to 4 providers |
| A2A protocol | HTTP JSON-RPC with Shield verification |

## License

MIT

---

*The Cloud is Burning. The Vault Stands.*
