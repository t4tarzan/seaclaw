# SeaZero Implementation Plan — Sea-Claw v3.0

**Version:** 3.0.0-alpha
**Codename:** SeaZero
**Status:** Phase 0 complete, Phase 1 in progress
**Last Updated:** 2026-02-13

---

## 1. Philosophy Adherence Checklist

Every change MUST pass these checks before merge:

| # | Principle | Check |
|---|-----------|-------|
| 1 | **Small footprint** | Binary stays under 300KB stripped. No new shared libraries. |
| 2 | **Zero malloc** | All new allocations use SeaArena. No malloc/free in hot paths. |
| 3 | **Pure C11** | No C++, no inline assembly, no compiler extensions beyond `_GNU_SOURCE`. |
| 4 | **Two dependencies** | Only libcurl + libsqlite3. Agent Zero is Docker, not a C dependency. |
| 5 | **Grammar Shield** | ALL external input (Agent Zero output, LLM responses) passes through Shield. |
| 6 | **Static tools** | No dynamic tool loading. `tool_agent_zero` is compiled in like all 57 others. |
| 7 | **Opt-in integration** | SeaClaw without Docker = same 203KB binary, same 57 tools. SeaZero is additive. |
| 8 | **One API key** | User provides ONE LLM key. SeaClaw proxies for Agent Zero. |
| 9 | **Trust boundary** | Agent Zero NEVER sees real API keys, credentials, or sensitive tool access. |
| 10 | **Audit everything** | Every delegation, every LLM call, every tool result — logged in SQLite. |
| 11 | **Five Pillars** | Substrate → Senses → Shield → Brain → Hands. Each layer only knows below. |
| 12 | **Sovereign** | Works fully offline with local LLM. No cloud requirement. |

---

## 2. Database Schema v3

### 2.1 Existing Tables (v2 — unchanged)

```sql
-- Key-value config store
config(key TEXT PK, value TEXT, updated_at DATETIME)

-- Audit/event log
trajectory(id INTEGER PK, entry_type TEXT, title TEXT, content TEXT, created_at DATETIME)

-- User tasks
tasks(id INTEGER PK, title TEXT, status TEXT, priority TEXT, content TEXT, created_at, updated_at)

-- Chat history per conversation
chat_history(id INTEGER PK, chat_id INTEGER, role TEXT, content TEXT, created_at DATETIME)

-- Recall engine (separate DB)
recall_facts(id INTEGER PK, category TEXT, content TEXT, keywords TEXT, importance INTEGER,
             created_at TEXT, accessed_at TEXT, access_count INTEGER)
```

### 2.2 New Tables (v3 — SeaZero integration)

