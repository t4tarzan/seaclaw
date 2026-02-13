# SeaZero — C + Python Hybrid AI Platform

> *The discipline of C. The autonomy of Python. One command to install.*

SeaZero combines SeaClaw (a C11 orchestrator) with Agent Zero (a Python autonomous agent) into a hybrid platform where C handles orchestration, security, and memory, while Python handles open-ended reasoning and code generation — all through a single install command.

---

## The Architecture

```
┌──────────────────────────────────────────────────────────────────┐
│                        YOUR MACHINE                               │
│                                                                   │
│   ┌──────────────────────────────────────────────────────────┐   │
│   │              SeaClaw (C11 Binary, ~203KB)                 │   │
│   │                                                           │   │
│   │  ┌─────────┐ ┌─────────┐ ┌──────────┐ ┌──────────────┐  │   │
│   │  │  Arena   │ │ Grammar │ │   LLM    │ │  58 Static   │  │   │
│   │  │ Memory   │ │ Shield  │ │  Proxy   │ │   Tools      │  │   │
│   │  │ (zero    │ │ (byte-  │ │ (port    │ │  (compiled   │  │   │
│   │  │  malloc) │ │  level) │ │  7432)   │ │   in C)      │  │   │
│   │  └─────────┘ └─────────┘ └────┬─────┘ └──────┬───────┘  │   │
│   │                                │              │           │   │
│   │  ┌─────────────────────────────┴──────────────┘           │   │
│   │  │  Tool #58: agent_zero (bridge)                         │   │
│   │  │  HTTP JSON → Agent Zero container                      │   │
│   │  └─────────────────────────────┬──────────────────────┐   │   │
│   │                                │                      │   │   │
│   │  ┌─────────┐ ┌────────────┐   │   ┌──────────────┐   │   │   │
│   │  │ SQLite  │ │ Workspace  │   │   │  PII Filter  │   │   │   │
│   │  │ v3 DB   │ │ Manager    │   │   │  + Output    │   │   │   │
│   │  │ (audit, │ │ (shared    │   │   │  Size Limit  │   │   │   │
│   │  │  tasks, │ │  files)    │   │   │  (64KB max)  │   │   │   │
│   │  │  usage) │ │            │   │   │              │   │   │   │
│   │  └─────────┘ └────────────┘   │   └──────────────┘   │   │   │
│   └───────────────────────────────┼───────────────────────┘   │   │
│                                   │                           │   │
│   ┌───────────────────────────────┼───────────────────────┐   │   │
│   │         Docker Container      │  (isolated)           │   │   │
│   │                               ▼                       │   │   │
│   │   ┌───────────────────────────────────────────────┐   │   │   │
│   │   │          Agent Zero (Python)                   │   │   │   │
│   │   │                                                │   │   │   │
│   │   │  • Autonomous reasoning + code generation      │   │   │   │
│   │   │  • Web browsing + research                     │   │   │   │
│   │   │  • File creation in shared workspace           │   │   │   │
│   │   │  • Multi-step task execution                   │   │   │   │
│   │   │                                                │   │   │   │
│   │   │  LLM access: via SeaClaw proxy ONLY            │   │   │   │
│   │   │  API key: NEVER sees the real key              │   │   │   │
│   │   │  Network: isolated (bridge + internet only)    │   │   │   │
│   │   │  Filesystem: read-only root + tmpfs            │   │   │   │
│   │   │  Syscalls: seccomp whitelist only              │   │   │   │
│   │   └───────────────────────────────────────────────┘   │   │   │
│   └───────────────────────────────────────────────────────┘   │   │
│                                                                   │
└──────────────────────────────────────────────────────────────────┘
```

---

## The Complete Flow

### 1. Installation (One Command)

```bash
curl -fsSL https://raw.githubusercontent.com/t4tarzan/seaclaw/main/install.sh | bash
```

The interactive installer handles everything:

| Step | What Happens |
|------|-------------|
| **System Check** | Detects Linux, verifies sudo, checks package manager |
| **Dependencies** | Installs gcc, make, libcurl, libsqlite3, git |
| **Build** | Clones repo, compiles release binary (~203KB), runs 141 tests |
| **LLM Provider** | Arrow-key menu: OpenRouter / OpenAI / Gemini / Anthropic / Local |
| **Telegram** | Optional bot token + chat ID |
| **Agent Zero** | Optional: checks Docker, generates internal bridge token, writes proxy config |
| **Launch** | Saves config, offers TUI or Telegram launch |

### 2. What Happens at Startup

When SeaClaw starts with Agent Zero enabled:

```
1. sea_config_load()           → Read config.json
2. sea_db_open()               → Open SQLite (v3 schema: 8 tables)
3. sea_tools_init()            → Register 58 tools (including agent_zero)
4. sea_proxy_start()           → Start LLM proxy on 127.0.0.1:7432
5. sea_workspace_init()        → Create ~/.seazero/workspace/
6. Docker container starts     → Agent Zero on port 8080
7. Agent Zero connects         → Uses proxy for LLM (never sees real API key)
8. TUI / Telegram ready        → User can delegate tasks
```

### 3. Task Delegation Flow

When a user types `/delegate Write a Python script to analyze CSV data`:

