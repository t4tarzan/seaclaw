# Sea-Claw Sovereign System

> *"We stop building software that breaks. We start building logic that survives."*

A zero-dependency AI agent platform written in pure C11. 39KB binary. 45 tests. No garbage collector. No runtime. No excuses.

---

## What Is This?

Sea-Claw is a sovereign computing engine — a single binary that runs an AI agent with:

- **Arena Allocation** — No malloc, no free, no leaks. Memory is a notebook: write forward, rip out the page.
- **Zero-Copy JSON Parser** — Parses directly into the input buffer. 7 μs per parse. No copies.
- **Grammar Shield** — Every byte is validated against a charset bitmap before it touches the engine. Shell injection, SQL injection, XSS — all rejected at the byte level in < 1 μs.
- **Telegram Bot** — Long-polling Telegram integration. Commands dispatched through the same shield → tool pipeline.
- **Static Tool Registry** — Tools are compiled in. No dynamic loading. No eval. No surprises.

## Architecture

```
┌─────────────────────────────────────────────┐
│              Sea-Claw Binary (39KB)         │
├──────────┬──────────┬───────────┬───────────┤
│ Substrate│  Senses  │  Shield   │   Hands   │
│ (Arena)  │(JSON,HTTP)│(Grammar) │  (Tools)  │
├──────────┴──────────┴───────────┴───────────┤
│              Event Loop (main.c)            │
├─────────────────────┬───────────────────────┤
│    TUI Mode         │   Telegram Mode       │
│  Interactive CLI    │  Long-polling bot      │
└─────────────────────┴───────────────────────┘
```

### The Five Pillars

| Pillar | Directory | Purpose |
|--------|-----------|---------|
| **Substrate** | `src/core/` | Arena allocator, logging |
| **Senses** | `src/senses/` | JSON parser, HTTP client |
| **Shield** | `src/shield/` | Byte-level grammar validation |
| **Hands** | `src/hands/` | Static tool registry + implementations |
| **Telegram** | `src/telegram/` | Bot interface (Mirror pattern) |

## Quick Start

### Build

```bash
# Requirements: gcc, make, libcurl4-openssl-dev
sudo apt install build-essential libcurl4-openssl-dev

# Debug build (with AddressSanitizer)
make

# Release build (39KB stripped binary)
make release

# Run tests (45 tests)
make test
```

### Run — Interactive Mode

```bash
./sea_claw
```

Commands:
- `/help` — Show help
- `/status` — System status (arena usage, uptime, tools)
- `/tools` — List registered tools
- `/exec <tool> <args>` — Execute a tool
- `/quit` — Exit

Natural language input is validated through the Shield before processing.

### Run — Telegram Bot Mode

```bash
./sea_claw --telegram YOUR_BOT_TOKEN --chat YOUR_CHAT_ID
```

- `--telegram <token>` — Bot token from @BotFather
- `--chat <id>` — Restrict to a single chat (0 = allow all)

The bot responds to the same commands (`/help`, `/status`, `/tools`, `/exec`).

## Test Results

```
Sea-Claw Arena Tests:    9 passed  (1M allocs in 11ms, 1M resets in 12ms)
Sea-Claw JSON Tests:    17 passed  (10K parses in 70ms, 7.0 μs/parse)
Sea-Claw Shield Tests:  19 passed  (100K validations in 98ms, 0.98 μs/check)
─────────────────────────────────────────────
Total: 45 passed, 0 failed
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
│   ├── sea_http.h         # Minimal HTTP client
│   ├── sea_shield.h       # Grammar filter
│   ├── sea_telegram.h     # Telegram bot interface
│   └── sea_tools.h        # Static tool registry
├── src/
│   ├── core/              # Substrate: arena, logging
│   ├── senses/            # I/O: JSON parser, HTTP client
│   ├── shield/            # Grammar validation
│   ├── telegram/          # Telegram bot
│   ├── hands/             # Tool registry + implementations
│   └── main.c             # Event loop (355 lines)
├── tests/                 # 45 tests across 3 suites
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
| Binary size (release) | 39 KB |
| Total source lines | 3,044 |
| External dependencies | libcurl (HTTPS only) |
| C standard | C11 |
| Tests | 45 |
| JSON parse speed | 7.0 μs/parse |
| Shield check speed | 0.98 μs/check |
| Arena alloc speed | 11 ns/alloc |

## License

MIT

---

*The Cloud is Burning. The Vault Stands.*