```sql
-- Schema version tracking
CREATE TABLE IF NOT EXISTS schema_version (
    version     TEXT NOT NULL,
    applied_at  DATETIME DEFAULT (datetime('now'))
);

-- Agent Zero instances
CREATE TABLE IF NOT EXISTS seazero_agents (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    agent_id    TEXT NOT NULL UNIQUE,           -- e.g. "agent-0", "agent-a1b2c3d4"
    status      TEXT NOT NULL DEFAULT 'stopped', -- stopped, starting, ready, busy, error
    container   TEXT,                            -- Docker container name
    port        INTEGER,                         -- localhost port
    provider    TEXT,                            -- LLM provider this agent uses
    model       TEXT,                            -- LLM model
    created_at  DATETIME DEFAULT (datetime('now')),
    last_seen   DATETIME DEFAULT (datetime('now'))
);

-- Delegated tasks (SeaClaw → Agent Zero)
CREATE TABLE IF NOT EXISTS seazero_tasks (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    task_id     TEXT NOT NULL UNIQUE,            -- e.g. "task-abc123"
    agent_id    TEXT NOT NULL,                   -- which agent is handling this
    chat_id     INTEGER,                         -- originating chat (Telegram/TUI)
    status      TEXT NOT NULL DEFAULT 'pending', -- pending, running, completed, failed, cancelled
    task_text   TEXT NOT NULL,                   -- natural language task description
    context     TEXT,                            -- conversation context passed to agent
    result      TEXT,                            -- agent's response (sanitized)
    error       TEXT,                            -- error message if failed
    steps_taken INTEGER DEFAULT 0,              -- how many steps agent took
    files       TEXT,                            -- JSON array of output file paths
    created_at  DATETIME DEFAULT (datetime('now')),
    started_at  DATETIME,
    completed_at DATETIME,
    elapsed_sec REAL DEFAULT 0
);

-- LLM usage tracking (both SeaClaw and Agent Zero calls)
CREATE TABLE IF NOT EXISTS seazero_llm_usage (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    caller      TEXT NOT NULL,                   -- 'seaclaw' or agent_id
    provider    TEXT NOT NULL,                   -- openrouter, openai, gemini, etc.
    model       TEXT NOT NULL,                   -- model name
    tokens_in   INTEGER DEFAULT 0,              -- prompt tokens
    tokens_out  INTEGER DEFAULT 0,              -- completion tokens
    cost_usd    REAL DEFAULT 0,                 -- estimated cost
    latency_ms  INTEGER DEFAULT 0,              -- response time
    status      TEXT DEFAULT 'ok',              -- ok, error, timeout, blocked
    task_id     TEXT,                            -- linked seazero_task if from agent
    created_at  DATETIME DEFAULT (datetime('now'))
);

-- Security audit trail (Shield blocks, injection attempts, etc.)
CREATE TABLE IF NOT EXISTS seazero_audit (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    event_type  TEXT NOT NULL,                   -- 'shield_block', 'delegation', 'tool_call',
                                                -- 'agent_spawn', 'agent_stop', 'llm_proxy',
                                                -- 'credential_access', 'file_access'
    source      TEXT NOT NULL,                   -- 'seaclaw', agent_id, 'user'
    target      TEXT,                            -- tool name, file path, URL, etc.
    detail      TEXT,                            -- JSON details
    severity    TEXT DEFAULT 'info',             -- info, warn, block, critical
    created_at  DATETIME DEFAULT (datetime('now'))
);

-- Indexes for v3 tables
CREATE INDEX IF NOT EXISTS idx_sz_tasks_status ON seazero_tasks(status);
CREATE INDEX IF NOT EXISTS idx_sz_tasks_agent ON seazero_tasks(agent_id);
CREATE INDEX IF NOT EXISTS idx_sz_tasks_chat ON seazero_tasks(chat_id);
CREATE INDEX IF NOT EXISTS idx_sz_llm_caller ON seazero_llm_usage(caller);
CREATE INDEX IF NOT EXISTS idx_sz_llm_task ON seazero_llm_usage(task_id);
CREATE INDEX IF NOT EXISTS idx_sz_audit_type ON seazero_audit(event_type);
CREATE INDEX IF NOT EXISTS idx_sz_audit_source ON seazero_audit(source);
CREATE INDEX IF NOT EXISTS idx_sz_agents_status ON seazero_agents(status);
```

### 2.3 Migration Strategy

- `sea_db_open()` already uses `CREATE TABLE IF NOT EXISTS`
- v3 tables are appended to `SCHEMA_SQL` — they won't affect existing v2 databases
- `schema_version` table tracks which version is applied
- No data migration needed — new tables are additive

---

## 3. Architecture Decisions (Locked)

| Decision | Choice | Rationale |
|----------|--------|-----------|
| **Repo structure** | `seazero/` subdirectory in `t4tarzan/seaclaw` | No new org, SeaClaw stays the project |
| **UX model** | Model C: shared workspace + sanitized summaries | Works in Telegram + TUI, enforces Shield |
| **LLM routing** | One key, SeaClaw proxies for Agent Zero | Security, budget control, provider flexibility |
| **IPC** | HTTP JSON on localhost:8080 | Uses existing `sea_http`, no new deps |
| **Agent Zero UI** | Not exposed | SeaClaw is the only user interface |
| **Trust boundary** | Agent Zero = untrusted, SeaClaw = trusted | Agent Zero can't touch email/GitHub/creds |
| **Database** | Same SQLite file, new tables for v3 | No Postgres, no Redis, stays lean |
| **Shared workspace** | `~/.seazero/workspace/<task-id>/` | Agent Zero writes, SeaClaw reads + sanitizes |