```
User (TUI/Telegram)
    │
    ▼
SeaClaw receives command
    │
    ├─ Grammar Shield validates input (byte-level, <1μs)
    │
    ├─ Creates task in SQLite: seazero_tasks table
    │
    ├─ Creates workspace: ~/.seazero/workspace/<task-id>/
    │
    ▼
Bridge (sea_zero.c) sends HTTP POST to Agent Zero
    │
    ├─ JSON payload: { task, context, max_steps }
    │
    ├─ Bearer token: internal bridge token (not the LLM API key)
    │
    ▼
Agent Zero (Docker container) receives task
    │
    ├─ Reasons about the task autonomously
    │
    ├─ Needs LLM? → Calls SeaClaw proxy on port 7432
    │   │
    │   ├─ Proxy validates internal token
    │   ├─ Proxy checks daily token budget (default: 100K tokens/day)
    │   ├─ Proxy forwards to real LLM API (Agent Zero never sees the key)
    │   ├─ Proxy logs usage: tokens_in, tokens_out, latency, cost
    │   └─ Returns LLM response to Agent Zero
    │
    ├─ Writes files to shared workspace (/agent/shared/)
    │
    ├─ Returns result JSON to SeaClaw bridge
    │
    ▼
SeaClaw receives Agent Zero output
    │
    ├─ Output size check: reject if > 64KB
    │
    ├─ Grammar Shield: detect output injection
    │
    ├─ PII Filter: redact emails, phones, SSNs, credit cards, IPs
    │
    ├─ Update task status in SQLite
    │
    ├─ Log audit event
    │
    ▼
User sees result (TUI/Telegram)
    │
    └─ Workspace files available at ~/.seazero/workspace/<task-id>/
```

### 4. Security Layers

Every interaction between SeaClaw and Agent Zero passes through 8 security layers:

| Layer | What | Where |
|-------|------|-------|
| **1. Docker Isolation** | Container with seccomp, read-only rootfs, no-new-privileges | docker-compose.yml |
| **2. Network Isolation** | Bridge network, DNS to 8.8.8.8/1.1.1.1 only | docker-compose.yml |
| **3. Credential Isolation** | Agent Zero only has internal token, never real API keys | sea_proxy.c |
| **4. Token Budget** | Daily limit (default 100K tokens/day) enforced by proxy | sea_proxy.c |
| **5. Grammar Shield** | Byte-level validation of all output from Agent Zero | sea_zero.c |
| **6. PII Filter** | Redacts leaked emails, phones, SSNs, credit cards | sea_zero.c |
| **7. Output Size Limit** | 64KB max response from Agent Zero | sea_zero.c |
| **8. Full Audit Trail** | Every task, LLM call, and security event logged to SQLite | sea_db.c |

### 5. TUI Commands

| Command | Description |
|---------|-------------|
| `/agents` | List Agent Zero instances with status (●/●/●) |
| `/delegate <task>` | Delegate a task to Agent Zero |
| `/sztasks` | Show delegated task history |
| `/usage` | LLM token usage breakdown (SeaClaw vs Agent Zero) |
| `/audit` | Recent security events |

### 6. Database Schema (v3)

SeaZero adds 4 tables to SeaClaw's SQLite database:

| Table | Purpose |
|-------|---------|
| `seazero_agents` | Registered Agent Zero instances (id, port, status, model) |
| `seazero_tasks` | Delegated tasks (task_id, agent_id, status, result, elapsed) |
| `seazero_llm_usage` | Per-call LLM usage (caller, model, tokens_in/out, latency, cost) |
| `seazero_audit` | Security events (event_type, source, target, severity) |

### 7. File Structure

```
seazero/
├── bridge/
│   ├── sea_zero.h/c          # Bridge API: delegate tasks to Agent Zero
│   ├── sea_proxy.h/c         # LLM proxy server on port 7432
│   └── sea_workspace.h/c     # Shared workspace manager
├── config/
│   ├── seccomp.json           # Syscall whitelist for container
│   └── agent-zero.env         # Environment template
├── scripts/
│   ├── setup.sh               # Docker image setup
│   └── spawn-agent.sh         # Agent lifecycle management
├── docker-compose.yml         # Container configuration
├── SEAZERO_PLAN.md            # Master implementation plan
└── README.md                  # SeaZero documentation
```

---

## Why C + Python?

| Aspect | C (SeaClaw) | Python (Agent Zero) | Together |
|--------|-------------|---------------------|----------|
| **Role** | Orchestrator, security, memory | Autonomous executor, reasoning | Best of both worlds |
| **Binary** | ~203KB, starts in <1ms | Docker container, ~500MB | C stays lean, Python stays isolated |
| **Memory** | Arena allocation, zero leaks | GC-managed | C controls the budget |
| **Security** | Grammar Shield, byte-level | Sandboxed in Docker | C is the gatekeeper |
| **LLM Access** | Direct API calls | Via C proxy only | C controls the keys and budget |
| **Tools** | 57 compiled-in (static) | Dynamic (code generation) | 57 fast + unlimited dynamic |
| **Speed** | μs-level operations | ms-level operations | Fast path in C, complex path in Python |

---

## Quick Start

```bash
# Install SeaClaw + optional Agent Zero
curl -fsSL https://raw.githubusercontent.com/t4tarzan/seaclaw/main/install.sh | bash

# In the TUI:
🦀 > /agents                    # Check Agent Zero status
🦀 > /delegate Analyze my CSV   # Delegate a complex task
🦀 > /sztasks                   # Check task progress
🦀 > /usage                     # See token consumption
```
