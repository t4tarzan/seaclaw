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
- [x] `seazero/config/agent-zero.env.example` — environment template
- [x] `seazero/scripts/setup.sh` — first-time setup wizard
- [x] `seazero/scripts/spawn-agent.sh` — agent lifecycle management
- [x] `Makefile` — added `seazero-*` targets
- [x] Merge `dev` → `main` (Z.AI, Docker work)
- [x] Push `feature/seazero` to GitHub

### Phase 1: Database v3 + Tool Registration ✅ COMPLETE
- [x] **1.1** Add v3 schema to `SCHEMA_SQL` in `sea_db.c`
- [x] **1.2** Add `sea_db_sz_*` functions to `sea_db.h` / `sea_db.c`:
  - `sea_db_sz_agent_register()` — register/update agent
  - `sea_db_sz_task_create()` — create delegated task
  - `sea_db_sz_task_start/complete/fail()` — update task status/result
  - `sea_db_sz_task_list()` — list tasks by status
  - `sea_db_sz_llm_log()` — log LLM usage
  - `sea_db_sz_audit()` — log security event
- [x] **1.3** Add `tool_agent_zero` to `s_registry[]` in `sea_tools.c` as tool #58
- [x] **1.4** Add `extern SeaError tool_agent_zero(...)` forward declaration
- [x] **1.5** Bridge API fixes (sea_json_parse, sea_shield, sea_arena signatures)
- [x] **1.6** Compile (0 warnings) + all 12 existing test suites pass (0 regressions)
- [x] **1.7** `tests/test_seazero.c` — 18 assertions, all pass

### Phase 2: LLM Proxy ✅ COMPLETE
- [x] **2.1** POSIX socket HTTP listener on 127.0.0.1:7432 (background pthread)
- [x] **2.2** POST /v1/chat/completions — OpenAI-compatible proxy endpoint
- [x] **2.3** Token budget: daily limit via `seazero_llm_usage`, default 100K tokens/day
- [x] **2.4** Internal token validation on every request (Bearer auth)
- [x] **2.5** Usage logging: tokens_in, tokens_out, latency, cost per call
- [x] **2.6** docker-compose updated: `OPENAI_API_BASE=http://host.docker.internal:7432`
- [x] **2.7** Wired into main.c: reads config from DB, starts proxy if enabled
- [x] **2.8** Clean shutdown via `sea_proxy_stop()` on exit

### Phase 3: Unified Installer ✅ COMPLETE
- [x] **3.1** install.sh updated: Step 6/7 "Agent Zero (Optional)"
- [x] **3.2** Internal bridge token generated (`openssl rand -hex 32`)
- [x] **3.3** Agent Zero env auto-generated with proxy URL + internal token
- [x] **3.4** Docker detection during setup
- [x] **3.5** SeaZero config written to SQLite: seazero_enabled, seazero_internal_token, seazero_agent_url, seazero_token_budget
- [x] **3.6** Banner updated to v3.0.0, 58 tools

### Phase 4: Security Hardening ✅ COMPLETE
- [x] **4.1** Seccomp profile: `seazero/config/seccomp.json` (whitelist-only syscalls)
- [x] **4.2** PII filter on all Agent Zero output (`sea_pii_redact` with `SEA_PII_ALL`)
- [x] **4.3** Output size limit: 64KB max response from Agent Zero
- [x] **4.4** Network isolation: Docker bridge network, DNS to 8.8.8.8/1.1.1.1
- [x] **4.5** Credential isolation: Agent Zero env has ONLY internal token, no real keys
- [x] **4.6** Read-only root filesystem + tmpfs for /tmp and /run
- [x] **4.7** Shared workspace volume: `~/.seazero/workspace` mounted at `/agent/shared`
- [x] **4.8** Grammar Shield: `sea_shield_detect_output_injection` on all output
- [ ] **4.9** Penetration test: try to escape container, exfiltrate keys, inject via output

### Phase 5: Shared Workspace + File Delivery ✅ COMPLETE
- [x] **5.1** `sea_workspace.h/c`: workspace manager with create/list/read/cleanup
- [x] **5.2** Shared volume in docker-compose: `~/.seazero/workspace` → `/agent/shared`
- [x] **5.3** Path traversal protection: blocks `..` and absolute paths
- [x] **5.4** File size limits: 10MB per file, 100MB total (configurable)
- [x] **5.5** Auto-cleanup: removes workspaces older than 7 days (configurable)
- [x] **5.6** Wired into main.c: `sea_workspace_init()` on SeaZero startup

### Phase 6: TUI Enhancements ✅ COMPLETE
- [x] **6.1** `/agents` — list Agent Zero instances with status icons (●/●/●)
- [x] **6.2** `/delegate <task>` — delegate task to Agent Zero via bridge
- [x] **6.3** `/sztasks` — show delegated task history with status icons
- [x] **6.4** `/usage` — LLM token usage breakdown (SeaClaw vs Agent Zero)
- [x] **6.5** `/audit` — recent security events with severity icons
- [x] **6.6** `/help` updated with SeaZero command section
- [x] **6.7** `sea_db_sz_audit_list()` — query function for audit events

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
- `seazero/config/agent-zero.env.example`
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