---

## 4. Task Breakdown — All Phases

### Phase 0: Foundation ✅ COMPLETE
- [x] Create `feature/seazero` branch
- [x] `seazero/README.md` — architecture, philosophy, usage
- [x] `seazero/bridge/sea_zero.h` — C bridge API
- [x] `seazero/bridge/sea_zero.c` — implementation using `sea_http`
- [x] `seazero/docker-compose.yml` — lean Agent Zero container
- [x] `seazero/config/agent-zero.env` — environment template
- [x] `seazero/scripts/setup.sh` — first-time setup wizard
- [x] `seazero/scripts/spawn-agent.sh` — agent lifecycle management
- [x] `Makefile` — added `seazero-*` targets
- [x] Merge `dev` → `main` (Z.AI, Docker work)
- [x] Push `feature/seazero` to GitHub

### Phase 1: Database v3 + Tool Registration
- [ ] **1.1** Add v3 schema to `SCHEMA_SQL` in `sea_db.c`
- [ ] **1.2** Add `sea_db_seazero_*` functions to `sea_db.h` / `sea_db.c`:
  - `sea_db_sz_agent_register()` — register/update agent
  - `sea_db_sz_task_create()` — create delegated task
  - `sea_db_sz_task_update()` — update task status/result
  - `sea_db_sz_task_get()` — get task by ID
  - `sea_db_sz_llm_log()` — log LLM usage
  - `sea_db_sz_audit()` — log security event
- [ ] **1.3** Add `tool_agent_zero` to `s_registry[]` in `sea_tools.c` as tool #58
- [ ] **1.4** Add `extern SeaError tool_agent_zero(...)` forward declaration
- [ ] **1.5** Wire `sea_zero_init()` call in `main.c` (if `seazero_enabled` in config)
- [ ] **1.6** Compile and run existing tests (ensure no regressions)
- [ ] **1.7** Write `tests/test_seazero.c` — unit tests for bridge + DB functions

### Phase 2: LLM Proxy
- [ ] **2.1** Add lightweight HTTP listener to SeaClaw (port 7432)
  - Minimal: only `/llm/proxy` endpoint for Agent Zero
  - Uses `libcurl` multi interface or simple `accept()` loop
  - Validates internal token on every request
- [ ] **2.2** Proxy logic: receive OpenAI-format request → validate → forward to real LLM → return
- [ ] **2.3** Token budget: `seazero_llm_usage` table tracks per-agent usage
  - Config: `seazero_token_budget_daily: 100000` (default)
  - Reject if over budget, log to `seazero_audit`
- [ ] **2.4** Injection check: Grammar Shield validates Agent Zero's prompts before forwarding
- [ ] **2.5** Usage dashboard: `/usage` TUI command shows SeaClaw vs Agent Zero token usage
- [ ] **2.6** Update `docker-compose.yml`: Agent Zero gets `OPENAI_API_BASE=http://host:7432/llm/proxy`

### Phase 3: Unified Installer
- [ ] **3.1** Update `install.sh` with optional "Enable Agent Zero?" step
- [ ] **3.2** Generate internal bridge token during setup (`openssl rand -hex 32`)
- [ ] **3.3** Write Agent Zero config automatically from SeaClaw's config
- [ ] **3.4** Pull/build Agent Zero Docker image if user opts in
- [ ] **3.5** Add `seazero_enabled`, `seazero_agent_url`, `seazero_internal_token` to `config.json`
- [ ] **3.6** Update `--doctor` command to check Agent Zero health
- [ ] **3.7** Update `--onboard` wizard with SeaZero options

