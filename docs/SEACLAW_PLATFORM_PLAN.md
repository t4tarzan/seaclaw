# SeaClaw Platform Plan — Multi-Tenant Agent-as-a-Service

> **Date:** 2026-02-25
> **Status:** Draft — awaiting review
> **Goal:** Turn SeaClaw + SeaZero into a multi-tenant platform where each user gets an isolated AI agent accessible via Telegram, WebChat, or SSH TUI — with the security architecture that makes SeaClaw unique.

---

## 1. The Core Architecture (What We Already Have)

### The SeaZero Security Model

SeaClaw is NOT a simple LLM wrapper. It's a **two-tier architecture** where security and work happen in different places:

```
┌──────────────────────────────────────────────────────────────┐
│  TIER 1: SeaClaw (C11 binary, 203 KB, 28 MB RAM)            │
│  "The Gatekeeper"                                            │
│                                                              │
│  ┌─────────────┐  ┌─────────────┐  ┌──────────────────────┐│
│  │ Grammar      │  │ PII         │  │ Local/Tiny LLM       ││
│  │ Shield       │  │ Firewall    │  │ (Ollama/qwen-tiny)   ││
│  │              │  │             │  │                       ││
│  │ • Input      │  │ • Email     │  │ • Decides routing     ││
│  │   injection  │  │ • Phone     │  │ • Simple tasks        ││
│  │   detection  │  │ • SSN       │  │ • Tool selection      ││
│  │ • Output     │  │ • Credit    │  │ • Redaction decisions  ││
│  │   injection  │  │   card      │  │ • Summarization       ││
│  │   detection  │  │ • IP addr   │  │                       ││
│  │ • Tool name  │  │             │  │ Runs on Raspberry Pi  ││
│  │   validation │  │ [REDACTED]  │  │ or 4 GB RAM server    ││
│  └─────────────┘  └─────────────┘  └──────────────────────┘│
│                                                              │
│  58 built-in tools │ SQLite DB │ Arena allocator │ Audit log │
│  Telegram bot │ TUI │ Bus │ Channels │ Sessions │ Recall DB  │
│                                                              │
│  Proxy server on port 7432:                                  │
│  • Validates internal token from Agent Zero                  │
│  • Checks daily token budget (100K tokens/day)               │
│  • Swaps fake token → real API key                           │
│  • Forwards to real LLM endpoint                             │
│  • Logs all usage to SQLite                                  │
└────────────────────────────┬─────────────────────────────────┘
                             │ HTTP (localhost only)
                             │ Token: internal bridge token
                             │ Port: 8080 (Agent Zero)
                             │ Port: 7432 (Proxy)
                             ▼
┌──────────────────────────────────────────────────────────────┐
│  TIER 2: Agent Zero (Python, Kali Linux Docker, 2 GB RAM)    │
│  "The Worker"                                                │
│                                                              │
│  ┌─────────────┐  ┌─────────────┐  ┌──────────────────────┐│
│  │ Python       │  │ Cloud LLM    │  │ Kali Linux tools     ││
│  │ Runtime      │  │ (via proxy)  │  │                       ││
│  │              │  │              │  │ • nmap, nikto         ││
│  │ • pip install│  │ • GPT-4o     │  │ • sqlmap, dirb        ││
│  │ • Any library│  │ • Claude 3   │  │ • hydra, john         ││
│  │ • Web scrape │  │ • Qwen 72B   │  │ • Network scanning    ││
│  │ • Code exec  │  │ (local)      │  │ • Vuln assessment     ││
│  │ • File I/O   │  │              │  │ • Pen testing         ││
│  └─────────────┘  └─────────────┘  └──────────────────────┘│
│                                                              │
│  LOCKED DOWN:                                                │
│  • read_only rootfs │ cap_drop: ALL │ no-new-privileges     │
│  • seccomp profile │ 1 CPU │ 2 GB RAM │ 100 PIDs max        │
│  • tmpfs only for /tmp and /run                              │
│  • Never sees real API key (gets fake internal token)        │
│  • All LLM calls routed through SeaClaw proxy (port 7432)   │
│  • Shared workspace volume for file exchange                 │
└──────────────────────────────────────────────────────────────┘
```

