# OpenClaw vs Sea-Claw — Feature Comparison & Gap Analysis

> **Date:** 2025-02-12
> **Purpose:** Identify what OpenClaw does that Sea-Claw doesn't, assess what's worth adopting, and plan improvements.

---

## 1. Architecture Overview

| Aspect | OpenClaw | Sea-Claw |
|---|---|---|
| **Language** | TypeScript (Node.js ≥22) | C11 (single binary) |
| **Binary size** | ~200 MB (node_modules) | ~2 MB (static link) |
| **RAM at idle** | ~150-300 MB (V8 + gateway) | ~17 MB (arena) |
| **Stars** | 68,000+ | Private |
| **Contributors** | 514+ | Solo |
| **License** | MIT | Private |
| **Architecture** | WebSocket gateway + RPC agent | Monolithic event loop |
| **Package manager** | npm/pnpm/bun | make |
| **Config format** | JSON (`~/.openclaw/openclaw.json`) | JSON (`config.json`) |
| **Workspace** | `~/.openclaw/workspace/` | `~/.seaclaw/` |
| **Personality files** | `AGENTS.md`, `SOUL.md`, `TOOLS.md` | `IDENTITY.md`, `SOUL.md`, `USER.md`, `AGENTS.md`, `MEMORY.md` |

### Verdict
Sea-Claw's C architecture is a **massive advantage** for embedded/edge deployment (8 GB Hetzner, Raspberry Pi, routers). OpenClaw requires Node.js 22 and ~300 MB RAM just to idle. Sea-Claw's 17 MB footprint is 15-20× lighter.

---

## 2. Channel Support (Messaging Platforms)

| Channel | OpenClaw | Sea-Claw |
|---|---|---|
| **Telegram** | ✅ (grammY) | ✅ (native C, libcurl) |
| **WhatsApp** | ✅ (Baileys) | ❌ |
| **Slack** | ✅ (Bolt) | ❌ |
| **Discord** | ✅ (discord.js) | ❌ |
| **Signal** | ✅ (signal-cli) | ❌ |
| **iMessage** | ✅ (BlueBubbles) | ❌ |
| **Microsoft Teams** | ✅ (extension) | ❌ |
| **Google Chat** | ✅ (Chat API) | ❌ |
| **Matrix** | ✅ (extension) | ❌ |
| **WebChat** | ✅ (built-in UI) | ❌ |
| **CLI/Gateway** | ✅ | ✅ |
| **Multi-channel routing** | ✅ (per-agent sessions) | ❌ (single channel) |
| **Group chat support** | ✅ (mention gating, activation modes) | ❌ |
| **DM pairing/security** | ✅ (pairing codes, allowlists) | ❌ |

### Gap: HIGH — Multi-Channel
OpenClaw's biggest differentiator is its **multi-channel inbox**. Sea-Claw currently only supports Telegram + CLI.

### Recommendation
- **Phase 1 (quick win):** Add **WhatsApp** via WhatsApp Business Cloud API (REST-based, fits our libcurl HTTP client). This covers 90% of personal assistant use cases.
- **Phase 2:** Add **Discord** (WebSocket-based, needs a small WS client in C).
- **Phase 3:** Add **Slack** (REST + Events API).
- **Skip for now:** iMessage (Apple-only), Teams (enterprise), Signal (complex protocol), Matrix (niche).

---

## 3. Tool / Skill Comparison

### 3a. Core Tools — Sea-Claw Has, OpenClaw Has

| Category | OpenClaw Tools | Sea-Claw Tools | Notes |
|---|---|---|---|
| **Shell execution** | `bash` (full shell) | `shell_exec` (command runner) | Equivalent |
| **File read/write** | `read`, `write`, `edit` | `file_read`, `file_write`, `edit_file` | Equivalent |
| **Web fetch** | `http` (via fetch) | `web_fetch`, `http_request` | Sea-Claw has more granular HTTP |
| **Web search** | Via skills (Exa, etc.) | `web_search` (Brave), `exa_search` | Equivalent |
| **Process management** | `process` | `process_list` | Equivalent |
| **Cron/scheduling** | `cron` (built-in) | `cron_manage`, `cron_parse` | Equivalent |
| **Memory/personality** | `SOUL.md`, `AGENTS.md`, `TOOLS.md` | `memory_manage`, `recall` (SQLite-backed) | **Sea-Claw is stronger** — has scored recall DB |
| **Session management** | `sessions_list/history/send` | ❌ | Gap |
| **Task management** | Via skills | `task_manage` (SQLite-backed) | Equivalent |

