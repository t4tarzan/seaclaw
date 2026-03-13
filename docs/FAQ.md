# Sea-Claw FAQ

Frequently asked questions about Sea-Claw — the sovereign AI agent platform written in C11.

---

## Table of Contents

- [General](#general)
- [Installation & Setup](#installation--setup)
- [Configuration](#configuration)
- [Using Sea-Claw](#using-sea-claw)
- [Skills & Cron](#skills--cron)
- [Architecture & Development](#architecture--development)
- [SeaZero (C+Python Hybrid)](#seazero-cpython-hybrid)
- [Sea-Claw Mesh (Enterprise)](#sea-claw-mesh-enterprise)
- [Security](#security)
- [Troubleshooting](#troubleshooting)

---

## General

### What is Sea-Claw?

Sea-Claw is a sovereign AI agent platform — a single binary written in pure C11 that runs an AI agent with 57 compiled-in tools, a Grammar Shield, an Arena allocator, and a SQLite-backed memory system. It supports interactive TUI mode, Telegram bot mode, and gateway multi-channel mode.

Key numbers at a glance:

| Metric | Value |
|--------|-------|
| Language | C11 |
| Source lines | 13,400+ |
| Source files | 95 |
| Tools | 57 (static, compiled-in) |
| Tests | 116 across 13 suites |
| External dependencies | 2: libcurl, libsqlite3 |
| Binary size | ~203KB (release) |
| Startup time | < 1ms |

### What platforms are supported?

| Platform | Status | Notes |
|----------|--------|-------|
| **Linux** (Ubuntu, Debian, Fedora, Arch, Alpine) | Fully supported | One-line installer or manual build |
| **macOS** (Intel & Apple Silicon) | Supported | Homebrew dependencies + manual build |
| **Windows** | Not natively supported | Use WSL2 — see the [Installation](#installation--setup) section |

### What are the system requirements?

- **Compiler:** gcc 11+ or clang (C11 standard)
- **Build tools:** make 4.x
- **Runtime libraries:** libcurl 7.x, libsqlite3 3.x
- **RAM:** ~16 MB idle (configurable via `arena_size_mb`)
- **Disk:** ~203KB for the release binary + `seaclaw.db` for persistent state

### Is Sea-Claw open source?

Yes. Sea-Claw is released under the MIT license. The source is at [github.com/t4tarzan/seaclaw](https://github.com/t4tarzan/seaclaw).

---

## Installation & Setup

### How do I install Sea-Claw?

**Linux (one-line install):**

```bash
curl -fsSL https://raw.githubusercontent.com/t4tarzan/seaclaw/main/install.sh | bash
```

The installer detects your package manager (apt/dnf/yum/pacman/apk), installs build dependencies, clones the repo, compiles the binary, runs all 116 tests, and walks you through LLM provider selection.

**macOS:**

```bash
brew install curl sqlite3
git clone https://github.com/t4tarzan/seaclaw.git
cd seaclaw
make release
sudo make install
```

**Windows — use WSL2:**

```powershell
# In PowerShell (as Administrator):
wsl --install
# Restart, then open Ubuntu and run the Linux one-liner
```

Native Windows support is on the roadmap. WSL2 provides the full Linux experience.

### How do I build manually?

```bash
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

For a debug build with AddressSanitizer:

```bash
make all              # Debug build with -O0 -g -fsanitize=address,undefined
```

### How do I run the interactive setup?

```bash
sea_claw --onboard
```

The onboard wizard prompts for:
- **LLM provider** — OpenRouter (recommended), OpenAI, Gemini, Anthropic, or Local (Ollama/LM Studio)
- **API key** — entered securely (hidden input)
- **Model** — accept the default or specify your own
- **Telegram bot** (optional) — token from [@BotFather](https://t.me/BotFather) + your chat ID

The wizard writes a `config.json` file you can edit at any time.

### What does --doctor do?

```bash
sea_claw --doctor
```

`--doctor` runs a full self-check: binary version, config file presence, LLM provider and API key validity, Telegram status, fallback chain, environment variables, database health, skills directory, and tool count. Each item is shown with a green ✓ (good) or red ✗ (needs attention). Run this first when diagnosing any issue.

---

## Configuration

### Where is the config file?

By default, Sea-Claw reads `config.json` from the current working directory. You can specify a different path:

```bash
sea_claw --config ~/.config/seaclaw/config.json
```

To create a config from the example template:

```bash
cp config/config.example.json config.json
```

### How do I set API keys?

Two options — either in `config.json`:

```json
{
  "llm_provider": "openrouter",
  "llm_api_key": "sk-or-..."
}
```

Or via environment variables (which always override `config.json`):

| Variable | Provider |
|----------|----------|
| `OPENROUTER_API_KEY` | OpenRouter |
| `OPENAI_API_KEY` | OpenAI |
| `GEMINI_API_KEY` | Google Gemini |
| `ANTHROPIC_API_KEY` | Anthropic |
| `TELEGRAM_BOT_TOKEN` | Telegram |
| `EXA_API_KEY` | Exa Search |
| `BRAVE_API_KEY` | Brave Search |

### Can I use environment variables instead of config.json?

Yes. Environment variables always take precedence over `config.json` for API keys. You can also put them in a `.env` file — Sea-Claw loads it automatically **from the working directory where `sea_claw` is launched**. The `.env` file must be in that exact directory; it is not read from `~/.seaclaw/` or other locations.

Example `.env`:

```bash
OPENROUTER_API_KEY=sk-or-...
OPENAI_API_KEY=sk-...
TELEGRAM_BOT_TOKEN=123456:ABC...
```

### What LLM providers are supported?

Sea-Claw supports five provider types:

| Provider | Config value | Notes |
|----------|-------------|-------|
| OpenRouter | `"openrouter"` | Recommended — access to many models |
| OpenAI | `"openai"` | GPT-4o, GPT-4o-mini, etc. |
| Google Gemini | `"gemini"` | Gemini 2.0 Flash, etc. |
| Anthropic | `"anthropic"` | Claude models |
| Local | `"local"` | Ollama, LM Studio, any OpenAI-compatible server |

### How do I configure fallback LLM providers?

Add a `llm_fallbacks` array in `config.json`. If the primary provider fails, Sea-Claw automatically tries each fallback in order (up to 4):

```json
{
  "llm_provider": "openrouter",
  "llm_api_key": "sk-or-...",
  "llm_model": "moonshotai/kimi-k2.5",
  "llm_fallbacks": [
    { "provider": "openai", "model": "gpt-4o-mini" },
    { "provider": "gemini", "model": "gemini-2.0-flash" }
  ]
}
```

### How do I set up a Telegram bot?

1. Message [@BotFather](https://t.me/BotFather) on Telegram → `/newbot` → get your token
2. Message [@userinfobot](https://t.me/userinfobot) → get your numeric chat ID
3. Add to `config.json`:

```json
{
  "telegram_token": "123456:ABC...",
  "telegram_chat_id": 890034905
}
```

Or launch directly with flags:

```bash
sea_claw --telegram YOUR_TOKEN --chat YOUR_CHAT_ID
```

Set `telegram_chat_id` to `0` to allow messages from any chat. Set it to a specific non-zero ID to restrict to one conversation.

---

## Using Sea-Claw

### How do I start Sea-Claw?

```bash
# Interactive TUI mode
sea_claw

# With a specific config file
sea_claw --config ~/.config/seaclaw/config.json

# Telegram bot mode
sea_claw --telegram YOUR_BOT_TOKEN --chat YOUR_CHAT_ID

# Gateway mode (bus-based, multi-channel)
sea_claw --gateway --telegram YOUR_BOT_TOKEN
```

### What are the available CLI modes?

| Mode | Command | Description |
|------|---------|-------------|
| **TUI** | `sea_claw` | Interactive prompt in your terminal |
| **Telegram** | `sea_claw --telegram <token>` | Long-polling Telegram bot |
| **Gateway** | `sea_claw --gateway` | Bus-based multi-channel mode |
| **Doctor** | `sea_claw --doctor` | Diagnostics and self-check |
| **Onboard** | `sea_claw --onboard` | First-run setup wizard |

### What slash commands are available?

In both TUI and Telegram modes:

| Command | Description |
|---------|-------------|
| `/help` | Full command reference |
| `/status` | System status and memory |
| `/tools` | List all 57 tools |
| `/task list` | List tasks |
| `/task create <title>` | Create a task |
| `/task done <id>` | Complete a task |
| `/file read <path>` | Read a file |
| `/file write <path>\|<content>` | Write a file |
| `/shell <command>` | Run a shell command |
| `/web <url>` | Fetch URL content |
| `/session clear` | Clear chat history for this chat |
| `/model` | Show the current LLM model |
| `/audit` | View recent audit trail |
| `/delegate <url> <task>` | Delegate to a remote agent (A2A) |
| `/exec <tool> <args>` | Raw tool execution |

### What are the 57 tools?

Sea-Claw has 57 tools compiled-in at build time across these categories:

- **File I/O:** `file_read`, `file_write`, `file_append`, `file_delete`, `file_list`, `file_exists`
- **Shell:** `shell_exec`
- **Web:** `web_fetch`, `web_search` (Exa, Brave)
- **Tasks & DB:** `task_create`, `task_list`, `task_update`, `task_done`
- **Text processing:** `text_upper`, `text_lower`, `text_trim`, `text_replace`, `text_split`, `text_join`, `text_contains`, `text_length`, `count_lines`, `text_regex`
- **Data:** `json_format`, `csv_parse`, `base64_encode`, `base64_decode`
- **Hashing & encoding:** `hash_compute` (CRC32/DJB2/FNV-1a), `hex_encode`, `hex_decode`
- **Math:** `math_eval`, `math_round`, `math_clamp`, `math_stats`
- **Network:** `dns_lookup`, `ssl_check`, `ping`
- **Weather:** `weather_fetch`
- **Memory:** `memory_read`, `memory_write`, `memory_append`
- **Recall:** `recall_store`, `recall_query`
- **Cron:** `cron_create`, `cron_list`, `cron_delete`
- **Agent:** `system_status`, `echo`, `spawn`
- **A2A:** `agent_delegate`

The full count is 57 and is fixed at compile time. Use `/tools` to see the current list with descriptions.

> **Note:** 57 tools is a compile-time constant. Adding a new tool requires writing a `.c` implementation, adding a registry entry in `sea_tools.c`, updating the Makefile, and rebuilding. See [How do I add a new tool?](#how-do-i-add-a-new-tool) below.

### How does conversation memory work?

Sea-Claw uses two memory systems:

1. **Session memory** — Per-chat conversation history stored in SQLite (`seaclaw.db`). Up to 50 messages per chat. When the history threshold is reached, the agent auto-summarizes using the LLM, so context is preserved without growing unbounded. Sessions survive restarts.

2. **Long-term memory** — Markdown files in `~/.seaclaw/`:
   - `MEMORY.md` — Long-term facts and preferences
   - `IDENTITY.md`, `USER.md`, `SOUL.md`, `AGENTS.md` — Bootstrap personality files
   - `notes/YYYYMM/YYYYMMDD.md` — Daily notes

The memory workspace is created automatically on first run at `~/.seaclaw/`.

### How do I clear chat history?

```
/session clear
```

This removes all chat history for the current chat ID from `seaclaw.db`. Long-term memory files in `~/.seaclaw/` are not affected.

---

## Skills & Cron

### What are Skills?

Skills are Markdown-based plugins that extend the agent's capabilities at runtime. Each skill is a `.md` file with YAML frontmatter and a prompt body. Skills are loaded from `~/.seaclaw/skills/`.

Example skill file:

```markdown
---
name: summarize
description: Summarize any text or document
trigger: /summarize
---

You are a precise summarizer. When given text, produce a bullet-point summary
focusing on the key facts. Be concise.
```

### How do I create a custom skill?

1. Create a `.md` file in `~/.seaclaw/skills/`:

```bash
mkdir -p ~/.seaclaw/skills
cat > ~/.seaclaw/skills/my_skill.md << 'EOF'
---
name: translate
description: Translate text to a target language
trigger: /translate
---

Translate the user's text to the language they specify. Preserve tone and meaning.
EOF
```

2. Restart Sea-Claw (skills are loaded at startup). Use the trigger command:

```
/translate Hello World to Spanish
```

### How do I schedule background tasks?

Use the `cron_create` tool or `/exec cron_create`:

```
/exec cron_create "0 9 * * *" shell "echo Morning check >> /tmp/log.txt"
```

Or through natural language:

```
Schedule a shell command to run every 5 minutes: echo heartbeat >> /tmp/hb.log
```

Cron jobs are stored in `seaclaw.db` and survive restarts. Use `/exec cron_list` to see all jobs and `/exec cron_delete <id>` to remove one.

### What cron expression formats are supported?

Three formats are supported:

| Format | Example | Meaning |
|--------|---------|---------|
| Standard cron | `0 9 * * *` | Every day at 09:00 |
| Interval | `@every 5m` | Every 5 minutes (`s`/`m`/`h` suffixes) |
| One-shot | `@once 30s` | Fire once after 30 seconds |

Three job types:
- `shell` — Run a shell command
- `tool` — Call a registered Sea-Claw tool
- `bus_msg` — Publish a message on the internal bus

---

## Architecture & Development

### What is the Arena allocator and why does Sea-Claw use it?

The Arena allocator is Sea-Claw's memory model. Instead of calling `malloc`/`free` per allocation, all memory comes from a single `mmap`'d block. Allocations are sequential writes (bump-pointer); reset is a single pointer move.

Performance: **30ns per allocation**, zero fragmentation, deterministic timing.

Sea-Claw uses two arenas:

| Arena | Size | Purpose |
|-------|------|---------|
| `s_session_arena` | 16MB (configurable) | Config strings, long-lived data |
| `s_request_arena` | 1MB | Per-message allocations, reset after each response |

The `arena_size_mb` config key controls the session arena size (default: 16). Benefits:
- No memory leaks possible
- No use-after-free possible
- No GC pauses
- Arena reset is O(1)

### What is the Grammar Shield?

The Grammar Shield validates every input byte against a charset bitmap before it touches any engine logic. It rejects shell injection, SQL injection, XSS, and control characters at **< 1μs per check**.

Ten grammar types are defined:

| Grammar | Allows |
|---------|--------|
| `SAFE_TEXT` | Printable ASCII + valid UTF-8 |
| `NUMERIC` | `0-9 . - + e E` |
| `FILENAME` | `a-z A-Z 0-9 . - _ /` |
| `URL` | RFC 3986 charset |
| `COMMAND` | `/ a-z 0-9 space . _ -` |
| `JSON` | Valid JSON characters |
| `HEX` | `0-9 a-f A-F` |
| `BASE64` | `A-Z a-z 0-9 + / =` |
| `ALPHA` | Letters only |
| `ALPHANUM` | Letters + digits |

Injection detection catches: `$()`, backticks, `&&`, `||`, `;`, `|`, `../`, `<script>`, `javascript:`, SQL keywords (`DROP TABLE`, `UNION SELECT`, `OR 1=1`).

### Why only two external dependencies?

Sea-Claw uses exactly two runtime libraries: `libcurl` (HTTP) and `libsqlite3` (database). Every other capability — JSON parsing, memory management, security, scheduling — is implemented from scratch in C11.

This keeps the binary tiny (~203KB release), eliminates supply-chain risk from third-party packages, and makes deployment trivial: copy the binary and two shared libraries.

### How does the tool calling loop work?

```
User input → Shield validate → Build prompt (system + history + input)
    → POST to LLM API → Parse response
    → Tool call found? → Yes: execute tool, append result, loop
                       → No:  return final text response
```

The agent loop in `src/brain/sea_agent.c` continues calling the LLM until it produces a final text response with no tool calls. All tool outputs are arena-allocated and fed back into the next LLM call as part of the message history.

### How do I add a new tool?

Three steps:

1. **Create** `src/hands/impl/tool_example.c`:

```c
#include "seaclaw/sea_types.h"
#include "seaclaw/sea_arena.h"

SeaError tool_example(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    char buf[256];
    int len = snprintf(buf, sizeof(buf), "Result: %.*s",
                       (int)args.len, (const char*)args.data);
    u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)len);
    if (!dst) return SEA_ERR_ARENA_FULL;
    output->data = dst;
    output->len  = (u32)len;
    return SEA_OK;
}
```

2. **Register** in `src/hands/sea_tools.c` — add an `extern` declaration and a registry entry `{id, "example", "Description. Args: text", tool_example}`.

3. **Add** to `Makefile` HANDS_SRC list, then `make release`.

After rebuilding, the tool is available via `/exec example <args>` and the LLM agent can call it automatically.

### How do I run the test suite?

```bash
make test
```

Expected output (116 tests, 13 suites, all passing):

```
Sea-Claw Arena Tests:     9 passed
Sea-Claw JSON Tests:     17 passed
Sea-Claw Shield Tests:   19 passed
Sea-Claw DB Tests:       10 passed
Sea-Claw Config Tests:    6 passed
Sea-Claw Bus Tests:      10 passed
Sea-Claw Session Tests:  11 passed
Sea-Claw Memory Tests:    8 passed
Sea-Claw Cron Tests:     14 passed
Sea-Claw Skill Tests:    12 passed
Sea-Claw PII Tests:       7 passed
Sea-Claw Recall Tests:    7 passed
Sea-Claw SeaZero Tests:  18 passed
─────────────────────────────────────
Total: 116 passed, 0 failed (13 suites)
```

### What is the difference between debug and release builds?

| Build | Command | Flags | Binary size |
|-------|---------|-------|-------------|
| Debug | `make all` | `-O0 -g -fsanitize=address,undefined` | ~3MB |
| Release | `make release` | `-O3 -DNDEBUG -flto` (stripped) | ~203KB |

Use debug builds during development to catch memory issues early via AddressSanitizer. Use release builds for deployment.

### Where does Sea-Claw persist data?

| Location | Contents |
|----------|----------|
| `seaclaw.db` (working directory) | SQLite database: chat history, tasks, audit trail, cron jobs, usage stats |
| `~/.seaclaw/` | Memory workspace: `MEMORY.md`, bootstrap files, daily notes |
| `~/.seaclaw/skills/` | Skill plugin `.md` files |
| `config.json` (working directory) | Configuration (LLM keys, Telegram token, arena size) |

---

## SeaZero (C+Python Hybrid)

### What is SeaZero?

SeaZero combines Sea-Claw (the C11 orchestrator) with [Agent Zero](https://github.com/frdel/agent-zero) (a Python autonomous agent) into a hybrid platform. C handles orchestration, security, and memory. Python handles open-ended reasoning and code generation. Agent Zero runs in a Docker container — isolated, budget-controlled, and never sees your real API keys.

> **Requirement:** SeaZero requires Docker. The `install.sh` installer offers optional Docker + Agent Zero setup.

### How does Agent Zero integrate with Sea-Claw?

Sea-Claw starts an LLM proxy on `127.0.0.1:7432`. Agent Zero runs inside Docker and can only reach the LLM through this proxy — it never sees the real API key. Sea-Claw validates the bridge token, enforces a daily token budget, and filters all Agent Zero output through the Grammar Shield and a PII filter before returning results to the user.

The integration is implemented as tool #58 (`agent_zero`) in the static tool registry.

### What security model does SeaZero use?

8-layer security:

```
Layer 1 │ Docker Isolation     Seccomp, read-only rootfs, no-new-privileges
Layer 2 │ Network Isolation    Bridge network, DNS to 8.8.8.8/1.1.1.1 only
Layer 3 │ Credential Isolation Agent Zero only has the internal bridge token
Layer 4 │ Token Budget         Daily limit (default 100K tokens/day)
Layer 5 │ Grammar Shield       Byte-level validation of all Agent Zero output
Layer 6 │ PII Filter           Redacts emails, phones, SSNs, credit cards, IPs
Layer 7 │ Output Size Limit    64KB max response
Layer 8 │ Full Audit Trail     Every event logged to SQLite
```

### How do I use the /delegate command?

```
/delegate Write a Python script that analyzes the CSV files in /tmp
```

Sea-Claw sends the task to Agent Zero via the HTTP JSON bridge. Agent Zero reasons autonomously, executes code, and returns the result. The result passes through the Shield and PII filter before reaching you. Use `/sztasks` to see delegated task history and `/usage` to see token consumption.

---

## Sea-Claw Mesh (Enterprise)

### What is Sea-Claw Mesh?

Sea-Claw Mesh turns every machine on your local network into an AI-powered agent node, all coordinated by a single Captain node running a local LLM. No data ever leaves the network — no cloud APIs, no API keys, zero operating cost.

Full architecture documentation: [docs/MESH_ARCHITECTURE.md](MESH_ARCHITECTURE.md)

### What is a Captain node vs a Crew node?

| Role | Description |
|------|-------------|
| **Captain** | Central brain. Runs Ollama with a large model (e.g. 70B). Receives user requests via Telegram/CLI, routes tool calls to Crew nodes, aggregates results. |
| **Crew** | Worker nodes. Any Linux/macOS machine — laptop, desktop, Raspberry Pi, server. Auto-register with the Captain on startup, advertising their capabilities. |

Start a Captain:

```bash
./sea_claw --mode captain --config captain.json
```

Start a Crew node:

```bash
./sea_claw --mode crew --config crew.json
```

### How does capability-based routing work?

Each Crew node advertises its capabilities in its config (e.g. `["docker", "build", "shell_exec"]`). The Captain matches incoming tool calls to the right node:

```
User: "Build the Docker image"

Captain checks nodes:
  node1 (laptop):  [file_read, shell_exec]   → no docker
  node2 (desktop): [docker, build, test]     → matches ✓

→ Routes docker build to node2
→ Result returned to Captain → LLM formats response → user
```

All inter-node traffic is Shield-verified at every boundary. Sea-Claw Mesh deployment is consulting-based — contact [enterprise@oneconvergence.com](mailto:enterprise@oneconvergence.com).

---

## Security

### How does the Grammar Shield protect against prompt injection?

The Shield validates every byte of every input before it reaches any parsing or processing logic. It uses a charset bitmap lookup — O(1) per byte — so validation costs < 1μs per check regardless of input length.

Two checks are always applied to user input:
1. **Grammar check** — Rejects bytes that don't fit the expected character set for that input type
2. **Injection detector** — Pattern-scans for known attack patterns: `$()`, backticks, `&&`, `||`, `;`, `|`, `../`, `<script>`, `javascript:`, `eval()`, SQL keywords

The Shield is also applied to **outputs** from remote agents (A2A) before those results are used.

### What is the multi-layer security model?

Six layers protect the standard Sea-Claw deployment:

```
Layer 1 │ Shield Grammar Filter    Byte-level charset validation (<1μs)
Layer 2 │ Shield Injection Detect  Pattern matching for attack strings
Layer 3 │ Tool-Level Validation    Each tool validates its own arguments
Layer 4 │ Static Tool Registry     AI cannot invent tools; no dynamic loading
Layer 5 │ Arena Memory Safety      No malloc/free → no use-after-free
Layer 6 │ A2A Output Verification  Remote results Shield-validated before use
```

SeaZero adds 8 layers. Sea-Claw Mesh adds 6 layers including mesh authentication and IP allowlisting.

### How is A2A traffic secured?

Agent-to-Agent (A2A) traffic uses HTTP JSON-RPC. All responses from remote agents are passed through the Grammar Shield and injection detector before Sea-Claw uses them. The `/delegate` command and `agent_delegate` tool both enforce this. Peers are identified by URL; you configure trusted peers explicitly — there is no automatic discovery of untrusted nodes.

---

## Troubleshooting

### The build fails — what should I check first?

1. **Missing dependencies** — Run `sea_claw --doctor` or verify manually:
   ```bash
   pkg-config --libs libcurl libsqlite3
   ```
   Install any missing packages (see [How do I build manually?](#how-do-i-build-manually))

2. **Compiler version** — Requires gcc 11+ or clang with C11 support:
   ```bash
   gcc --version
   ```

3. **Clean build** — Stale object files can cause issues:
   ```bash
   make clean && make release
   ```

4. **Check the full error output** — Sea-Claw compiles with `-Wall -Wextra -Werror`. Any warning is a build error — read the exact message to identify the failing file.

### LLM API calls are failing — how do I debug?

1. Run `sea_claw --doctor` — it tests the LLM provider and shows the exact error.
2. Check your API key is set: environment variable takes precedence over `config.json`. Verify with:
   ```bash
   echo $OPENROUTER_API_KEY
   ```
3. Check your fallback chain in `config.json` — if the primary fails, fallbacks are tried in order.
4. Check the audit trail for recent errors:
   ```
   /audit
   ```
5. Try a direct curl to confirm the API key works outside of Sea-Claw:
   ```bash
   curl -s https://openrouter.ai/api/v1/models \
     -H "Authorization: Bearer $OPENROUTER_API_KEY" | head -c 200
   ```

### Telegram bot is not receiving messages — what's wrong?

1. Confirm the bot token is correct — regenerate it from [@BotFather](https://t.me/BotFather) if unsure.
2. Check `telegram_chat_id` in `config.json`. If set to a non-zero value, the bot only responds to that exact chat ID. Set to `0` to allow all chats.
3. Make sure the bot has been started — send `/start` to it in Telegram.
4. Verify long-polling is running: check the log output for `[TELEGRAM]` lines at startup.
5. Only one process can poll a bot token at a time. If another instance is running with the same token, messages will be silently consumed by it.

### How do I interpret sea_claw --doctor output?

Each check line shows a ✓ (pass) or ✗ (fail) with a brief description. Common failures:

| Output | Meaning | Fix |
|--------|---------|-----|
| `✗ config.json not found` | No config file in working directory | Run `sea_claw --onboard` or copy `config/config.example.json` |
| `✗ LLM API key missing` | No key in config or environment | Set the appropriate env var or update `config.json` |
| `✗ Telegram token invalid` | Token rejected by Telegram API | Regenerate token from @BotFather |
| `✗ seaclaw.db not writable` | Permissions issue on DB file | Check file and directory permissions |
| `✗ skills dir missing` | `~/.seaclaw/skills/` does not exist | `mkdir -p ~/.seaclaw/skills` |
| `✗ tool count mismatch` | Binary and registry are out of sync | `make clean && make release` |