### What Happens When a User Sends a Message

```
User: "Scan my network for vulnerabilities"
  │
  ▼
TIER 1 — SeaClaw (C binary):
  ├── 1. Grammar Shield: scan input for injection attacks
  ├── 2. PII Firewall: redact any PII in the prompt BEFORE sending to LLM
  ├── 3. Load history from SQLite (last 20 messages)
  ├── 4. Send to local/tiny LLM (or cloud LLM via configured provider)
  │      LLM sees tool #58: "agent_zero — delegate complex tasks"
  │      LLM decides: this needs Agent Zero (network scanning = complex)
  │      LLM responds: {"tool": "agent_zero", "params": {"task": "scan network..."}}
  │
  ├── 5. SeaClaw executes tool #58 → sea_zero_delegate()
  │      POST http://localhost:8080/api/v1/task
  │      {"task": "scan network...", "max_steps": 10, "timeout": 120}
  │
  │      ┌──────────────────────────────────────────────────────┐
  │      │ TIER 2 — Agent Zero (Python/Kali Docker):            │
  │      │                                                      │
  │      │  Step 1: Plan (calls "OpenAI" = actually proxy)      │
  │      │    → Proxy validates token, checks budget, swaps key │
  │      │    → Forwards to real LLM (cloud or local)           │
  │      │    → LLM returns plan                                │
  │      │                                                      │
  │      │  Step 2: Execute (pip install python-nmap, scan)     │
  │      │  Step 3: Analyze results (another LLM call → proxy)  │
  │      │  Step 4: Generate report                             │
  │      │  Step 5: Save to /agent/shared/report.html           │
  │      │                                                      │
  │      │  Return: {"result": "Found 15 hosts, 47 ports..."}  │
  │      └──────────────────────────────────────────────────────┘
  │
  ├── 6. SeaClaw receives Agent Zero's result
  ├── 7. Output size limit check (64 KB max)
  ├── 8. Grammar Shield: scan OUTPUT for injection attacks
  ├── 9. PII Firewall: redact any PII in the OUTPUT
  ├── 10. Save both user message + response to SQLite
  └── 11. Send clean response to user via Telegram/TUI
```

### The Security Guarantee

Every piece of data passes through SeaClaw's C-level filters **twice**:
- **Inbound**: Shield → PII redact → LLM
- **Outbound from LLM**: Shield → PII redact → User
- **Outbound from Agent Zero**: Size limit → Shield → PII redact → User

This is what OpenClaw K8s Platform does NOT have. It sends raw prompts to raw LLM endpoints with zero filtering.

---

## 2. What OpenClaw K8s Does That We Need to Match

| OpenClaw K8s Feature | SeaClaw Status | Plan |
|---------------------|----------------|------|
| **Web signup form** (API keys, soul type) | ❌ | Phase C: Gateway UI |
| **Multi-user instances** | ❌ Single user | Phase B: Instance Manager |
| **Agent personalities (5 Souls)** | ✅ SOUL.md + USER.md | Already done, per-instance |
| **Real-time dashboard** (Kanban, chat) | ❌ | Phase C: Simple status dashboard |
| **PostgreSQL + pgvector** | ✅ SQLite + keyword recall | SQLite is lighter and sufficient |
| **Redis pub/sub** | ✅ sea_bus.c (compiled in) | Already better (zero dependencies) |
| **JWT auth** | ❌ | Phase C: Gateway auth |
| **Git integration** | ❌ | Future (Phase F) |
| **Prometheus + Grafana monitoring** | ❌ | sea_usage.c covers this lighter |
| **10 Docker containers** | 1 binary + 1 Docker container | SeaZero is already more efficient |