### Phase 4: Security Hardening
- [ ] **4.1** Seccomp profile for Agent Zero container (`seazero/config/seccomp.json`)
- [ ] **4.2** PII filter on all Agent Zero output (existing `sea_pii`)
- [ ] **4.3** File access audit: log every file Agent Zero writes to shared workspace
- [ ] **4.4** Network isolation: Agent Zero can only reach SeaClaw proxy + internet (no LAN)
- [ ] **4.5** Credential isolation: Agent Zero env has NO access to:
  - Telegram token
  - Email credentials
  - GitHub tokens
  - Any SeaClaw config secrets
- [ ] **4.6** Output size limits: Agent Zero response capped at 64KB
- [ ] **4.7** Task timeout: hard kill after `seazero_timeout_sec` (default 120s)
- [ ] **4.8** Penetration test: try to escape container, exfiltrate keys, inject via output

### Phase 5: Shared Workspace + File Delivery
- [ ] **5.1** Shared volume: `~/.seazero/workspace/<task-id>/` mounted in Agent Zero
- [ ] **5.2** SeaClaw reads workspace after task completion
- [ ] **5.3** File delivery in Telegram: attach small files, link large ones
- [ ] **5.4** File delivery in TUI: show path, offer to open
- [ ] **5.5** Workspace cleanup: auto-delete after 7 days (configurable)

### Phase 6: TUI Enhancements
- [ ] **6.1** `/agents` command — list Agent Zero instances + status
- [ ] **6.2** `/delegate <task>` command — explicitly delegate to Agent Zero
- [ ] **6.3** `/tasks` command — show delegated task history
- [ ] **6.4** `/usage` command — LLM token usage breakdown
- [ ] **6.5** `/audit` command — recent security events
- [ ] **6.6** Status indicator: show when Agent Zero is working

