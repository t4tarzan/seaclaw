# Sea-Claw vs PicoClaw vs OpenClaw

> *Three agents. Three philosophies. One comparison.*

## At a Glance

| Metric | **Sea-Claw** (C11) | **PicoClaw** (Go) | **OpenClaw** (TypeScript) |
|--------|:---:|:---:|:---:|
| **Language** | C11 | Go 1.21 | TypeScript/Node.js |
| **Source lines** | 13,400+ | ~8,500 | ~15,000+ |
| **Source files** | 95 | 38 | 200+ |
| **Binary size** | ~3 MB | ~15 MB | ~200 MB (node_modules) |
| **Startup time** | < 1 ms | ~50 ms | ~3 sec |
| **Peak memory** | 16 MB | ~40 MB | ~200 MB |
| **Dependencies** | 2 (libcurl, SQLite) | ~20 Go modules | ~400 npm packages |
| **Tools** | 56 compiled-in | ~12 | ~20 |
| **Tests** | 116 (10 suites) | ~30 | ~50 |
| **Architecture** | x86 + ARM | x86 + ARM | x86 only |
| **Garbage collector** | None (arena) | Go GC | V8 GC |
| **Runtime** | None | Go runtime | Node.js + V8 |
| **Container required** | No | No | Docker recommended |
| **LLM providers** | 5 (+ fallback chain) | 2-3 | 3-4 |
| **Telegram bot** | Built-in | Plugin | Plugin |
| **Cron scheduler** | Built-in (SQLite) | None | External |
| **Session management** | Built-in (per-chat) | Basic | Redis-based |
| **Long-term memory** | Markdown files | None | Vector DB |
| **Skills/plugins** | Markdown-based | None | JS plugins |
| **Message bus** | Thread-safe pub/sub | None | Event emitter |
| **A2A protocol** | HTTP JSON-RPC | None | Partial |
| **Usage tracking** | Per-provider/day | None | Cloud dashboard |
| **Security model** | Grammar shield (byte-level) | Input sanitization | YARA rules |
| **Supply chain risk** | 2 deps, auditable | ~20 deps | ~400 deps |
| **Runs on 2005 hardware** | Yes | No | No |
| **License** | MIT | MIT | MIT |

## Deep Dive

### Binary & Deployment

| | Sea-Claw | PicoClaw | OpenClaw |
|---|---|---|---|
| Install | `make && sudo make install` | `go build` | `npm install && docker-compose up` |
| First run | `./sea_claw --onboard` | Edit YAML | Edit .env + Docker |
| Diagnostics | `./sea_claw --doctor` | None | None |
| Min server | $3/mo VPS, 512MB RAM | $5/mo VPS, 1GB RAM | $10/mo VPS, 2GB RAM |
| Offline capable | Yes (with local LLM) | Partial | No |

### Security

| Attack Vector | Sea-Claw | PicoClaw | OpenClaw |
|---|---|---|---|
| SQL injection | Grammar shield rejects at byte level | Parameterized queries | ORM layer |
| Shell injection | Bitmap validation before exec | Go exec sanitization | Child process |
| XSS | No web UI (TUI/Telegram) | N/A | Sanitization library |
| Path traversal | Shield + whitelist | Filepath check | Middleware |
| Supply chain | 2 C libraries (curl, sqlite3) | 20 Go modules | 400+ npm packages |
| Dynamic code exec | Impossible (static registry) | Not used | `eval()` possible |
| Memory safety | Arena allocation (no use-after-free) | Go GC | V8 GC |

### Performance Benchmarks

Measured on Hetzner CX22 (2 vCPU, 4GB RAM, AMD EPYC):

| Operation | Sea-Claw | PicoClaw | OpenClaw |
|---|---|---|---|
| Startup | < 1 ms | ~50 ms | ~3,000 ms |
| JSON parse (180B) | 3 μs | ~10 μs | ~15 μs |
| Input validation | 0.6 μs | ~5 μs | ~20 μs |
| Memory alloc (64B) | 30 ns | ~100 ns (GC) | ~200 ns (V8) |
| Memory reset | 7 ns | N/A (GC) | N/A (GC) |
| Tool execution | < 1 ms | ~5 ms | ~10 ms |
| Idle memory | 16 MB | ~40 MB | ~200 MB |

### Feature Matrix

| Feature | Sea-Claw | PicoClaw | OpenClaw |
|---|---|---|---|
| Interactive TUI | ✅ | ✅ | ❌ |
| Telegram bot | ✅ | ✅ (plugin) | ✅ (plugin) |
| Discord bot | 🔜 (deferred) | ❌ | ✅ |
| WhatsApp | 🔜 (deferred) | ❌ | ❌ |
| Slack | 🔜 (deferred) | ❌ | ✅ |
| Web UI | ❌ (Mirror pattern) | ❌ | ✅ |
| Multi-provider LLM | ✅ (5 providers) | ✅ (2-3) | ✅ (3-4) |
| Fallback chain | ✅ (up to 4) | ❌ | ❌ |
| Tool calling | ✅ (56 tools) | ✅ (~12) | ✅ (~20) |
| Conversation memory | ✅ (SQLite) | ✅ (in-memory) | ✅ (Redis) |
| Long-term memory | ✅ (markdown) | ❌ | ✅ (vector DB) |
| Session isolation | ✅ (per-chat) | ❌ | ✅ (per-user) |
| Cron jobs | ✅ (persistent) | ❌ | ❌ |
| Skills/plugins | ✅ (markdown) | ❌ | ✅ (JS) |
| A2A delegation | ✅ | ❌ | Partial |
| Usage tracking | ✅ | ❌ | Cloud |
| Sub-agent spawn | ✅ | ❌ | ❌ |
| File editing | ✅ | ❌ | ✅ |
| Web search | ✅ (Brave + Exa) | ✅ (1 provider) | ✅ |

### Why C?

| Concern | C Answer |
|---|---|
| "C is unsafe" | Arena allocation eliminates use-after-free. No malloc/free pairs. Grammar shield validates every byte. |
| "C is slow to develop" | 13,400 lines covers what takes 15,000+ in TypeScript. No framework churn. No dependency updates. |
| "C has no ecosystem" | 2 dependencies. Both are battle-tested C libraries older than most npm packages. |
| "C can't do web" | libcurl handles HTTPS. We don't need Express, Axios, or node-fetch. |
| "C is not modern" | C11 has `_Atomic`, `_Static_assert`, `_Generic`. It compiles on every platform that exists. |
| "Nobody writes C anymore" | Linux, PostgreSQL, SQLite, curl, Redis, Nginx, Git — all C. The infrastructure layer never left. |

---

*"When the world chose chaos, India chose C."*