### What We Do BETTER Already

| Metric | OpenClaw K8s | SeaClaw + SeaZero |
|--------|-------------|-------------------|
| Per-instance RAM | 4+ GB (10 containers) | ~30 MB (SeaClaw) + 2 GB (Agent Zero shared) |
| Per-instance disk | ~2 GB Docker images | ~5 MB (binary + DB + config) |
| Startup time | 30-60 seconds | <50 milliseconds |
| Input security | None | Grammar Shield (byte-level, <1μs) |
| Output security | None | Grammar Shield + PII Firewall |
| LLM key protection | JWT (user holds key) | Proxy (user never touches cloud key) |
| Memory system | pgvector (needs OpenAI API) | SQLite recall (no external API) |
| Audit trail | Gateway text logs | SQLite event log (queryable) |
| Code execution sandbox | None built-in | Kali Docker (cap_drop ALL, seccomp) |

---

## 3. Multi-Tenant Architecture

### The Key Decision: How Many Agent Zero Containers?

There are two models:

#### Model A: One Agent Zero Per User (Expensive, Maximum Isolation)
```
User Alice → SeaClaw instance (28 MB) + Agent Zero container (2 GB) = ~2 GB total
User Bob   → SeaClaw instance (28 MB) + Agent Zero container (2 GB) = ~2 GB total
                                                        8 GB VPS = 3-4 users max
```

#### Model B: Shared Agent Zero, Per-User SeaClaw (Efficient, Recommended)
```
User Alice → SeaClaw instance (28 MB) ─┐
User Bob   → SeaClaw instance (28 MB) ──┤── Shared Agent Zero (2 GB)
User Carol → SeaClaw instance (28 MB) ──┤   (task queue, one at a time)
User Dave  → SeaClaw instance (28 MB) ─┘
                                  Total: ~2.1 GB for 4 users
                                  8 GB VPS = 50-100 users (most won't use Agent Zero)
```

#### Model C: Hybrid — Shared Agent Zero + On-Demand Spawn (Best)
```
Default:  All users share one Agent Zero instance (2 GB)
          Tasks are queued, executed one at a time
          Good for: occasional complex tasks

Premium:  User gets a dedicated Agent Zero container
          Spawned on demand, killed after idle timeout (15 min)
          Good for: heavy pentest/automation workloads

Crew:     User brings their own device (Raspberry Pi, laptop)
          SeaClaw runs locally, Agent Zero runs on hub VPS
          Mesh connection via Tailscale
```

**Recommendation: Model C** — start with shared Agent Zero, add dedicated containers later.

---

## 4. Per-User Instance Layout

```
/var/seaclaw/
├── master.db                       # Master database (all instances, users, billing)
├── sea_claw                        # Single binary (shared by all instances)
├── souls/                          # Soul templates
│   ├── eva.md                      # The Analyst (temperature 0.3)
│   ├── alex.md                     # The Developer (temperature 0.2)
│   ├── tom.md                      # The Creative (temperature 0.9)
│   ├── sarah.md                    # The Communicator
│   └── max.md                      # The Generalist
│
├── instances/
│   ├── alice/                      # Alice's isolated instance
│   │   ├── config.json             # Her LLM config (provider, model, API URL)
│   │   ├── .env                    # Her API keys (encrypted at rest)
│   │   ├── seaclaw.db              # Her SQLite DB (chat history, recall, tasks)
│   │   ├── SOUL.md                 # Her chosen personality
│   │   ├── USER.md                 # Her user profile
│   │   ├── MEMORY.md               # Her agent's memory
│   │   ├── skills/                 # Her installed skills
│   │   └── workspace/              # Shared volume with Agent Zero
│   │
│   ├── bob/
│   │   ├── config.json
│   │   ├── .env
│   │   ├── seaclaw.db
│   │   └── ...
│   │
│   └── carol/
│       └── ...
│
└── seazero/
    ├── docker-compose.yml          # Shared Agent Zero container
    ├── agent-zero.env              # Internal bridge token
    └── proxy.conf                  # Proxy config (port 7432)
```