### Phase 7: Electron UI (Optional, Future)
- [ ] **7.1** Evaluate if Telegram + TUI cover 90%+ of use cases
- [ ] **7.2** If needed: Node.js gateway on SeaClaw's HTTP listener
- [ ] **7.3** React frontend from Kimi scaffold (adapt, don't rewrite)
- [ ] **7.4** WebSocket for real-time Agent Zero activity feed

### Phase 8: RunPod Serverless LLM
- [ ] **8.1** Add `SEA_LLM_RUNPOD` provider to `sea_agent.c`
- [ ] **8.2** RunPod serverless endpoint support (cold start handling)
- [ ] **8.3** Both SeaClaw and Agent Zero use same RunPod instance
- [ ] **8.4** Cost tracking in `seazero_llm_usage`

---

## 5. Role Separation Matrix

| Capability | SeaClaw (Trusted) | Agent Zero (Sandboxed) |
|-----------|-------------------|----------------------|
| Send email | ✅ | ❌ asks SeaClaw |
| GitHub push | ✅ | ❌ asks SeaClaw |
| Read credentials | ✅ | ❌ never |
| Access filesystem | ✅ (full) | ⚠️ workspace only |
| Shell execution | ✅ (Grammar Shield) | ✅ (inside container) |
| Web browsing | ✅ (tool_web_fetch) | ✅ (inside container) |
| LLM calls | ✅ (direct) | ✅ (via proxy only) |
| Write code | ✅ (tool_file_write) | ✅ (inside container) |
| Network scan | ❌ (not a tool) | ✅ (Kali tools) |
| Data analysis | ⚠️ (basic tools) | ✅ (Python/pandas) |
| Long research | ❌ (single-turn) | ✅ (multi-step) |
| Personality/chat | ❌ (strict tool) | ✅ (natural language) |

---

## 6. LLM Routing Architecture

```
User provides ONE API key during onboarding
                    │
                    ▼
    ┌──────────────────────────────┐
    │  SeaClaw config.json         │
    │  llm_api_key: "sk-or-..."   │
    │  llm_provider: "openrouter"  │
    │  seazero_internal_token: "..." │
    └──────────┬───────────────────┘
               │
    ┌──────────▼───────────────────┐
    │  SeaClaw Brain (direct)      │
    │  Uses real API key           │
    │  57 tools, strict prompts    │
    └──────────────────────────────┘
               │
    ┌──────────▼───────────────────┐
    │  SeaClaw LLM Proxy (:7432)   │
    │  Validates internal token    │
    │  Checks token budget         │
    │  Grammar Shield on prompts   │
    │  Forwards to real LLM        │
    │  Logs usage to SQLite        │
    └──────────┬───────────────────┘
               │
    ┌──────────▼───────────────────┐
    │  Agent Zero (Docker)         │
    │  OPENAI_API_BASE=:7432/llm   │
    │  OPENAI_API_KEY=internal_tok │
    │  Never sees real API key     │
    └──────────────────────────────┘
```

---

## 7. File Manifest

### Existing files modified:
- `src/core/sea_db.c` — add v3 schema + seazero DB functions
- `include/seaclaw/sea_db.h` — add seazero struct/function declarations
- `src/hands/sea_tools.c` — add tool #58 `agent_zero`
- `src/main.c` — wire `sea_zero_init()` if enabled
- `Makefile` — add `seazero/bridge/sea_zero.o` to build (conditional)
- `install.sh` — add optional Agent Zero step

### New files (Phase 0 — done):
- `seazero/README.md`
- `seazero/bridge/sea_zero.h`
- `seazero/bridge/sea_zero.c`
- `seazero/docker-compose.yml`
- `seazero/config/agent-zero.env`
- `seazero/scripts/setup.sh`
- `seazero/scripts/spawn-agent.sh`
- `seazero/SEAZERO_PLAN.md` (this file)

### New files (upcoming):
- `seazero/config/seccomp.json` — Phase 4
- `tests/test_seazero.c` — Phase 1
- `seazero/Dockerfile.agent-zero` — Phase 3

---

## 8. Git Strategy

```
main ─────────────────────────────────────────────►
  │
  ├── feature/seazero (Phase 0) ──── PR ──► main
  │     │
  │     ├── Phase 1 commits (DB v3 + tool registration)
  │     ├── Phase 2 commits (LLM proxy)
  │     ├── Phase 3 commits (installer)
  │     └── Phase 4 commits (security)
  │
  └── Tag: v3.0.0-alpha (after Phase 4 complete)
```

---

## 9. Success Metrics

| Metric | Target | How to Verify |
|--------|--------|---------------|
| Binary size | < 300KB | `ls -lh dist/sea_claw` |
| Existing tests | 100% pass | `make test` |
| New tests | > 20 assertions | `./test_seazero` |
| Agent Zero spawn | < 10s | `time make seazero-up` |
| Task delegation | < 2s overhead | Bridge round-trip time |
| LLM proxy latency | < 50ms added | Proxy vs direct comparison |
| Shield validation | < 1ms | Existing benchmark |
| No credential leak | 0 leaks | Penetration test (Phase 4.8) |
| Opt-in clean | 0 regressions | Build without SeaZero, run all tests |

---

## 10. Context for Future Sessions

### What exists today:
- SeaClaw v2: 9,159 lines C11, 57 tools, 6 LLM providers (incl. Z.AI), Telegram + TUI
- SeaZero Phase 0: bridge code, Docker config, scripts, Makefile targets
- Branch: `feature/seazero` on `t4tarzan/seaclaw`

### What's decided:
- One API key, SeaClaw proxies for Agent Zero
- Agent Zero is untrusted, SeaClaw is trusted
- Shared workspace for file delivery
- Model C UX (sanitized summaries, not raw Agent Zero output)
- SQLite v3 schema (4 new tables, additive migration)

### What's next:
- Phase 1: Add v3 schema to `sea_db.c`, wire `tool_agent_zero` as tool #58
- Phase 2: LLM proxy endpoint on port 7432
- Phase 3: Update installer
- Phase 4: Security hardening

### Key files to read first:
- `seazero/SEAZERO_PLAN.md` (this file)
- `seazero/bridge/sea_zero.h` (bridge API)
- `src/core/sea_db.c` (database, add v3 schema here)
- `src/hands/sea_tools.c` (tool registry, add #58 here)
- `src/main.c` (startup, wire SeaZero init here)
