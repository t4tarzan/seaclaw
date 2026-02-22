# SeaBot

> *"We stop building software that breaks. We start building logic that survives."*

A sovereign AI agent platform written in pure C11. 16 test suites. 217 assertions. 58 tools. No garbage collector. No runtime. No excuses.

**GitHub:** [github.com/t4tarzan/seabot](https://github.com/t4tarzan/seabot) &nbsp;|&nbsp; **Install:** `curl -fsSL https://raw.githubusercontent.com/t4tarzan/seabot/main/install.sh | bash`

---

## What Is This?

SeaBot is a sovereign computing engine — a single binary that runs an AI agent with:

- **Arena Allocation** — No malloc, no free, no leaks. Memory is a notebook: write forward, rip out the page.
- **Zero-Copy JSON Parser** — Parses directly into the input buffer. 3 μs per parse. No copies.
- **Grammar Shield** — Every byte is validated against a charset bitmap before it touches the engine. Shell injection, SQL injection, XSS — all rejected at the byte level in < 1 μs.
- **CLI Subcommand Dispatch** — `sea_claw doctor`, `sea_claw onboard`, `sea_claw version`, `sea_claw help`. Table-driven, extensible, backward-compatible with `--flag` mode.
- **Extension Interface** — Trait-like `SeaExtension` struct with init/destroy/health vtable. Register tools, channels, memory backends, and providers at runtime.
- **Dynamic Tool Registry** — DJB2 hash table for O(1) lookup. 58 static tools + up to 64 dynamic tools registered at runtime.
- **Skills System** — Markdown-based plugins with YAML frontmatter. Install from URL (`sea_skill_install`), auto-discover `AGENT.md` files walking CWD→root. Compatible with HF Skills / Claude Code / 99 format.
- **Auth & Token Framework** — Bearer tokens with 8-bit permissions bitmask (chat, tools, shell, files, network, admin, delegate, skills). Create, validate, revoke. Dev mode bypass.
- **Message Bus** — Thread-safe pub/sub bus decouples channels from the AI brain. Multi-channel ready.
- **Session Management** — Per-channel, per-chat session isolation with LLM-driven auto-summarization.
- **Long-Term Memory** — Markdown-based memory files: identity, daily notes, bootstrap files. Survives restarts.
- **Cron Scheduler** — Persistent background jobs with `@every`, `@once`, and cron expressions. SQLite-backed.
- **Telegram Bot** — Long-polling Telegram integration. Commands dispatched through the same shield → tool pipeline.
- **LLM Agent** — Routes natural language to OpenRouter/OpenAI/Gemini/Anthropic/local APIs. Multi-provider fallback chain. Parses tool calls, executes them, loops until done.
- **Usage Tracking** — Token consumption per provider, per day. SQLite-persisted audit trail.
- **Agent-to-Agent (A2A)** — Delegate tasks to remote agents via HTTP JSON-RPC. Shield-verified results.
- **SeaZero Hybrid** — C11 orchestrator + Python Agent Zero in Docker. Sandboxed, budget-controlled, PII-filtered.
- **Mesh Networking** — Captain/Crew architecture for on-prem distributed AI. HMAC auth, capability-based routing.

## Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                       SeaBot Binary                          │
├──────────┬──────────┬──────────┬─────────┬───────────────────┤
│ Substrate│  Senses  │  Shield  │  Brain  │      Hands        │
│(Arena,DB)│(JSON,HTTP)│(Grammar)│ (Agent) │ (58 Tools + Dyn)  │
├──────────┴──────────┴──────────┴─────────┴───────────────────┤
│  CLI ─── Extensions ─── Auth ─── Skills ─── Bus ─── A2A     │
│  Session ─── Memory ─── Cron ─── Usage ─── Recall ─── PII   │
├──────────────────────┬───────────────────────────────────────┤
│   TUI Mode           │  Telegram Mode + Gateway Mode        │
│  Interactive CLI      │  Long-polling bot + Bus-based        │
│  Subcommands + flags  │  Multi-channel ready                 │
├──────────────────────┴───────────────────────────────────────┤
│  SeaZero (opt-in): Agent Zero in Docker + LLM Proxy         │
│  Mesh (opt-in): Captain/Crew distributed AI on LAN          │
└──────────────────────────────────────────────────────────────┘
```

### Module Map

| Module | Directory | Purpose |
|--------|-----------|---------|
| **Substrate** | `src/core/` | Arena allocator, logging, SQLite DB, config |
| **Senses** | `src/senses/` | JSON parser, HTTP client |
| **Shield** | `src/shield/` | Byte-level grammar validation |
| **Brain** | `src/brain/` | LLM agent loop with tool calling + fallback |
| **Hands** | `src/hands/` | 58 tools + dynamic registration (hash table, O(1) lookup) |
| **CLI** | `src/cli/` | Subcommand dispatch: doctor, onboard, version, help |
| **Extensions** | `src/ext/` | Trait-based extension registry (tools, channels, memory, providers) |
| **Auth** | `src/auth/` | Bearer token auth with permissions bitmask |
| **Skills** | `src/skills/` | Markdown skills, URL install, AGENT.md auto-discovery |
| **Bus** | `src/bus/` | Thread-safe message bus (pub/sub, inbound/outbound queues) |
| **Channels** | `src/channels/` | Channel abstraction + Telegram channel adapter |
| **Session** | `src/session/` | Per-chat session isolation, LLM-driven summarization |
| **Memory** | `src/memory/` | Long-term markdown memory: identity, daily notes, bootstrap |
| **Cron** | `src/cron/` | Persistent job scheduler (@every, @once, cron expressions) |
| **Usage** | `src/usage/` | Token tracking per provider, per day |
| **Recall** | `src/recall/` | Keyword-based memory retrieval |
| **PII** | `src/pii/` | PII detection and redaction |
| **A2A** | `src/a2a/` | Agent-to-Agent delegation protocol |
| **Telegram** | `src/telegram/` | Bot interface (Mirror pattern) |
| **Mesh** | `src/mesh/` | Captain/Crew distributed AI on LAN |
| **SeaZero** | `seazero/` | Agent Zero bridge, LLM proxy, workspace manager |

## Platform Support

| Platform | Status | Install Method |
|----------|--------|----------------|
| **Linux** (Ubuntu, Debian, Fedora, Arch, Alpine) | ✅ Fully supported | One-liner or manual build |
| **macOS** (Intel & Apple Silicon) | ✅ Supported | Homebrew deps + manual build |
| **Windows** | 🔜 Coming soon | Use WSL2 for now |

---

## Install

### One-Line Install (Linux)

```bash
curl -fsSL https://raw.githubusercontent.com/t4tarzan/seabot/main/install.sh | bash
```

This interactive installer will:
1. Detect your package manager (apt/dnf/yum/pacman/apk)
2. Install build dependencies (gcc, make, libcurl, libsqlite3)
3. Clone the repo and build a release binary
4. Run all 16 test suites (217 assertions)
5. Walk you through LLM provider selection (OpenRouter/OpenAI/Gemini/Anthropic/Local)
6. Configure API keys and optional fallback providers
7. Optionally set up Telegram bot and Agent Zero
8. Generate `config.json` and launch SeaBot

### macOS

```bash
brew install curl sqlite3
git clone https://github.com/t4tarzan/seabot.git
cd seabot
make release
make test
sudo make install
```

### Windows (via WSL2)

```powershell
wsl --install
# Restart, then inside Ubuntu:
curl -fsSL https://raw.githubusercontent.com/t4tarzan/seabot/main/install.sh | bash
```

### Manual Build

```bash
# Ubuntu/Debian:
sudo apt install build-essential libcurl4-openssl-dev libsqlite3-dev
# Fedora/RHEL:
sudo dnf install gcc make libcurl-devel sqlite-devel
# Arch:
sudo pacman -S gcc make curl sqlite

git clone https://github.com/t4tarzan/seabot.git
cd seabot
make release          # Optimized binary
make test             # 16 suites, 217 assertions
sudo make install     # Installs to /usr/local/bin
```

---

## Getting Started

### Step 1: First-Run Setup

```bash
sea_claw onboard
# or: sea_claw --onboard
```

The onboard wizard prompts for:
- **LLM Provider** — OpenRouter (recommended), OpenAI, Google Gemini, Anthropic, or Local (Ollama/LM Studio)
- **API Key** — Paste your key (hidden input)
- **Model** — Accept the default or specify your own
- **Telegram Bot** (optional) — Token from [@BotFather](https://t.me/BotFather) + your chat ID

### Step 2: Verify Your Setup

```bash
sea_claw doctor
# or: sea_claw --doctor
```

Doctor checks: binary version, config, LLM provider, API keys, Telegram, fallback chain, database health, skills directory, tool count, extensions.

### Step 3: Launch

```bash
# Interactive TUI mode
sea_claw

# Specific config
sea_claw --config=~/.config/seabot/config.json

# Telegram bot mode
sea_claw --telegram YOUR_BOT_TOKEN YOUR_CHAT_ID

# Gateway mode (bus-based, multi-channel)
sea_claw --gateway --telegram YOUR_BOT_TOKEN

# Mesh mode (distributed)
sea_claw --mesh captain
sea_claw --mesh crew
```

### Step 4: Talk to Your Agent

```
🦀 > /help                          # Full command reference
🦀 > /tools                         # List all tools (58 static + dynamic)
🦀 > /exec echo Hello World         # Run a tool directly
🦀 > /status                        # System status
🦀 > /agents                        # List Agent Zero instances
🦀 > /delegate Analyze /tmp/*.csv   # Delegate to Agent Zero
🦀 > What files are in /tmp?        # Natural language → AI + tools
🦀 > /quit                          # Exit
```

---

## Configuration

### Config File

```json
{
  "telegram_token": "",
  "telegram_chat_id": 0,
  "db_path": "seabot.db",
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

| Variable | Provider |
|----------|----------|
| `OPENROUTER_API_KEY` | OpenRouter |
| `OPENAI_API_KEY` | OpenAI |
| `GEMINI_API_KEY` | Google Gemini |
| `ANTHROPIC_API_KEY` | Anthropic |
| `TELEGRAM_BOT_TOKEN` | Telegram |
| `EXA_API_KEY` | Exa Search |
| `BRAVE_API_KEY` | Brave Search |

Or use a `.env` file in the working directory — loaded automatically.

### CLI Reference

**Subcommands** (new):

| Command | Description |
|---------|-------------|
| `sea_claw doctor` | Diagnostic report |
| `sea_claw onboard` | Interactive setup wizard |
| `sea_claw version` | Print version |
| `sea_claw help` | Show all commands |

**Flags** (legacy, still supported):

| Flag | Description |
|------|-------------|
| `--config=<path>` | Config file (default: `config.json`) |
| `--db=<path>` | Database file (default: `seabot.db`) |
| `--telegram <token> <chat_id>` | Telegram bot mode |
| `--gateway` | Bus-based multi-channel mode |
| `--mesh captain\|crew` | Distributed mesh mode |
| `--doctor` | Diagnostic report |
| `--onboard` | Interactive setup wizard |
| `--version` | Print version |

---

## Skills System

### Built-in Skills

Skills are markdown files with YAML frontmatter + prompt body, loaded from `~/.seabot/skills/`:

```markdown
---
name: summarize
description: Summarize text concisely
trigger: /summarize
---
You are a summarization expert. Given the following text,
produce a concise summary capturing all key points...
```

### Install from URL

```bash
# Install a skill from a GitHub raw URL
sea_claw skill install https://raw.githubusercontent.com/.../summarize.md
```

Skills are validated (YAML frontmatter), Shield-checked (injection detection), and saved to the skills directory.

### AGENT.md Auto-Discovery

SeaBot walks from your current directory up to the filesystem root, loading any `AGENT.md` files it finds. This is compatible with:
- **HF Skills** format
- **Claude Code** AGENT.md
- **99 (ThePrimeagen)** AGENT.md
- **Cursor** rules

---

## Auth & Tokens

SeaBot includes a Bearer token auth framework for securing Gateway API, A2A delegation, and remote skill install.

```
Permissions bitmask (8 bits):
  CHAT       (0x01)  — Send/receive messages
  TOOLS      (0x02)  — Execute tools
  SHELL      (0x04)  — Run shell commands
  FILES      (0x08)  — Read/write files
  NETWORK    (0x10)  — HTTP requests
  ADMIN      (0x20)  — Config changes, token management
  DELEGATE   (0x40)  — Delegate to Agent Zero / A2A
  SKILLS     (0x80)  — Install/manage skills
```

Tokens are 64-char hex strings generated from `/dev/urandom`. Dev mode disables auth for local development.

---

## Telegram Bot

### Setup

1. Message [@BotFather](https://t.me/BotFather) → `/newbot` → get your token
2. Message [@userinfobot](https://t.me/userinfobot) → get your chat ID
3. Run: `sea_claw --telegram YOUR_TOKEN YOUR_CHAT_ID`

### Commands

| Command | Description |
|---------|-------------|
| `/help` | Full command reference |
| `/status` | System status & memory |
| `/tools` | List all tools |
| `/task list` | List tasks |
| `/task create <title>` | Create a task |
| `/file read <path>` | Read a file |
| `/shell <command>` | Run shell command |
| `/web <url>` | Fetch URL content |
| `/delegate <task>` | Delegate to Agent Zero |
| `/model` | Show/swap LLM model |
| `/think [off\|low\|medium\|high]` | Set thinking level |
| `/usage` | Token spend dashboard |
| `/audit` | Recent security events |

Or just type naturally — the bot uses AI + tools with conversation memory.

---

## SeaZero — C + Python Hybrid AI

> *The discipline of C. The autonomy of Python.*

SeaZero combines SeaBot (C11 orchestrator) with [Agent Zero](https://github.com/frdel/agent-zero) (Python autonomous agent). C handles orchestration, security, and memory. Python handles open-ended reasoning and code generation. Agent Zero runs in Docker — isolated, budget-controlled, and never sees your real API keys.

```
┌─────────────────────────────────────────────────────────────┐
│                    SeaBot (C11)                              │
│  Arena Memory │ Grammar Shield │ LLM Proxy │ 58 Tools       │
│  SQLite v3 DB │ PII Filter     │ Workspace │ Auth Framework  │
├───────────────┴────────────────┴──────┬─────┴───────────────┤
│            HTTP JSON Bridge           │  8 Security Layers  │
├───────────────────────────────────────┼─────────────────────┤
│         Docker Container (isolated)   │                     │
│   Agent Zero (Python)                 ▼                     │
│   • Autonomous reasoning + code generation                  │
│   • LLM via SeaBot proxy ONLY (never sees real API key)     │
│   • Seccomp + read-only rootfs + network isolation          │
└─────────────────────────────────────────────────────────────┘
```

### 8-Layer Security

| Layer | Protection |
|-------|-----------|
| 1 | Docker isolation — seccomp, read-only rootfs, no-new-privileges |
| 2 | Network isolation — bridge network, restricted DNS |
| 3 | Credential isolation — internal token only, never real API keys |
| 4 | Token budget — daily limit enforced by LLM proxy |
| 5 | Grammar Shield — byte-level validation of all output |
| 6 | PII filter — redacts emails, phones, SSNs, credit cards, IPs |
| 7 | Output size limit — 64KB max response |
| 8 | Full audit trail — every event logged to SQLite |

### Quick Start

```bash
# Install with Agent Zero support
curl -fsSL https://raw.githubusercontent.com/t4tarzan/seabot/main/install.sh | bash
# → Select "Yes" when asked about Agent Zero

make seazero-setup    # Pull Docker image
sea_claw
🦀 > /delegate Analyze the CSV files in /tmp and create a summary report
```

---

## Mesh — On-Prem Distributed AI

> *Your AI. Your network. Your rules. Zero data leakage.*

SeaBot Mesh turns every machine on your local network into an AI-powered node — coordinated by a Captain running a local LLM. **No data ever leaves your network.**

```
┌──────────────────────────────────────────────────────────┐
│                   YOUR LOCAL NETWORK                     │
│                                                          │
│   ┌────────────────────────────────────┐                 │
│   │   CAPTAIN  (M4 Mac, 64 GB RAM)    │                 │
│   │   Ollama (70B) + SeaBot + SQLite   │                 │
│   └──────────┬─────────────────────────┘                 │
│       ┌──────┼──────┬──────────┐                         │
│       ▼      ▼      ▼          ▼                         │
│   ┌──────┐┌──────┐┌──────┐┌────────┐                    │
│   │Laptop││Desk- ││ RPi  ││ Server │                    │
│   │files ││docker││GPIO  ││database│                    │
│   │shell ││build ││camera││backup  │                    │
│   └──────┘└──────┘└──────┘└────────┘                    │
└──────────────────────────────────────────────────────────┘
```

| Concern | Cloud AI | SeaBot Mesh |
|---------|----------|-------------|
| **Data sovereignty** | Messages → third-party servers | Nothing leaves your LAN |
| **Operating cost** | $0.01–$0.10 per message | $0 — local LLM |
| **Availability** | Internet outage = dead | Works offline |
| **Latency** | 200–2000 ms | 5–50 ms (LAN) |
| **Binary per node** | 200+ MB (Node.js) | ~2 MB (static C) |
| **RAM per node** | 150–300 MB | 8–17 MB |

### Quick Start

```bash
# Captain (central machine with GPU/LLM)
ollama pull qwen2.5:72b
sea_claw --mesh captain

# Crew (any machine on the LAN)
sea_claw --mesh crew
```

6-layer security: HMAC auth → IP allowlist → capability gating → input shield → output shield → audit trail.

---

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

---

## Test Results

```
16 test suites, 217 assertions, 0 failures
Build: 0 warnings under -Werror -Wpedantic

  Arena Tests:      9 passed  (1M allocs in 30ms, 1M resets in 7ms)
  JSON Tests:      17 passed  (100K parses in 300ms, 3 μs/parse)
  Shield Tests:    19 passed  (1M validations in 557ms, 0.6 μs/check)
  DB Tests:        10 passed  (SQLite WAL mode, CRUD + persistence)
  Config Tests:     6 passed  (JSON loader, defaults, partial configs)
  Bus Tests:       10 passed  (pub/sub, FIFO, concurrent, timeout)
  Session Tests:   11 passed  (isolation, history, summarization, DB)
  Memory Tests:     8 passed  (workspace, bootstrap, daily notes, context)
  Cron Tests:      14 passed  (schedule parsing, CRUD, tick, one-shot)
  Skill Tests:     12 passed  (parse, load, registry, enable/disable)
  Recall Tests:     6 passed  (keyword search, ranking, persistence)
  PII Tests:        8 passed  (email, phone, SSN, credit card, IP)
  SeaZero Tests:   18 passed  (bridge, proxy, workspace, delegation)
  Ext Tests:       19 passed  (extension registry, CLI dispatch, dynamic tools)
  Auth Tests:      14 passed  (tokens, permissions, revoke, skill install, AGENT.md)
```

---

## Project Structure

```
seabot/
├── include/seaclaw/       # Public headers (22 headers)
│   ├── sea_types.h        # Fixed-width types, SeaSlice, SeaError
│   ├── sea_arena.h        # Arena allocator
│   ├── sea_log.h          # Structured logging
│   ├── sea_json.h         # Zero-copy JSON parser
│   ├── sea_http.h         # HTTP client
│   ├── sea_shield.h       # Grammar filter
│   ├── sea_db.h           # SQLite database
│   ├── sea_config.h       # Config loader
│   ├── sea_agent.h        # LLM agent loop
│   ├── sea_tools.h        # Tool registry (static + dynamic)
│   ├── sea_cli.h          # CLI subcommand dispatch
│   ├── sea_ext.h          # Extension interface
│   ├── sea_auth.h         # Auth & token framework
│   ├── sea_skill.h        # Skills + URL install + AGENT.md
│   ├── sea_bus.h          # Message bus
│   ├── sea_session.h      # Session management
│   ├── sea_memory.h       # Long-term memory
│   ├── sea_cron.h         # Cron scheduler
│   ├── sea_recall.h       # Memory recall
│   ├── sea_a2a.h          # Agent-to-Agent protocol
│   ├── sea_mesh.h         # Mesh networking
│   └── sea_telegram.h     # Telegram bot
├── src/
│   ├── core/              # Arena, logging, DB, config
│   ├── senses/            # JSON, HTTP
│   ├── shield/            # Grammar validation
│   ├── brain/             # LLM agent
│   ├── hands/             # 58 tools + dynamic registration
│   ├── cli/               # Subcommand dispatch
│   ├── ext/               # Extension registry
│   ├── auth/              # Token auth
│   ├── skills/            # Skill plugins
│   ├── bus/               # Message bus
│   ├── channels/          # Channel adapters
│   ├── session/           # Session isolation
│   ├── memory/            # Long-term memory
│   ├── cron/              # Job scheduler
│   ├── recall/            # Memory recall
│   ├── usage/             # Token tracking
│   ├── pii/               # PII filter
│   ├── a2a/               # A2A delegation
│   ├── telegram/          # Telegram bot
│   ├── mesh/              # Mesh networking
│   └── main.c             # Entry point
├── seazero/               # Agent Zero integration
│   ├── bridge/            # C↔Python bridge, LLM proxy, workspace
│   ├── config/            # Seccomp profile
│   └── docker-compose.yml # Container config
├── tests/                 # 16 test suites, 217 assertions
├── config/                # Example configs
├── docs/                  # Architecture, roadmap, flow docs
├── Makefile               # Build system
└── install.sh             # Interactive installer
```

---

## Stats

| Metric | Value |
|--------|-------|
| C standard | C11 |
| External dependencies | libcurl, libsqlite3 |
| Test suites | 16 (217 assertions, 0 failures) |
| Static tools | 58 |
| Dynamic tool slots | 64 |
| LLM providers | OpenRouter, OpenAI, Gemini, Anthropic, Local |
| Fallback chain | Up to 4 providers |
| JSON parse speed | 3 μs/parse |
| Shield check speed | 0.6 μs/check |
| Arena alloc speed | 30 ns/alloc |
| Startup time | < 1 ms |
| Build | 0 warnings under `-Werror -Wpedantic` |

---

## Design Principles

1. **Zero-copy** — Data is never copied unless absolutely necessary. JSON values point into the original buffer.
2. **Arena allocation** — All memory comes from a fixed arena. Reset is instant. Leaks are impossible.
3. **Grammar-first security** — Input is validated at the byte level before parsing. If it doesn't fit the shape, it's rejected.
4. **Static + dynamic registry** — 58 tools compiled in. Dynamic tools registered at runtime via hash table. No eval. No surprises.
5. **Extension-first** — New capabilities (tools, channels, memory backends, providers) plug in via a trait-like interface without touching core code.
6. **Mirror pattern** — The UI (TUI or Telegram) reflects engine state. It never calculates.

---

## License

MIT

---

*The Cloud is Burning. The Vault Stands.*