### Per-Instance Process

Each user gets their own `sea_claw` process:

```bash
# Alice's instance
sea_claw \
  --config /var/seaclaw/instances/alice/config.json \
  --db /var/seaclaw/instances/alice/seaclaw.db \
  --telegram <alice_bot_token> \
  --chat <alice_chat_id> \
  --gateway-port 31042 \
  --proxy-port 7432 \
  --workspace /var/seaclaw/instances/alice/workspace
```

- PID: 4521
- RAM: 28 MB
- Port: 31042 (WebChat + API)
- Telegram: polling her bot
- DB: her own SQLite file
- Proxy: shared port 7432 (routes to Agent Zero)

### LLM Key Handling — Three Scenarios

```
Scenario 1: User brings their own API key (BYOK)
  ┌──────────┐     ┌──────────────┐     ┌──────────────┐
  │ SeaClaw   │────▶│ User's LLM   │     │ Agent Zero   │
  │ (Alice)   │     │ (OpenRouter)  │     │ (shared)     │
  │           │     │ sk-or-v1-... │     │              │
  │ Her key   │     └──────────────┘     │ Uses proxy   │
  │ in her    │                          │ → same key   │
  │ config    │     SeaClaw uses her     │   swapped in │
  └──────────┘     key directly          └──────────────┘

Scenario 2: User uses platform's shared LLM (metered)
  ┌──────────┐     ┌──────────────┐     ┌──────────────┐
  │ SeaClaw   │────▶│ Platform LLM │     │ Agent Zero   │
  │ (Bob)     │     │ (our key)    │     │ (shared)     │
  │           │     │              │     │              │
  │ No key    │     │ Proxy tracks │     │ Uses proxy   │
  │ needed    │     │ Bob's usage  │     │ → our key    │
  │ from Bob  │     │ for billing  │     │   budget-    │
  └──────────┘     └──────────────┘     │   limited    │
                                         └──────────────┘

Scenario 3: User runs local LLM (Ollama on their machine)
  ┌──────────┐     ┌──────────────┐     ┌──────────────┐
  │ SeaClaw   │────▶│ Ollama       │     │ Agent Zero   │
  │ (Carol)   │     │ (localhost)  │     │ (shared)     │
  │           │     │ qwen:72b     │     │              │
  │ Crew node │     │              │     │ Uses proxy   │
  │ on RPi    │     │ On her Mac   │     │ → routes to  │
  │           │     │ via Tailscale│     │   Ollama     │
  └──────────┘     └──────────────┘     └──────────────┘
  (Mesh VPN)       (Captain hub)        (On captain)
```

---

## 5. Access Methods

### 5a. Telegram Bot (Already Works)

Each user creates their own Telegram bot via @BotFather, enters the token in the signup form. Their SeaClaw instance polls that bot. Zero infrastructure needed.

### 5b. WebChat (New — Phase A2)

Each instance exposes a WebSocket endpoint on its random port:
```
https://agents.seaclawagent.com/alice/chat
     │
     ▼ Nginx reverse proxy
     │
     http://localhost:31042/chat  (Alice's SeaClaw instance)
```

Simple HTML/JS chat widget. No React, no Node.js — served as static HTML from SeaClaw's built-in HTTP server (`channel_webchat.c`).

### 5c. SSH TUI (New — Phase D2)

For power users who want the full terminal experience:
```
ssh -p 31043 alice@agents.seaclawagent.com
     │
     ▼ Nginx stream proxy or direct port
     │
     SeaClaw TUI on Alice's instance
```

Or via Tailscale:
```
ssh alice@vps-hostname.ts.net -p 31043
```