### 3b. Tools OpenClaw Has That Sea-Claw Doesn't

| OpenClaw Tool | What It Does | Priority for Sea-Claw |
|---|---|---|
| **Browser control** | CDP-based Chrome automation (click, type, screenshot, scrape) | 🔴 HIGH |
| **Canvas / A2UI** | Agent-driven visual workspace (render HTML/charts live) | 🟡 MEDIUM |
| **Voice Wake + Talk Mode** | Always-on speech recognition + TTS (ElevenLabs) | 🟡 MEDIUM |
| **Camera snap/clip** | Take photos/video from device camera | ⚪ LOW |
| **Screen recording** | Record screen on macOS/iOS/Android | ⚪ LOW |
| **Location** | GPS location from device | ⚪ LOW |
| **Notifications** | Push notifications to device | 🟡 MEDIUM |
| **Agent-to-Agent** | Multi-agent sessions, message routing between agents | 🟡 MEDIUM |
| **Webhooks** | Inbound webhook triggers | 🔴 HIGH |
| **Gmail Pub/Sub** | Email event triggers | 🟡 MEDIUM |
| **Docker sandboxing** | Run untrusted code in containers | 🟡 MEDIUM |

### 3c. Tools Sea-Claw Has That OpenClaw Doesn't (Built-In)

| Sea-Claw Tool | What It Does | OpenClaw Equivalent |
|---|---|---|
| `hash_compute` | CRC32, DJB2, FNV1a hashing | Needs a skill |
| `math_eval` | Expression evaluation | Needs a skill |
| `dns_lookup` | DNS resolution | Via bash |
| `ssl_check` | SSL certificate inspection | Via bash |
| `whois_lookup` | Domain WHOIS | Via bash |
| `csv_parse` | CSV parsing | Via bash/Python |
| `json_query` | JSONPath queries | Via bash/jq |
| `diff_text` | Text diffing | Via bash/diff |
| `regex_match` | Regex matching | Via bash/grep |
| `unit_convert` | Unit conversion | Needs a skill |
| `uuid_gen` | UUID generation | Via bash/uuidgen |
| `password_gen` | Secure password generation | Needs a skill |
| `recall` | SQLite-backed scored fact memory | No equivalent — OpenClaw uses flat markdown only |
| `spawn` | Sub-agent spawning | `sessions_send` (more advanced) |
| `shield` (injection detection) | Input/output injection scanning | Basic prompt-injection resistance via model choice |

