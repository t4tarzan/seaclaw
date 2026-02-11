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

## Platform Support

| Platform | Status | Install Method |
|----------|--------|----------------|
| **Linux** (Ubuntu, Debian, Fedora, Arch, Alpine) | ✅ Fully supported | One-liner or manual build |
| **macOS** (Intel & Apple Silicon) | ✅ Supported | Homebrew deps + manual build |
| **Windows** | 🔜 Coming soon | Use WSL2 for now (see below) |

---

## Install

### One-Line Install (Linux)

```bash
curl -fsSL https://raw.githubusercontent.com/t4tarzan/seaclaw/main/install.sh | bash
```

This interactive installer will:
1. Detect your package manager (apt/dnf/yum/pacman/apk)
2. Install build dependencies (gcc, make, libcurl, libsqlite3)
3. Clone the repo and build a release binary (~203KB)
4. Run all 116 tests across 10 suites
5. Walk you through LLM provider selection (OpenRouter/OpenAI/Gemini/Anthropic/Local)
6. Configure API keys and optional fallback providers
7. Optionally set up a Telegram bot
8. Generate `config.json` and launch Sea-Claw

### macOS

```bash
# Install dependencies via Homebrew
brew install curl sqlite3

# Clone and build
git clone https://github.com/t4tarzan/seaclaw.git
cd seaclaw
make release
make test
sudo make install
```

### Windows (via WSL2)

```powershell
# 1. Open PowerShell as Administrator and install WSL2
wsl --install

# 2. Restart your computer, then open Ubuntu from Start Menu

# 3. Inside WSL2, run the one-liner:
curl -fsSL https://raw.githubusercontent.com/t4tarzan/seaclaw/main/install.sh | bash
```

Native Windows support is on the roadmap. WSL2 provides the full Linux experience with zero compromises.

### Manual Build (any Linux/macOS)

```bash
# Requirements: gcc (or clang), make, libcurl-dev, libsqlite3-dev
# Ubuntu/Debian:
sudo apt install build-essential libcurl4-openssl-dev libsqlite3-dev
# Fedora/RHEL:
sudo dnf install gcc make libcurl-devel sqlite-devel
# Arch:
sudo pacman -S gcc make curl sqlite

git clone https://github.com/t4tarzan/seaclaw.git
cd seaclaw

make release          # Optimized binary (~203KB stripped)
make test             # 116 tests, 10 suites
sudo make install     # Installs to /usr/local/bin
```

---

## Getting Started — Onboarding

### Step 1: First-Run Setup

```bash
sea_claw --onboard
```

The onboard wizard will prompt you for:
- **LLM Provider** — Choose from OpenRouter (recommended), OpenAI, Google Gemini, Anthropic, or Local (Ollama/LM Studio)
- **API Key** — Paste your key (hidden input). Get one at:
  - OpenRouter: https://openrouter.ai/keys
  - OpenAI: https://platform.openai.com/api-keys
  - Gemini: https://aistudio.google.com/apikey
  - Anthropic: https://console.anthropic.com/settings/keys
- **Model** — Accept the default or specify your own
- **Telegram Bot** (optional) — Token from [@BotFather](https://t.me/BotFather) + your chat ID

This generates a `config.json` file. You can edit it anytime.

### Step 2: Verify Your Setup

```bash
sea_claw --doctor
```

Doctor checks everything: binary version, config file, LLM provider, API keys, Telegram status, fallback chain, environment variables, database health, skills directory, and tool count. Green ✓ = good, red ✗ = needs attention.

### Step 3: Launch

```bash
# Interactive TUI mode
sea_claw

# Or with a specific config
sea_claw --config ~/.config/seaclaw/config.json

# Telegram bot mode
sea_claw --telegram YOUR_BOT_TOKEN --chat YOUR_CHAT_ID

# Gateway mode (bus-based, multi-channel ready)
sea_claw --gateway --telegram YOUR_BOT_TOKEN
```

### Step 4: Talk to Your Agent

In TUI mode, type commands or natural language:

```
🦀 > /help                          # Full command reference
🦀 > /tools                         # List all 56 tools
🦀 > /exec echo Hello World         # Run a tool directly
🦀 > /status                        # System status
🦀 > What files are in /tmp?        # Natural language → AI + tools
🦀 > Summarize this conversation    # Uses conversation memory
🦀 > /quit                          # Exit
```

---

## Configuration

### Config File

```bash
cp config/config.example.json config.json
```

```json
{
  "telegram_token": "",
  "telegram_chat_id": 0,
  "db_path": "seaclaw.db",
  "log_level": "info",
  "arena_size_mb": 16,
  "llm_provider": "openrouter",
  "llm_api_key": "sk-or-...",
  "llm_model": "moonshotai/kimi-k2.5",
  "llm_api_url": "",
  "llm_fallbacks": [
    { "provider": "openai", "model": "gpt-4o-mini" },
    { "provider": "gemini", "model": "gemini-2.0-flash" }
  ]
}
```

### Environment Variables

API keys can also be set via environment variables (override config):

| Variable | Provider |
|----------|----------|
| `OPENROUTER_API_KEY` | OpenRouter |
| `OPENAI_API_KEY` | OpenAI |
| `GEMINI_API_KEY` | Google Gemini |
| `ANTHROPIC_API_KEY` | Anthropic |
| `TELEGRAM_BOT_TOKEN` | Telegram |
| `EXA_API_KEY` | Exa Search |
| `BRAVE_API_KEY` | Brave Search |

Or put them in a `.env` file in the working directory — Sea-Claw loads it automatically.

### CLI Flags

| Flag | Description |
|------|-------------|
| `--config <path>` | Config file (default: `config.json`) |
| `--db <path>` | Database file (default: `seaclaw.db`) |
| `--telegram <token>` | Bot token from @BotFather |
| `--chat <id>` | Restrict to a single chat (0 = allow all) |
| `--gateway` | Bus-based multi-channel mode |
| `--doctor` | Diagnostic report |
| `--onboard` | Interactive setup wizard |

---

## Telegram Bot

### Setup

1. Message [@BotFather](https://t.me/BotFather) on Telegram → `/newbot` → get your token
2. Message [@userinfobot](https://t.me/userinfobot) → get your chat ID
3. Run: `sea_claw --telegram YOUR_TOKEN --chat YOUR_CHAT_ID`

### Slash Commands

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