### 5d. Mesh / Tailscale (New — Phase D1)

For users who run SeaClaw on their own hardware (Raspberry Pi, laptop) and want to connect to the hub:

```
┌─────────────────────────┐         ┌─────────────────────────┐
│ User's Raspberry Pi      │         │ VPS Hub                  │
│                          │         │                          │
│ SeaClaw (crew mode)      │◄──────▶│ SeaClaw (captain mode)   │
│ 203 KB, 28 MB RAM        │  Mesh  │ Agent Zero (Docker)      │
│ Telegram bot             │  VPN   │ Ollama (local LLM)       │
│                          │ (9100/ │ Proxy (7432)             │
│ Tailscale: 100.x.y.z    │  9101) │ Tailscale: 100.a.b.c    │
└─────────────────────────┘         └─────────────────────────┘

User talks to Telegram bot on their Pi.
Pi's SeaClaw handles simple tasks locally (tools, file ops).
Complex tasks → mesh → hub → Agent Zero → cloud LLM → back.
```

---

## 6. The Gateway Signup Form

### What It Collects

```
┌─────────────────────────────────────────────────────────────┐
│  🦀 Create Your Sea-Claw Agent                              │
│                                                              │
│  ── Account ──                                               │
│  Username:        [_______________]                           │
│  Email:           [_______________]                           │
│                                                              │
│  ── LLM Provider ──                                          │
│  ○ Bring your own key (BYOK)                                 │
│    Provider: [OpenRouter ▼]                                   │
│    API Key:  [sk-or-v1-___________]  (encrypted, never logged)│
│    Model:    [qwen/qwen-2.5-72b ▼]                           │
│  ○ Use platform LLM (metered, $0.001/1K tokens)              │
│  ○ Connect my local LLM (Ollama via Tailscale)               │
│    Ollama URL: [http://100.x.y.z:11434]                      │
│                                                              │
│  ── Agent Personality ──                                      │
│  ○ Eva — The Analyst (precise, data-driven)                  │
│  ○ Alex — The Developer (technical, code-focused)            │
│  ● Tom — The Creative (expressive, brainstorming)            │
│  ○ Sarah — The Communicator (clear, teaching-oriented)       │
│  ○ Max — The Generalist (balanced, multi-tasking)            │
│  ○ Custom (paste your SOUL.md)                               │
│                                                              │
│  ── Access Methods ── (select one or more)                   │
│  ☑ Telegram Bot                                              │
│    Bot Token: [123456:ABC-DEF___________]                    │
│    Chat ID:   [________] (leave empty for any chat)          │
│  ☑ WebChat (browser)                                         │
│  ☐ SSH Terminal (advanced)                                   │
│    SSH Public Key: [ssh-rsa AAAA___________]                 │
│                                                              │
│  ── Security ──                                              │
│  ☑ Enable PII Firewall (redact emails, phones, SSNs, etc.)  │
│  ☑ Enable Grammar Shield (injection detection)               │
│  ☑ Enable Agent Zero (complex task delegation)               │
│  Token budget: [100000] tokens/day                           │
│                                                              │
│  ── Network Access ──                                        │
│  ○ Tailscale tunnel (zero open ports, recommended)           │
│  ○ Direct port (firewall-protected)                          │
│  ○ Mesh VPN (connect your own devices)                       │
│    Tailscale auth key: [tskey-___________]                   │
│                                                              │
│           [ 🦀 Create My Agent ]                             │
│                                                              │
│  Your agent will be ready in <2 seconds.                     │
│  203 KB binary. 28 MB RAM. 58 tools. Yours.                 │
└─────────────────────────────────────────────────────────────┘
```

### What Happens on Submit

