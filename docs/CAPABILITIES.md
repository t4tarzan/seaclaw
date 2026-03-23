# Sea-Claw: Capabilities & Features Reference

> **Version:** v2.0.0 (released February 2026)
> **Language:** C11 · **Binary:** ~82 KB · **Tests:** 116 passing · **Tools:** 57 compiled-in

---

## Table of Contents

1. [What Is Sea-Claw?](#1-what-is-sea-claw)
2. [Core Architecture](#2-core-architecture)
3. [The Five Pillars](#3-the-five-pillars)
4. [All 57 Tools](#4-all-57-tools)
5. [LLM & AI Capabilities](#5-llm--ai-capabilities)
6. [Session Management & Memory](#6-session-management--memory)
7. [Skills Plugin System](#7-skills-plugin-system)
8. [Cron Scheduler](#8-cron-scheduler)
9. [Security Model: The Shield](#9-security-model-the-shield)
10. [Telegram Bot Interface](#10-telegram-bot-interface)
11. [Agent-to-Agent (A2A) Protocol](#11-agent-to-agent-a2a-protocol)
12. [Sea-Claw Mesh (On-Prem Enterprise)](#12-sea-claw-mesh-on-prem-enterprise)
13. [CLI Reference](#13-cli-reference)
14. [Configuration](#14-configuration)
15. [Installation](#15-installation)
16. [Performance Benchmarks](#16-performance-benchmarks)
17. [Test Coverage](#17-test-coverage)
18. [Platform Support](#18-platform-support)
19. [Roadmap](#19-roadmap)

---

## 1. What Is Sea-Claw?

Sea-Claw is a **sovereign AI agent platform** written in pure C11. It runs as a single static binary — no runtime, no garbage collector, no dynamic loading — and provides a complete AI agent with tool calling, long-term memory, a cron scheduler, a plugin system, and multi-provider LLM routing, all secured by a byte-level grammar filter.

**In one sentence:** A 82 KB binary that turns any machine into an AI agent with 57 tools, Telegram integration, persistent memory, and cryptographically-enforced input validation — with zero cloud dependency when paired with a local LLM.

### Key Properties

| Property | Value |
|----------|-------|
| Language | C11 |
| Binary size | ~82 KB (release), ~3 MB (debug) |
| External dependencies | libcurl, libsqlite3 (2 total) |
| Startup time | < 1 ms |
| Peak RAM (idle) | ~16–17 MB |
| Tools compiled-in | 57 |
| Tests | 116 (13 suites, all passing) |
| LLM providers supported | 6 (OpenRouter, OpenAI, Gemini, Anthropic, Local, Fallback chain) |
| Input validation speed | 0.6 µs/check |
| JSON parse speed | 3 µs/parse |
| Arena alloc speed | 30 ns/alloc |

---

## 2. Core Architecture

```
┌──────────────────────────────────────────────────────────┐
│                    Sea-Claw Binary                       │
├──────────┬──────────┬──────────┬─────────┬───────────────┤
│ Substrate│  Senses  │  Shield  │  Brain  │    Hands      │
│(Arena,DB)│(JSON,HTTP)│(Grammar)│ (Agent) │  (57 Tools)   │
├──────────┴──────────┴──────────┴─────────┴───────────────┤
│  Bus ─── Session ─── Memory ─── Cron ─── Skills ─── A2A │
├──────────────────────┬───────────────────────────────────┤
│   TUI Mode           │  Telegram Mode + Gateway Mode    │
│  Interactive CLI      │  Long-polling bot + Bus-based    │
│  --doctor / --onboard │  Multi-channel ready             │
└──────────────────────┴───────────────────────────────────┘
```

**Data flow in gateway/Telegram mode:**
```
Channel (Telegram) → Bus (inbound queue) → Agent Loop → Bus (outbound queue) → Channel
```

Every message transits through the Shield (byte-level grammar validation) before reaching the agent.

---

## 3. The Five Pillars

| Pillar | Directory | Role |
|--------|-----------|------|
| **Substrate** | `src/core/` | Arena allocator, structured logging, SQLite DB, config loader |
| **Senses** | `src/senses/` | Zero-copy JSON parser, minimal HTTP client |
| **Shield** | `src/shield/` | Byte-level grammar validation — all input validated before parsing |
| **Brain** | `src/brain/` | LLM agent loop, tool calling, multi-provider fallback chain |
| **Hands** | `src/hands/` | 57 compiled-in tools: file, shell, web, search, text, data, hash, DNS, SSL, weather, math, cron, memory, spawn |

Supporting systems built on these pillars:

| System | Directory | Description |
|--------|-----------|-------------|
| **Bus** | `src/bus/` | Thread-safe pub/sub message bus (inbound/outbound queues) |
| **Channels** | `src/channels/` | Abstract channel interface + Telegram adapter |
| **Session** | `src/session/` | Per-chat session isolation, ring-buffer history, auto-summarization |
| **Memory** | `src/memory/` | Markdown-based long-term memory (identity, daily notes, bootstrap files) |
| **Cron** | `src/cron/` | Persistent job scheduler (`@every`, `@once`, cron expressions) |
| **Skills** | `src/skills/` | Markdown-based skill plugins with YAML frontmatter |
| **Usage** | `src/usage/` | Token consumption tracking per provider, per day |
| **A2A** | `src/a2a/` | Agent-to-Agent delegation over HTTP JSON-RPC |
| **Mesh** | `src/mesh/` | Captain/Crew on-premises mesh network mode |
| **PII Firewall** | `src/pii/` | Personally-identifiable information detection and blocking |
| **Recall** | `src/recall/` | Semantic recall engine for long-term memory retrieval |
| **Telegram** | `src/telegram/` | Telegram Bot API (Mirror pattern — no UI logic in adapter) |

---

## 4. All 57 Tools

Tools are compiled-in at build time (static registry). They are invokable via:
- Natural language → LLM agent automatically selects and calls tools
- `/exec <tool_name> <args>` in TUI or Telegram
- Direct C API: `sea_tool_exec("tool_name", args, &arena, &output)`

### Core (2 tools)

| Tool | Description |
|------|-------------|
| `echo` | Echoes input back; validates the tool pipeline end-to-end |
| `system_status` | Reports binary version, uptime, arena usage, DB path, tool count |

### File I/O (6 tools)

| Tool | Description |
|------|-------------|
| `file_read` | Read a file from disk into the arena |
| `file_write` | Write content to a file |
| `dir_list` | List directory contents |
| `file_info` | File metadata: size, permissions, modification time |
| `checksum_file` | Compute SHA-256 checksum of a file |
| `file_search` | Search for files matching a glob pattern |

### System (6 tools)

| Tool | Description |
|------|-------------|
| `shell_exec` | Execute a shell command (Shield-validated before exec) |
| `process_list` | List running processes |
| `env_get` | Read an environment variable |
| `disk_usage` | Disk space usage for a path |
| `syslog_read` | Read recent system log entries |
| `uptime` | System uptime and load averages |

### Network (8 tools)

| Tool | Description |
|------|-------------|
| `web_fetch` | HTTP GET — fetch URL content |
| `dns_lookup` | DNS query for a hostname |
| `net_info` | Network interface information |
| `http_request` | HTTP POST/PUT/DELETE with custom headers |
| `ip_info` | Geolocation and ASN info for an IP address |
| `whois_lookup` | WHOIS record for a domain |
| `ssl_check` | TLS certificate validity and expiry for a domain |
| `weather` | Current weather data for a location |

### Search (2 tools)

| Tool | Description |
|------|-------------|
| `exa_search` | AI-enhanced web search via Exa API |
| `web_search` | Web search via Brave Search API *(added v2.0)* |

### Text Processing (9 tools)

| Tool | Description |
|------|-------------|
| `text_summarize` | Summarize a block of text via the LLM |
| `text_transform` | Transform text: uppercase, lowercase, title, trim, reverse |
| `regex_match` | Apply a regex pattern to text, return matches |
| `diff_text` | Compute a line diff between two text blocks |
| `grep_text` | Search for a pattern within a text block |
| `wc` | Word, line, and character count |
| `head_tail` | Extract first/last N lines |
| `sort_text` | Sort lines alphabetically or numerically |
| `string_replace` | Find-and-replace within a string |

### Data Processing (4 tools)

| Tool | Description |
|------|-------------|
| `json_format` | Pretty-print or minify JSON |
| `csv_parse` | Parse CSV into a structured table |
| `json_query` | JMESPath-style query on a JSON document |
| `json_to_csv` | Convert a JSON array of objects to CSV |

### Encoding & Hashing (3 tools)

| Tool | Description |
|------|-------------|
| `hash_compute` | Compute MD5, SHA-1, SHA-256, or SHA-512 of a string |
| `url_parse` | Parse a URL into scheme, host, path, query components |
| `encode_decode` | Base64 encode/decode; URL encode/decode |

### Math & Utility (7 tools)

| Tool | Description |
|------|-------------|
| `timestamp` | Current UTC timestamp in ISO 8601 and Unix formats |
| `math_eval` | Safe arithmetic expression evaluator (no eval(), no shell) |
| `uuid_gen` | Generate a v4 UUID |
| `random_gen` | Cryptographically random bytes/string |
| `cron_parse` | Validate and describe a cron expression |
| `calendar` | Generate a calendar for a given month/year |
| `unit_convert` | Convert between units (length, weight, temperature, etc.) |

### Security (1 tool)

| Tool | Description |
|------|-------------|
| `password_gen` | Generate a strong random password with configurable charset |

### Developer (1 tool)

| Tool | Description |
|------|-------------|
| `count_lines` | Count lines of code in a file or directory |

### Task & Database (2 tools)

| Tool | Description |
|------|-------------|
| `task_manage` | Create, list, update, and complete tasks in the SQLite task table |
| `db_query` | Run a read-only SQL query against the embedded SQLite database |

### Agent Control (4 tools — added v2.0)

| Tool | Description |
|------|-------------|
| `edit_file` | Surgical find-and-replace within a file (no full rewrite) |
| `cron_manage` | Create, list, remove, pause, and resume cron jobs from agent context |
| `memory_manage` | Read, write, or append to long-term memory files and daily notes |
| `spawn` | Spawn a focused sub-agent from natural language; result returned to caller |
| `message` | Send a message to any channel/chat via the bus |

> **Note:** The total is 57 because `spawn` and `message` each count as one tool and the v2 additions bring the count from 50 to 57.

---

## 5. LLM & AI Capabilities

### Supported Providers

| Provider | Config key | Notes |
|----------|-----------|-------|
| OpenRouter | `openrouter` | Recommended — access to 200+ models |
| OpenAI | `openai` | GPT-4o, GPT-4o-mini, etc. |
| Google Gemini | `gemini` | Gemini 2.0 Flash, Pro, etc. |
| Anthropic | `anthropic` | Claude 3.5 Sonnet, Haiku, etc. |
| Local (Ollama/LM Studio) | `local` | Zero-cost, air-gapped operation |
| Fallback chain | Up to 4 providers | Automatic retry with exponential backoff |

### Agent Loop

The agent follows a **ReAct-style** loop:
1. Receives user message from Bus
2. Injects system context: identity, memory files, session history, available skills, tool manifest
3. Calls LLM with tool-calling enabled
4. Parses tool calls from response
5. Executes tools (Shield-validated args)
6. Feeds results back to LLM
7. Repeats until the LLM returns a final response with no tool calls
8. Sends response to Bus → back to the originating channel

### Streaming

The agent supports streaming responses: partial tokens are delivered to the channel as they arrive, providing low-latency feedback for long responses.

### Multi-Provider Fallback

If the primary provider fails (network error, rate limit, API error), Sea-Claw automatically retries with the next provider in the fallback chain, using exponential backoff. Up to 4 providers can be chained.

---

## 6. Session Management & Memory

### Session Management

Each channel+chat combination has its own isolated session:

- **Ring buffer** — keeps last 50 messages in RAM
- **SQLite persistence** — `sessions` and `session_messages` tables survive restarts
- **Auto-summarization** — when history grows long, the LLM generates a summary; old messages are pruned and the summary is injected into context
- **Multi-part summarization** — handles extremely long histories by splitting, summarizing each part, then merging
- **`/session clear`** — wipe history for the current chat

### Long-Term Memory

Memory files live at `~/.seaclaw/` and are markdown-formatted:

| File | Purpose |
|------|---------|
| `IDENTITY.md` | Agent's name, role, and persona |
| `SOUL.md` | Agent's values, personality, and behavioral guidelines |
| `USER.md` | Persistent facts about the user |
| `AGENTS.md` | Known remote agents and their capabilities |
| `MEMORY.md` | Index of memory entries (loaded every session) |
| `YYYYMM/YYYYMMDD.md` | Daily notes — agent writes observations here automatically |

At startup, all bootstrap files are read and injected into the system prompt so the agent always has full context.

The agent can read and write memory using the `memory_manage` tool via natural language:
- *"Remember that Alice prefers concise answers"* → agent writes to `USER.md`
- *"What do you remember about the project?"* → agent reads `MEMORY.md`

### Recall Engine

A semantic recall module (`src/recall/`) allows the agent to retrieve relevant past notes from daily memory files, enabling recall across sessions without injecting the entire history into every prompt.

---

## 7. Skills Plugin System

Skills extend agent behavior without recompiling. Each skill is a Markdown file with YAML frontmatter:

```markdown
---
name: weather-report
triggers: ["weather", "forecast", "temperature"]
enabled: true
---
When the user asks about weather, use the `weather` tool to fetch current conditions
for their location, then format the response with temperature, conditions, and a
3-day outlook if available.
```

### Skills Features

| Feature | Description |
|---------|-------------|
| Directory scan | Skills loaded from `~/.seaclaw/skills/` on startup |
| Trigger matching | Skills activated when message matches trigger keywords |
| Prompt injection | Active skill instructions appended to system prompt |
| Enable/disable | Skills can be toggled at runtime without restart |
| GitHub install | `sea_claw skills install <github-url>` downloads and installs a skill |
| Registry search | `sea_claw skills search <query>` searches available skills |

### Built-in Skills (4)

| Skill | Description |
|-------|-------------|
| `weather` | Detailed weather reporting with forecast |
| `github` | GitHub API interactions (issues, PRs, repos) |
| `summarize` | Multi-document and webpage summarization |
| `tmux` | Terminal multiplexer session management |

---

## 8. Cron Scheduler

Persistent background jobs stored in SQLite (`cron_jobs` + `cron_log` tables). Jobs survive restarts.

### Schedule Types

| Type | Example | Description |
|------|---------|-------------|
| `@every` | `@every 300` | Repeat every N seconds |
| `@once` | `@once 2026-04-01T09:00:00` | Run once at a specific datetime |
| Cron expression | `0 9 * * 1-5` | Standard 5-field cron (minute, hour, day, month, weekday) |

### Job Types

| Type | Description |
|------|-------------|
| Shell command | Execute a shell command and capture output |
| Tool call | Invoke a named Sea-Claw tool with arguments |
| Bus message | Send a message to a channel/chat |

### Usage

```
# Via TUI or Telegram natural language:
"Every morning at 9am send me the weather forecast"
→ Agent creates: cron_manage add "0 9 * * *" message "weather London"

# Direct CLI:
sea_claw cron list
sea_claw cron add "@every 3600" shell_exec "df -h /"
sea_claw cron remove <job-id>
sea_claw cron enable <job-id>
sea_claw cron disable <job-id>
```

---

## 9. Security Model: The Shield

Every byte of input is validated by the Grammar Shield before it reaches any other system component. Validation runs in under 1 µs.

### Grammar Table

| Grammar | Allowed Characters | Blocks |
|---------|-------------------|--------|
| `SAFE_TEXT` | Printable ASCII + valid UTF-8 sequences | Control chars, null bytes |
| `NUMERIC` | `0-9 . - + e E` | Everything else |
| `FILENAME` | `a-z A-Z 0-9 . - _ /` | Spaces, special characters |
| `URL` | RFC 3986 character set | Non-URL bytes |
| `COMMAND` | `/ a-z 0-9 space . _ -` | Shell metacharacters |
| `JSON` | JSON structural characters + SAFE_TEXT | Binary data, control characters |
| `BASE64` | `A-Za-z0-9+/=` | Everything else |
| `HEX` | `0-9 a-f A-F` | Everything else |
| `EMAIL` | RFC 5321 charset | Malformed addresses |
| `PATH` | Filesystem path charset | Null bytes, traversal sequences |

### Injection Detection

The Shield specifically detects and blocks:

| Attack Class | Detected Patterns |
|-------------|-------------------|
| Shell injection | `` $() `...` && || ; | `` |
| Path traversal | `../` sequences |
| XSS | `<script>`, `javascript:`, `onerror=` |
| Eval injection | `eval()` |
| SQL injection | `DROP TABLE`, `UNION SELECT`, `OR 1=1`, `--` |

### PII Firewall

A dedicated PII module (`src/pii/`) scans messages for personally-identifiable information patterns (email addresses, phone numbers, national ID formats) and can block or redact them before they are stored or sent to external LLM APIs.

### 6-Layer Mesh Security (Enterprise)

When running in Mesh mode, an additional 6 security layers apply:

| Layer | Mechanism |
|-------|-----------|
| 1 | Mesh Authentication — shared secret + HMAC tokens |
| 2 | IP Allowlist — only configured LAN subnets accepted |
| 3 | Capability Gating — nodes only execute advertised tools |
| 4 | Input Shield — byte-level grammar validation at every boundary |
| 5 | Output Shield — LLM responses scanned for prompt injection |
| 6 | Full Audit Trail — every task, tool call, and result logged to SQLite |

---

## 10. Telegram Bot Interface

Sea-Claw includes a fully functional Telegram bot that uses the long-polling Bot API. The bot implements the **Mirror pattern**: the Telegram adapter never contains logic — it translates bus messages to/from Telegram API calls only.

### Setup

```bash
# 1. Get a token from @BotFather on Telegram
# 2. Get your chat ID from @userinfobot
sea_claw --telegram YOUR_TOKEN --chat YOUR_CHAT_ID
# Or in gateway mode:
sea_claw --gateway --telegram YOUR_TOKEN
```

### Slash Commands

| Command | Description |
|---------|-------------|
| `/help` | Full command reference |
| `/status` | System status and memory usage |
| `/tools` | List all 57 tools |
| `/task list` | List all tasks |
| `/task create <title>` | Create a new task |
| `/task done <id>` | Mark a task complete |
| `/file read <path>` | Read a file |
| `/file write <path>\|<content>` | Write to a file |
| `/shell <command>` | Execute a shell command |
| `/web <url>` | Fetch and summarize a URL |
| `/session clear` | Clear current chat session history |
| `/model` | Show the active LLM model |
| `/audit` | View recent usage audit trail |
| `/delegate <url> <task>` | Delegate a task to a remote agent via A2A |
| `/exec <tool> <args>` | Invoke any tool directly |

### Natural Language

In addition to slash commands, typing any natural language message invokes the AI agent, which selects and calls tools automatically to fulfill the request.

---

## 11. Agent-to-Agent (A2A) Protocol

Sea-Claw can delegate tasks to other Sea-Claw instances (or any compatible agent) over HTTP JSON-RPC.

### How It Works

1. Caller sends a JSON-RPC request to the remote agent's endpoint
2. Request is Shield-validated at both ends
3. Remote agent processes the task with its own tools and LLM
4. Response is Shield-verified before being accepted
5. Result is returned to the originating user/channel

### Service Discovery (v2.0)

Agents can advertise their capabilities, allowing the caller to choose the most appropriate remote agent for a given task automatically.

### Usage

```
# Via Telegram or TUI:
/delegate https://agent.internal:9100 "summarize the logs in /var/log/app.log"

# Via agent natural language:
"Ask the build agent to run the test suite and report results"
```

---

## 12. Sea-Claw Mesh (On-Prem Enterprise)

Sea-Claw Mesh turns an entire local network into a coordinated AI agent cluster with zero cloud dependency.

### Architecture

```
YOUR LOCAL NETWORK (no data leaves this boundary)

  ┌──────────────────────────────────────┐
  │   CAPTAIN  (M4 Mac, 64 GB RAM)       │
  │   Ollama (70B model, Q8_0 KV cache)  │
  │   Sea-Claw captain mode (port 9100)  │
  │   SQLite (shared task queue + audit) │
  └──────────┬───────────────────────────┘
             │
    ┌────────┼────────┬──────────┐
    ▼        ▼        ▼          ▼
 Laptop   Desktop    RPi      Server
 (files,  (docker,  (GPIO,   (backup,
  shell,   build,   sensors)  deploy)
  web)     test)
```

### Captain / Crew Roles

| Role | Description |
|------|-------------|
| **Captain** | Runs the local LLM (Ollama). Receives user messages via Telegram/CLI. Routes tool calls to the appropriate Crew node based on capability matching. Collects results and formats the final response. |
| **Crew** | Lightweight 82 KB binary on any machine. Registers capabilities with Captain on startup. Executes tools locally. Returns results to Captain. |

### Capability-Based Routing

Each Crew node advertises a list of capabilities (e.g., `docker`, `gpio`, `database`). The Captain routes each tool call to the node that advertises the required capability. No tool call can be routed to a node that has not advertised the relevant capability (Capability Gating — Layer 3 security).

### Why Mesh Over Cloud AI?

| Concern | Cloud AI Agents | Sea-Claw Mesh |
|---------|----------------|---------------|
| Data sovereignty | Every message → third-party servers | Nothing leaves your LAN |
| Compliance | GDPR, HIPAA, SOC2 concerns | Air-gappable; full SQLite audit trail |
| Operating cost | $0.01–$0.10 per message | $0 — local LLM, no API fees |
| Availability | Requires internet | Works offline |
| Latency | 200–2000 ms (cloud round-trip) | 5–50 ms (LAN round-trip) |
| RAM per node | 150–300 MB (Node.js) | 8–17 MB (C binary) |
| Runs on | Servers only | Laptops, Raspberry Pi, routers — anything with a CPU |

### Use Cases

- **Dev teams** — Every laptop is a Crew node; the M4 Mac runs the LLM. Code review, test runs, and deployments coordinated via Telegram.
- **IoT / Industrial** — Raspberry Pi nodes monitor sensors and GPIO. The Captain analyzes and acts. No internet required.
- **Regulated industries** — Healthcare, finance, legal. Data stays on-premises. Air-gappable. Full audit trail.
- **Research labs** — Workstations share one powerful LLM; experiment data stays on-site.
- **Small business** — One Mac Mini as the brain; employee laptops as nodes. Zero recurring cost.

---

## 13. CLI Reference

### Launch Modes

| Command | Mode |
|---------|------|
| `sea_claw` | Interactive TUI — type commands or natural language |
| `sea_claw --telegram TOKEN --chat ID` | Telegram bot mode (single bot, long-polling) |
| `sea_claw --gateway --telegram TOKEN` | Gateway mode — all channels + cron + heartbeat concurrently |
| `sea_claw --mode captain --config captain.json` | Mesh Captain mode |
| `sea_claw --mode crew --config crew.json` | Mesh Crew mode |

### Flags

| Flag | Description |
|------|-------------|
| `--config <path>` | Config file path (default: `config.json`) |
| `--db <path>` | SQLite database file (default: `seaclaw.db`) |
| `--telegram <token>` | Telegram bot token |
| `--chat <id>` | Restrict to a single chat ID (0 = allow all) |
| `--gateway` | Bus-based multi-channel gateway mode |
| `--doctor` | Full diagnostic report (config, LLM, keys, DB, tools, skills) |
| `--onboard` | Interactive first-run setup wizard |
| `--health-report` | Machine-readable health status output |
| `--mode <captain\|crew>` | Mesh network role |

### TUI Commands

| Command | Description |
|---------|-------------|
| `/help` | Full command list |
| `/tools` | List all 57 tools |
| `/exec <tool> <args>` | Execute a tool directly |
| `/status` | System status (memory, DB, uptime) |
| `/session clear` | Clear current session history |
| `/model` | Show active LLM provider and model |
| `/audit` | Token usage audit for today |
| `/quit` | Exit Sea-Claw |

### Sub-commands

```bash
sea_claw skills list            # List installed skills
sea_claw skills install <url>   # Install skill from GitHub
sea_claw skills remove <name>   # Remove a skill
sea_claw skills search <query>  # Search skill registry

sea_claw cron list              # List all cron jobs
sea_claw cron add <expr> <cmd>  # Add a cron job
sea_claw cron remove <id>       # Remove a cron job
sea_claw cron enable <id>       # Enable a paused job
sea_claw cron disable <id>      # Disable a job
```

---

## 14. Configuration

### Config File (`config.json`)

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
    { "provider": "openai",  "model": "gpt-4o-mini" },
    { "provider": "gemini",  "model": "gemini-2.0-flash" },
    { "provider": "anthropic", "model": "claude-3-5-haiku-20241022" }
  ]
}
```

### Environment Variables

Environment variables override config file values. Sea-Claw also auto-loads a `.env` file from the working directory.

| Variable | Description |
|----------|-------------|
| `OPENROUTER_API_KEY` | OpenRouter API key |
| `OPENAI_API_KEY` | OpenAI API key |
| `GEMINI_API_KEY` | Google Gemini API key |
| `ANTHROPIC_API_KEY` | Anthropic API key |
| `TELEGRAM_BOT_TOKEN` | Telegram bot token |
| `EXA_API_KEY` | Exa Search API key |
| `BRAVE_API_KEY` | Brave Search API key |

### Log Levels

`debug` → `info` → `warn` → `error`. Default: `info`.

---

## 15. Installation

### One-Line Install (Linux)

```bash
curl -fsSL https://raw.githubusercontent.com/t4tarzan/seaclaw/main/install.sh | bash
```

Detects package manager (apt/dnf/yum/pacman/apk), installs dependencies, builds the binary, runs all 116 tests, and launches the onboarding wizard.

### One-Line Install (macOS)

```bash
curl -fsSL https://raw.githubusercontent.com/t4tarzan/seaclaw/main/install-mac.sh | bash
```

Handles Xcode Command Line Tools, Homebrew dependencies, and generates a `~/.config/seaclaw/config.json`.

### Windows

Use WSL2: install WSL2 from PowerShell, then run the Linux one-liner inside Ubuntu. Native Windows support is planned for v3.0.

### Manual Build

```bash
# Ubuntu/Debian
sudo apt install build-essential libcurl4-openssl-dev libsqlite3-dev

git clone https://github.com/t4tarzan/seaclaw.git
cd seaclaw
make release          # Optimized binary (~82 KB stripped)
make test             # 116 tests, 13 suites
sudo make install     # Installs to /usr/local/bin
```

### Post-Install

```bash
sea_claw --onboard    # Interactive setup wizard
sea_claw --doctor     # Verify everything is working
sea_claw              # Launch TUI
```

---

## 16. Performance Benchmarks

All benchmarks measured on a standard development workstation.

| Benchmark | Speed | Notes |
|-----------|-------|-------|
| Arena allocation | 30 ns/alloc | 1 million allocs in 30 ms |
| Arena reset | 7 µs | 1 million resets in 7 ms |
| JSON parsing | 3 µs/parse | 100K parses in 300 ms |
| Shield validation | 0.6 µs/check | 1 million checks in 557 ms |
| Startup time | < 1 ms | From exec to ready |
| Binary size (release) | ~82 KB | Stripped static binary |
| RAM (idle) | ~16–17 MB | Arena pre-allocated |
| RAM per Crew node | ~8–17 MB | Idle, LLM model not included |
| LLM tool loop (local) | 5–50 ms | LAN round-trip to Ollama |
| LLM tool loop (cloud) | 200–2000 ms | Cloud API round-trip |

---

## 17. Test Coverage

```
Sea-Claw Arena Tests:      9 passed  (1M allocs in 30ms, 1M resets in 7ms)
Sea-Claw JSON Tests:      17 passed  (100K parses in 300ms, 3 µs/parse)
Sea-Claw Shield Tests:    19 passed  (1M validations in 557ms, 0.6 µs/check)
Sea-Claw DB Tests:        10 passed  (SQLite WAL mode, CRUD + persistence)
Sea-Claw Config Tests:     6 passed  (JSON loader, defaults, partial configs)
Sea-Claw Bus Tests:       10 passed  (pub/sub, FIFO, concurrent, timeout)
Sea-Claw Session Tests:   11 passed  (isolation, history, summarization, DB)
Sea-Claw Memory Tests:     8 passed  (workspace, bootstrap, daily notes, context)
Sea-Claw Cron Tests:      14 passed  (schedule parsing, CRUD, tick, one-shot)
Sea-Claw Skill Tests:     12 passed  (parse, load, registry, enable/disable)
─────────────────────────────────────────────────────────────
Total: 116 passed, 0 failed (13 suites)
```

Run tests: `make test`

---

## 18. Platform Support

| Platform | Status | Notes |
|----------|--------|-------|
| Linux (Ubuntu, Debian, Fedora, Arch, Alpine) | Fully supported | One-liner installer |
| macOS (Intel & Apple Silicon) | Fully supported | Homebrew deps + macOS installer |
| Raspberry Pi (ARM) | Fully supported | Ships as a Crew node in Mesh deployments |
| Windows (native) | Planned for v3.0 | Use WSL2 in the meantime |
| Docker | Supported | `Dockerfile` and `Dockerfile.dev` included |

---

## 19. Roadmap

### Current: v2.0.0 (Released February 2026)

All v2.0 phases complete:

| Phase | Feature | Status |
|-------|---------|--------|
| 10 | Message Bus & Channel Architecture | Complete |
| 12 | Session Management & Long-Term Memory | Complete |
| 13 | Skills Plugin System | Complete |
| 14 | Cron Scheduler & Background Tasks | Complete |
| 16 | Gateway Mode & Concurrent Channels | Complete |
| 17 | Advanced Agent Features (7 new tools, streaming, usage tracking) | Complete |
| 18 | Polish, Benchmarks & Release | Complete |

### Deferred to v3.0

| Phase | Feature |
|-------|---------|
| 11 | Multi-channel: Discord, WhatsApp, Slack, Signal, WebChat |
| 14 (partial) | Dedicated `sea_subagent` module (currently handled by `tool_spawn`) |
| 15 | Voice/media pipeline (Whisper transcription via Groq) |
| — | Native Windows binary |
| — | GitHub Actions CI pipeline |
| — | Extension Registry with O(1) DJB2 hash tool lookup |
| — | Vector recall for long-term memory |
| — | SeaZero C+Python bridge (unmerged branch) |

---

## Source Code Statistics

| Metric | v1.0 | v2.0.0 |
|--------|------|--------|
| Source lines | ~9,159 | ~48,000+ |
| C source files | 74 | 78 |
| Tools compiled-in | 50 | 57 |
| Tests | 61 | 116 |
| Test suites | 5 | 13 |
| External dependencies | 2 | 2 |
| Binary size (release) | ~203 KB | ~82 KB |

---

## License

MIT — see `LICENSE` in the repository root.

---

*The Cloud is Burning. The Vault Stands.*