### Verdict
Sea-Claw has **more built-in tools** (57 vs OpenClaw's ~12 core tools). OpenClaw compensates with its **Skills platform** (100+ community skills from ClawHub) and **bash** as a universal escape hatch. Sea-Claw's `recall` system (scored SQLite memory) is **genuinely superior** to OpenClaw's flat markdown memory.

---

## 4. Skills / Plugin System

| Aspect | OpenClaw | Sea-Claw |
|---|---|---|
| **Plugin format** | `SKILL.md` (markdown prompt + optional code) | ❌ No plugin system |
| **Registry** | ClawHub (100+ community skills) | ❌ |
| **Install command** | `openclaw skill install <name>` | ❌ |
| **Self-creating skills** | ✅ Agent can write new skills autonomously | ❌ |
| **Skill categories** | Smart home, productivity, social media, dev tools, health | ❌ |

### Gap: HIGH — Skills Platform
This is OpenClaw's second biggest differentiator. The ability for the agent to **create its own skills** and for users to **share skills** via a registry is extremely powerful.

### Recommendation
- **Phase 1:** Add a `skills/` directory in `~/.seaclaw/` where each skill is a `SKILL.md` file (same format as OpenClaw — just a markdown prompt injected into the system prompt when relevant).
- **Phase 2:** Add a `tool_skill_install` that can download skills from a Git repo or URL.
- **Phase 3:** Build a Sea-Claw skill registry (like ClawHub but lighter).

---

## 5. Security Model

| Aspect | OpenClaw | Sea-Claw |
|---|---|---|
| **Input validation** | Model-level prompt injection resistance | `sea_shield.c` — pattern-based injection detection |
| **Output validation** | None explicit | `sea_shield.c` — scans LLM output too |
| **Tool name validation** | Allowlist/denylist per session | `sea_shield.c` — grammar validation |
| **Sandboxing** | Docker containers for non-main sessions | ❌ No sandboxing |
| **DM security** | Pairing codes, allowlists, per-channel policies | ❌ |
| **Elevated access** | `/elevated on|off` toggle | ❌ (always full access) |
| **Audit logging** | Gateway logs | `sea_db.c` — SQLite event log |

### Verdict
Sea-Claw's `sea_shield.c` is **more rigorous** on input/output validation (pattern scanning both directions). OpenClaw relies on model-level resistance. However, OpenClaw's **Docker sandboxing** and **DM pairing** are important for multi-user scenarios.

### Recommendation
- **Phase 1:** Add a `--sandbox` flag that wraps `shell_exec` and `file_write` in a restricted directory (chroot or namespace).
- **Phase 2:** Add DM pairing for Telegram (pairing codes before accepting messages from unknown users).

---

## 6. LLM Provider Support

| Provider | OpenClaw | Sea-Claw |
|---|---|---|
| **Anthropic Claude** | ✅ (primary, OAuth) | ✅ (via API key) |
| **OpenAI** | ✅ (OAuth + API key) | ✅ |
| **Google Gemini** | ✅ | ✅ |
| **OpenRouter** | ✅ | ✅ (primary) |
| **Local (Ollama)** | ✅ | ✅ (just verified) |
| **Model failover** | ✅ (automatic chain) | ✅ (fallback array) |
| **OAuth subscriptions** | ✅ (Claude Pro/Max, ChatGPT) | ❌ (API keys only) |
| **Streaming** | ✅ (SSE streaming) | ❌ (blocking request) |
| **Session pruning** | ✅ (automatic context compaction) | ❌ |
| **Usage tracking** | ✅ (per-response cost) | ✅ (`sea_usage.c`) |

### Gap: HIGH — Streaming
OpenClaw streams responses token-by-token. Sea-Claw waits for the entire response. This makes Sea-Claw feel slow on long responses.

### Recommendation
- **Phase 1:** Add SSE (Server-Sent Events) parsing to `sea_http.c` for streaming responses. Use `CURLOPT_WRITEFUNCTION` callback to process `data:` chunks as they arrive.
- **Phase 2:** Add session pruning (automatic context compaction when token count exceeds threshold).

---

## 7. Session / Context Management

| Aspect | OpenClaw | Sea-Claw |
|---|---|---|
| **Session model** | Per-channel isolated sessions with metadata | Single global chat history |
| **Context compaction** | `/compact` command — summarizes and prunes | ❌ |
| **Multi-agent sessions** | ✅ (route channels to different agents) | ❌ |
| **Thinking levels** | `/think off\|minimal\|low\|medium\|high\|xhigh` | ❌ |
| **Verbose levels** | `/verbose on\|off` | ❌ |
| **Chat commands** | `/status`, `/new`, `/reset`, `/compact`, `/think`, `/usage` | `/help`, `/status`, `/tools`, `/exec`, `/tasks`, `/clear`, `/quit` |

### Gap: MEDIUM — Session Management
Sea-Claw's single-session model works for single-user, but `/compact` and `/think` are valuable features.

### Recommendation
- **Phase 1:** Add `/new` (reset session) and `/compact` (summarize + prune history) commands.
- **Phase 2:** Add `/think` levels (maps to `temperature` and `max_tokens` adjustments).

---

## 8. Apps & Companion Devices

| Aspect | OpenClaw | Sea-Claw |
|---|---|---|
| **macOS app** | ✅ (menu bar, Voice Wake, Talk Mode) | ❌ |
| **iOS app** | ✅ (Canvas, camera, Talk Mode) | ❌ |
| **Android app** | ✅ (Canvas, camera, Talk Mode) | ❌ |
| **WebChat UI** | ✅ (served from Gateway) | ❌ |
| **Control UI** | ✅ (web dashboard) | ❌ |

### Verdict
This is where OpenClaw's TypeScript/Electron stack pays off — easy to build cross-platform UIs. Sea-Claw's C binary is **not suited** for building native apps. However, a **WebChat UI** is achievable.

### Recommendation
- **Phase 1:** Add a minimal WebChat endpoint to the gateway (serve static HTML + WebSocket for chat). This is the `seaclaw-docs` Next.js app you already have — could add a chat widget.
- **Skip:** Native macOS/iOS/Android apps. Not our lane. Users interact via Telegram or CLI.

---

## 9. Priority Ranking — What to Build Next

### 🔴 HIGH PRIORITY (biggest impact, most feasible)

| # | Feature | OpenClaw Has | Effort | Impact |
|---|---|---|---|---|
| 1 | **SSE Streaming** | ✅ | Medium (modify `sea_http.c`) | Huge UX improvement |
| 2 | **Browser control** | ✅ (CDP) | High (new tool, needs headless Chrome) | Unlocks web automation |
| 3 | **WhatsApp channel** | ✅ (Baileys) | Medium (WhatsApp Cloud API + libcurl) | 2B+ users |
| 4 | **Skills/plugin system** | ✅ (ClawHub) | Low (just `SKILL.md` files in workspace) | Extensibility |
| 5 | **Webhook triggers** | ✅ | Medium (add HTTP listener to gateway) | Automation |
| 6 | **Session compaction** | ✅ (`/compact`) | Low (summarize history, prune old messages) | Token savings |

### 🟡 MEDIUM PRIORITY (nice to have)

| # | Feature | Effort | Impact |
|---|---|---|---|
| 7 | Discord channel | Medium | Developer community |
| 8 | Docker sandboxing | Medium | Security for multi-user |
| 9 | Agent-to-Agent routing | High | Multi-agent workflows |
| 10 | WebChat UI | Medium | Browser-based access |
| 11 | DM pairing security | Low | Telegram security |
| 12 | `/think` levels | Low | User control |

### ⚪ LOW PRIORITY (skip or defer)

| # | Feature | Why Skip |
|---|---|---|
| 13 | Native macOS/iOS/Android apps | Wrong stack (C binary, not Electron) |
| 14 | Voice Wake / Talk Mode | Niche, requires ElevenLabs API |
| 15 | Camera/screen recording | Device-specific, not our use case |
| 16 | Canvas / A2UI | Visual workspace, complex, low ROI for CLI agent |
| 17 | OAuth subscriptions | API keys work fine |

---

## 10. Sea-Claw's Unfair Advantages (Keep These)

Things Sea-Claw does **better** than OpenClaw that we should protect and amplify:

| Advantage | Sea-Claw | OpenClaw |
|---|---|---|
| **Binary size** | ~2 MB | ~200 MB (node_modules) |
| **RAM footprint** | 17 MB idle | 150-300 MB idle |
| **Startup time** | <50 ms | 2-5 seconds |
| **No runtime dependency** | Just libc + libcurl + SQLite | Node.js 22 + npm ecosystem |
| **Scored memory recall** | SQLite-backed, keyword-scored, recency-decayed | Flat markdown files only |
| **Input/output shield** | Pattern-based injection detection on both sides | Model-level only |
| **57 built-in tools** | All compiled in, zero dependencies | ~12 core tools + bash escape hatch |
| **Arena allocator** | Zero-leak guarantee, predictable memory | V8 garbage collector (unpredictable pauses) |
| **Audit trail** | SQLite event log with timestamps | Gateway logs (text files) |
| **Edge deployable** | Runs on 8 GB Hetzner, Raspberry Pi, routers | Needs 2+ GB RAM minimum |

---

## 11. Implementation Roadmap

```
Phase 1 (Next 2 weeks):
  ├── SSE streaming in sea_http.c
  ├── /compact and /new chat commands
  ├── Skills directory (~/.seaclaw/skills/*.md)
  └── /think level command

Phase 2 (Month 2):
  ├── WhatsApp Cloud API channel
  ├── Webhook inbound listener
  ├── Session compaction (auto-prune at token threshold)
  └── Browser tool (headless Chrome via CDP)

Phase 3 (Month 3):
  ├── Discord channel
  ├── DM pairing security
  ├── Docker sandbox mode
  └── WebChat UI widget

Phase 4 (Future):
  ├── Agent-to-Agent routing
  ├── Skill registry (SeaHub?)
  └── Notification system
```

---

## 12. Summary

**OpenClaw is a feature-rich, community-driven TypeScript agent with 68K stars and 514 contributors.** Its strengths are multi-channel support (15+ platforms), a skills marketplace, browser automation, and companion apps.

**Sea-Claw is a lean, fast, C-native agent optimized for edge deployment.** Its strengths are minimal resource usage (17 MB), scored memory recall, input/output security shielding, 57 built-in tools, and zero runtime dependencies.

**The biggest gaps to close:**
1. **SSE streaming** — makes responses feel instant
2. **Browser control** — unlocks web automation use cases
3. **WhatsApp** — reaches 2B+ users
4. **Skills system** — enables community extensibility
5. **Session compaction** — reduces token costs over long conversations

**What NOT to copy from OpenClaw:**
- Electron/Node.js stack (defeats our C advantage)
- Native apps (wrong lane)
- Voice/camera/screen features (niche)
- Flat markdown memory (our SQLite recall is better)