```
POST /api/v1/agents/create
{
  "username": "alice",
  "email": "alice@example.com",
  "llm": {
    "mode": "byok",
    "provider": "openrouter",
    "api_key": "sk-or-v1-...",      // encrypted before storage
    "model": "qwen/qwen-2.5-72b"
  },
  "soul": "tom",
  "channels": {
    "telegram": {
      "enabled": true,
      "bot_token": "123456:ABC-DEF...",
      "chat_id": null
    },
    "webchat": { "enabled": true },
    "ssh": { "enabled": false }
  },
  "security": {
    "pii_firewall": true,
    "grammar_shield": true,
    "agent_zero": true,
    "token_budget": 100000
  },
  "network": {
    "mode": "tailscale"
  }
}
```

**Backend flow** (Instance Manager):

```
1. Validate all inputs
2. Encrypt API key with platform master key
3. mkdir /var/seaclaw/instances/alice/
4. Write config.json (provider, model, API URL — key stored encrypted)
5. Write .env (decrypted key, only readable by alice's process)
6. Copy souls/tom.md → instances/alice/SOUL.md
7. Create empty seaclaw.db (schema auto-created on first run)
8. Pick random port: 31042 (WebChat) + 31043 (SSH, if enabled)
9. Fork sea_claw process with alice's config
10. If Tailscale: create serve rule → alice gets https://vps.ts.net/alice/
11. Register in master.db: (alice, 31042, PID, tom, running, now())
12. Return access URLs to user
```

**Time: <2 seconds** (fork + exec is instant, SeaClaw starts in <50ms)

---

## 7. Build Phases

### Phase A: Feature Parity (Close OpenClaw Gaps)

**Must be done before going multi-tenant.**

| # | Task | Effort | Files | Status |
|---|------|--------|-------|--------|
| A1 | **SSE Streaming** — token-by-token response in TUI and Telegram | Medium | `sea_http.c`, `sea_agent.c` | ✅ Done (stream_cb exists) |
| A2 | **WebChat channel** — HTTP/WebSocket endpoint serving chat UI | Medium | `channel_webchat.c` (new) | ❌ |
| A3 | **Session compaction** — `/compact` summarizes history, keeps last 10 | Low | `sea_agent.c`, `sea_db.c` | Partial (auto-summarize exists) |
| A4 | **Skills directory** — load SKILL.md files from workspace | Low | `sea_skill.c` (existing) | ✅ Done |
| A5 | **WhatsApp channel** — via WhatsApp Business Cloud API | Medium | `channel_whatsapp.c` (new) | ❌ Future |

**Note on A1**: SSE streaming is already implemented — `sea_agent.c` has `stream_cb`, `sea_http_post_stream()`, and SSE parsing. The code at line 685-697 injects `"stream":true` and uses `sse_data_cb`. This was a gap that's already closed.

### Phase B: Instance Manager

| # | Task | Effort | New Files |
|---|------|--------|-----------|
| B1 | `sea_spawn.h/c` — fork/exec SeaClaw instances | Medium | `src/platform/sea_spawn.h/c` |
| B2 | Master DB schema — `instances`, `users`, `billing` tables | Low | `src/platform/sea_platform_db.c` |
| B3 | Port allocator — random ports 30000-32767, collision avoidance | Low | Part of `sea_spawn.c` |
| B4 | Health checker — periodic PID check, port ping, auto-restart | Low | Part of `sea_spawn.c` |
| B5 | Instance lifecycle — create, stop, restart, destroy, list | Medium | Part of `sea_spawn.c` |
| B6 | Config writer — generate per-instance config.json + .env + SOUL.md | Low | Part of `sea_spawn.c` |
| B7 | API key encryption — encrypt at rest, decrypt only for process .env | Medium | `sea_crypto.c` (new or existing) |
| B8 | SeaZero sharing — route multiple instances through shared proxy | Medium | `sea_proxy.c` modifications |

**B8 is critical**: Currently the proxy validates ONE internal token. For multi-tenant, the proxy needs to:
- Accept per-user tokens (each instance gets its own bridge token)
- Track token budget per user, not globally
- Route to the correct LLM endpoint (BYOK users have different providers)

```c
// Modified proxy flow for multi-tenant:
static void handle_chat_completions(int fd, ProxyRequest* req) {
    // 1. Extract token from Authorization header
    const char* token = req->auth_token;

    // 2. Look up which user this token belongs to
    UserConfig* user = lookup_user_by_token(token);
    if (!user) {
        send_json_error(fd, 401, "Invalid token");
        return;
    }

    // 3. Check THIS USER's budget
    if (!check_user_budget(user)) {
        send_json_error(fd, 429, "Daily token budget exceeded");
        return;
    }

    // 4. Use THIS USER's real API key
    snprintf(auth_hdr, sizeof(auth_hdr), "Authorization: Bearer %s",
             user->real_api_key);

    // 5. Forward to THIS USER's LLM endpoint
    sea_http_post_json_auth(user->real_api_url, body, auth_hdr, &arena, &resp);

    // 6. Log usage for THIS USER
    sea_db_sz_llm_log(db, user->username, user->provider, user->model,
                       tokens_in, tokens_out, cost, latency_ms, "ok", NULL);
}
```

### Phase C: Gateway UI

| # | Task | Effort | New Files |
|---|------|--------|-----------|
| C1 | Static HTML signup form (no React, no Node.js) | Low | `gateway/index.html` |
| C2 | `POST /api/v1/agents/create` endpoint in SeaClaw | Medium | `src/platform/sea_gateway.c` |
| C3 | `GET /api/v1/agents/list` — user's instances | Low | Part of gateway |
| C4 | `POST /api/v1/agents/{id}/stop` — stop instance | Low | Part of gateway |
| C5 | `GET /api/v1/agents/{id}/status` — instance health | Low | Part of gateway |
| C6 | `GET /api/v1/agents/{id}/usage` — token usage | Low | Part of gateway |
| C7 | Simple dashboard page (status, usage, controls) | Low | `gateway/dashboard.html` |
| C8 | JWT or session auth for gateway | Medium | Part of gateway |
| C9 | Nginx config for routing (gateway + per-instance WebChat) | Low | `nginx/seaclaw-platform.conf` |

**Architecture decision**: The gateway runs as a special "platform mode" of SeaClaw itself:
```bash
sea_claw --platform \
  --gateway-port 443 \
  --master-db /var/seaclaw/master.db \
  --instances-dir /var/seaclaw/instances/
```

This means NO new runtime dependencies. The gateway is just another mode of the same 203 KB binary.

### Phase D: Secure Access

| # | Task | Effort | Details |
|---|------|--------|---------|
| D1 | **Tailscale integration** — auto-create serve rules per instance | Low | Shell exec: `tailscale serve ...` |
| D2 | **SSH access** — per-instance SSH port via Nginx stream proxy | Medium | `nginx stream {}` block |
| D3 | **Mesh VPN** — allow crew nodes to connect via Tailscale | Low | Already works (mesh architecture) |
| D4 | **Cloudflare Tunnel** alternative (for users without Tailscale) | Low | `cloudflared tunnel ...` |

### Phase E: Agent Zero Multi-Tenant

| # | Task | Effort | Details |
|---|------|--------|---------|
| E1 | **Task queue** — multiple users share one Agent Zero, tasks queued | Medium | Queue in master.db, FIFO |
| E2 | **Per-user workspace isolation** — each user's tasks write to their workspace | Low | Mount `/var/seaclaw/instances/<user>/workspace` per task |
| E3 | **On-demand container spawn** — premium users get dedicated Agent Zero | High | Docker API from C (`sea_docker.c`) |
| E4 | **Idle timeout** — kill dedicated containers after 15 min idle | Low | Timer + `docker stop` |

### Phase F: Future

| # | Task | Notes |
|---|------|-------|
| F1 | Billing / metering (Stripe) | For platform LLM usage |
| F2 | Git integration | Like OpenClaw K8s |
| F3 | Custom Docker images | User uploads their own Agent Zero variant |
| F4 | Multi-region | Deploy SeaClaw instances across Hetzner DCs |
| F5 | Federation | Multiple VPS hubs connected via mesh |

---

## 8. Comparison: Final Architecture vs. OpenClaw K8s

| Aspect | OpenClaw K8s Platform | SeaClaw Platform |
|--------|----------------------|-----------------|
| **Per-user footprint** | 4+ GB (10 containers) | 28 MB (1 process) |
| **Users on 8 GB VPS** | 1 | 50-200 |
| **Startup time** | 30-60 seconds | <2 seconds |
| **Dependencies** | Docker, PostgreSQL, Redis, Node.js, Python | 1 binary + 1 Docker container |
| **Input/output security** | ❌ None | ✅ Grammar Shield + PII Firewall |
| **LLM key protection** | JWT (user holds key in browser) | Proxy (key never leaves server) |
| **Code execution** | ❌ No sandbox | ✅ Kali Docker (seccomp, cap_drop) |
| **Memory system** | pgvector (needs OpenAI) | SQLite recall (zero external deps) |
| **Multi-channel** | Web UI only | Telegram + WebChat + SSH + Mesh |
| **Monitoring** | Prometheus + Grafana (300 MB) | Built-in sea_usage.c (0 MB) |
| **Secure tunnels** | ❌ None | ✅ Tailscale mesh VPN |
| **Edge deployment** | ❌ Needs 4+ GB | ✅ Runs on Raspberry Pi |

### The Pitch

> **"200 AI agents on one $10/month VPS."**
>
> Each agent: 203 KB binary, 28 MB RAM, 58 built-in tools, Grammar Shield,
> PII Firewall, Agent Zero delegation, Telegram + WebChat + SSH access.
>
> OpenClaw K8s needs 10 Docker containers and 4 GB RAM for ONE user.
> SeaClaw does it with one process and 28 MB.

---

## 9. Build Order Summary

```
Phase A: Close feature gaps           ← 1-2 weeks
  └── A2: WebChat channel (main missing piece)

Phase B: Instance manager             ← 2-3 weeks
  ├── B1-B6: fork/exec, master DB, port alloc, health check
  ├── B7: API key encryption
  └── B8: Multi-tenant proxy (per-user tokens + budgets)

Phase C: Gateway UI                   ← 1-2 weeks
  ├── C1-C2: Signup form + create API
  ├── C3-C7: Dashboard + management APIs
  └── C8-C9: Auth + Nginx config

Phase D: Secure access                ← 1 week
  ├── D1: Tailscale auto-setup
  ├── D2: SSH access
  └── D3-D4: Mesh + Cloudflare alternatives

Phase E: Agent Zero multi-tenant      ← 2-3 weeks
  ├── E1-E2: Task queue + workspace isolation
  └── E3-E4: On-demand containers + idle timeout

Phase F: Future                       ← Ongoing
  └── Billing, Git, custom images, multi-region
```

**Total estimated: 7-11 weeks to MVP (Phases A-D)**
**Full platform with dedicated Agent Zero: 9-14 weeks (A-E)**

---

## 10. What NOT to Build

Lessons from the OpenClaw comparison doc:

- **❌ Native macOS/iOS/Android apps** — wrong stack. Telegram + WebChat covers 95%.
- **❌ React dashboard with Kanban** — overkill. Simple HTML status page is enough.
- **❌ PostgreSQL + Redis** — SQLite per-instance is lighter, faster, zero-dependency.
- **❌ Kubernetes (yet)** — Docker Compose for Agent Zero is sufficient. K8s is Phase F.
- **❌ Voice/camera/screen** — niche features, not our lane.
- **❌ pgvector embeddings** — our keyword-scored SQLite recall works without an API call.
