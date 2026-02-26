# SeaClaw Platform — Master Plan V3
> **Date**: 2026-02-26  
> **Status**: Active  
> **Supersedes**: SEACLAW_PLATFORM_PLAN.md, ROADMAP_V2.md

---

## Architecture Decision Record

### ADR-1: Agent Zero — Optional Power Tier, Not Default

**Old thinking**: Every agent has Agent Zero (Python Docker, 2GB RAM) for autonomous tasks.

**New thinking**: Kimi K2.5 (and frontier models in general) can handle 90% of what Agent Zero was
needed for — multi-step reasoning, code generation, web research, file manipulation — **directly
inside SeaClaw** via native C tools + a capable LLM. Agent Zero is now **opt-in, premium tier**:

| Use case | Solution |
|----------|----------|
| Git clone, pull, status, diff | Native C tool (tool_git) — NEW |
| File read/write/search/edit | Already built (tools 1-10) |
| Shell command execution | Already built (tool_shell) |
| Web search | Already built (tool_web_search) |
| Project reports, CSV analysis | LLM + native tools sufficient |
| Long-running Python code execution | Agent Zero (premium) |
| Docker-in-Docker (pentest, custom envs) | Agent Zero (premium) |
| Multi-day autonomous task with checkpoints | Agent Zero (premium) |

**Result**: Default tier = SeaClaw alone (28MB, any LLM). Premium tier = SeaClaw + Agent Zero pod.

---

### ADR-2: Agent Swarm via SeaClaw A2A (Not Python Multi-Agent)

**Old thinking**: Agent swarm requires multiple Agent Zero Python containers.

**New thinking**: SeaClaw already has `tool_spawn` (sub-agent spawning) and A2A protocol.
On K8s, a "coordinator" SeaClaw pod can spawn task-specific "worker" SeaClaw pods via the
gateway API. Workers are just SeaClaw pods with a specific SOUL.md — same 28MB binary.

```
User → Coordinator Pod (alec)
           │
           ├── worker-git   (clones repo, reads files)
           ├── worker-test  (runs tests, reports failures)
           └── worker-doc   (generates documentation)
           │
           ▼
       Result aggregated → User
```

This is **Kimi K2.5 swarm mode**: the coordinator LLM decomposes the task, spawns workers
(K8s pods via gateway API), collects results, synthesizes. All SeaClaw, zero Python overhead.

**Toggle**: `swarm_mode: on/off` in agent config. Off by default — just use the one agent.

---

### ADR-3: K8s Multi-Node Autoscaling

Current: Single K3s node (this VPS). SeaClaw pods are **stateless** (all data in PVC).

Plan:
- **K3s agent join**: Any device (RPi, second VPS, bare metal) runs `k3s agent --server <url>` 
  and pods auto-schedule across nodes based on resource availability
- **Node labels**: Label nodes by capability (`gpu=true`, `high-memory=true`, `arm64=true`)
  so coordinator pods go to x86 and worker pods can go to RPi
- **PVC strategy**: Longhorn distributed storage OR NFS so PVCs follow pods across nodes
- **HPA**: Horizontal Pod Autoscaler on gateway deployment (scale 1→10 replicas under load)

---

### ADR-4: Agent Zero LLM Config in Signup Form

Agent Zero currently has no separate LLM config — it inherits the user's key via the proxy.
New design: signup form has an **"Enable Agent Zero" toggle** with:
- Same LLM key (default — AZ uses user's key via proxy)  
- Separate AZ key (power user — different key/model for AZ, e.g. cheaper model for AZ steps)
- Token budget (default 100K tokens/day for AZ calls)

---

## Platform — Current State (as of 2026-02-26)

```
✅ DONE
├── K3s cluster on VPS
├── gateway FastAPI service (NodePort 30090)
├── Nginx reverse proxy → /platform/
├── Multi-tenant SeaClaw pods (28MB each, <2s startup)
├── Signup form: username, email, LLM provider, API key, model, soul
├── WebChat: browser chat at /platform/chat/{username}
├── Kimi K2.5 via OpenRouter responding end-to-end
├── PVC per user (/workspace, /userdata)
├── SeaClaw HTTP API fix (Content-Length body read)
├── SeaClaw JSON parser fix (whitespace after colon)
├── OpenRouter HTTP-Referer + X-Title headers
└── All frontend URLs relative (works under /platform/ prefix)

❌ NOT DONE (by phase below)
```

---

## Phases

### Phase 1 — Platform Dashboard (Sprint 1)
**Goal**: Replace the basic "Active Agents" list with a real project management dashboard.

| ID | Task | Effort | Files |
|----|------|--------|-------|
| P1-01 | Dashboard tab: agent card with usage stats, uptime, model | S | templates/index.html |
| P1-02 | Projects tab: create project, link to git repo, assign to agent | M | templates/index.html, main.py |
| P1-03 | `POST /api/v1/agents/{user}/project` — clone repo into /workspace | M | main.py |
| P1-04 | `GET /api/v1/agents/{user}/workspace` — list files in /workspace | S | main.py |
| P1-05 | Task board: list SeaZero tasks from pod DB (pending/running/done) | M | templates/index.html, main.py |
| P1-06 | `GET /api/v1/agents/{user}/tasks` — proxy to pod /api/tasks | S | main.py |
| P1-07 | Agent settings panel: change model, update API key | M | templates/index.html, main.py |
| P1-08 | `PATCH /api/v1/agents/{user}/config` — update config.json in pod | M | main.py |
| P1-09 | Telegram token field in signup + settings (optional) | S | templates/index.html, main.py |
| P1-10 | Swarm toggle in settings panel (on/off) | S | templates/index.html |

**Deliverable**: Dashboard at `/platform/` shows projects, tasks, workspace files, agent settings.

---

### Phase 2 — Native Git Tools in SeaClaw (Sprint 2)
**Goal**: Add git as first-class C tools so any agent can work with repos from chat.

| ID | Task | Effort | Files |
|----|------|--------|-------|
| P2-01 | `tool_git_clone` — clone a repo into /workspace/{project} | M | src/hands/tool_git.c |
| P2-02 | `tool_git_pull` — pull latest changes | S | src/hands/tool_git.c |
| P2-03 | `tool_git_status` — show changed files | S | src/hands/tool_git.c |
| P2-04 | `tool_git_diff` — show diff of changed files | S | src/hands/tool_git.c |
| P2-05 | `tool_git_log` — show recent commits | S | src/hands/tool_git.c |
| P2-06 | `tool_git_checkout` — switch branch | S | src/hands/tool_git.c |
| P2-07 | Register tools #65-70 in sea_tools.c | S | src/hands/sea_tools.c |
| P2-08 | Rebuild Docker image + redeploy to K3s | S | platform/docker/Dockerfile.seaclaw |
| P2-09 | Test: ask alec to "clone https://github.com/... and summarize it" | S | — |

**Design**: All git tools use `popen("git ...")`  with sanitized args (Grammar Shield validates
repo URL before exec). Output piped back to agent as text. No libgit2 dependency needed.

---

### Phase 3 — Project Management Tools (Sprint 2 cont.)
**Goal**: Native C tools for tasks, milestones, reports — no Agent Zero needed for PM work.

| ID | Task | Effort | Files |
|----|------|--------|-------|
| P3-01 | `tool_task_create` — create task in SQLite seazero_tasks | S | src/hands/tool_pm.c |
| P3-02 | `tool_task_list` — list tasks by status/project | S | src/hands/tool_pm.c |
| P3-03 | `tool_task_update` — update task status, add notes | S | src/hands/tool_pm.c |
| P3-04 | `tool_report_generate` — LLM summarizes tasks into markdown report | M | src/hands/tool_pm.c |
| P3-05 | `tool_milestone` — set milestone, track % complete | S | src/hands/tool_pm.c |
| P3-06 | Register tools in sea_tools.c | S | src/hands/sea_tools.c |
| P3-07 | Add `GET /api/tasks` to sea_api.c (gateway can poll per-pod) | M | src/api/sea_api.c |
| P3-08 | Dashboard: Kanban columns (To Do / In Progress / Done) from tasks API | M | templates/index.html |

---

### Phase 4 — Agent Swarm (Sprint 3)
**Goal**: Coordinator agent spawns worker agents (K8s pods) for parallel task execution.

| ID | Task | Effort | Files |
|----|------|--------|-------|
| P4-01 | `tool_spawn_worker` — call gateway to create ephemeral worker pod | M | src/hands/tool_swarm.c |
| P4-02 | Worker pod lifecycle: auto-delete after task complete | M | main.py |
| P4-03 | `POST /api/v1/agents/{user}/workers` — create named worker | M | main.py |
| P4-04 | Inter-pod messaging via gateway relay | M | main.py, sea_api.c |
| P4-05 | Coordinator prompt template: decompose → assign → collect | M | souls/coordinator.md |
| P4-06 | Swarm toggle in user config + dashboard UI | S | main.py, templates |
| P4-07 | Test: "analyze this codebase and generate a PR review" end-to-end | M | — |

---

### Phase 5 — Agent Zero Integration (Sprint 4, Optional/Premium)
**Goal**: Agent Zero as opt-in premium tier for long Python execution + Docker-in-Docker.

| ID | Task | Effort | Files |
|----|------|--------|-------|
| P5-01 | Build Agent Zero Docker image for K3s | M | platform/docker/Dockerfile.agentzeroe |
| P5-02 | K8s manifest: shared agent-zero pod + Service | S | platform/k8s/agent-zero.yaml |
| P5-03 | Signup form: "Enable Agent Zero" toggle + separate LLM key option | S | templates/index.html |
| P5-04 | Per-user token budget config field (default 100K/day) | S | main.py |
| P5-05 | Gateway injects SEAZERO_AGENT_URL + SEAZERO_TOKEN into pod env | M | main.py |
| P5-06 | LLM proxy (sea_proxy.c) multi-tenant: per-user token + budget tracking | H | src/seazero/sea_proxy.c |
| P5-07 | Dashboard: AZ status indicator + task queue depth | M | templates/index.html |
| P5-08 | Test: ask agent to "run a Python script that downloads and analyzes data" | M | — |

---

### Phase 6 — K8s Multi-Node + Autoscaling (Sprint 5)
**Goal**: Cluster grows across devices; gateway scales under load.

| ID | Task | Effort | Files |
|----|------|--------|-------|
| P6-01 | K3s agent join script (for RPi / second VPS) | S | platform/scripts/join-node.sh |
| P6-02 | Node labels: capability-based scheduling (arm64, gpu, high-memory) | S | platform/k8s/node-labels.yaml |
| P6-03 | Longhorn storage OR NFS setup for cross-node PVCs | H | platform/k8s/storage.yaml |
| P6-04 | HPA for gateway: scale 1→10 replicas (CPU >50%) | S | platform/k8s/gateway-hpa.yaml |
| P6-05 | PodDisruptionBudget for gateway (always 1 available) | S | platform/k8s/gateway-pdb.yaml |
| P6-06 | Resource limits on SeaClaw pods (100m CPU, 64Mi RAM limit) | S | main.py (pod spec) |
| P6-07 | LimitRange + ResourceQuota per namespace | S | platform/k8s/quotas.yaml |
| P6-08 | Test: join RPi node, create agent, verify pod schedules to RPi | M | — |

---

### Phase 7 — Multi-Channel (Sprint 6)
**Goal**: Agents accessible via Discord, Slack, WhatsApp — not just WebChat.

| ID | Task | Effort | Files |
|----|------|--------|-------|
| P7-01 | `channel_discord.c` — Discord bot via HTTP Events API | H | src/channels/ |
| P7-02 | `channel_slack.c` — Slack via Socket Mode | H | src/channels/ |
| P7-03 | Discord/Slack token fields in signup form + settings | M | templates/index.html |
| P7-04 | Gateway injects channel tokens into pod env vars | M | main.py |
| P7-05 | Voice support: Whisper transcription via Groq | M | src/channels/sea_voice.c |

---

## Task Database Schema

All tasks above will be tracked in a `platform_tasks` SQLite table in the gateway DB:

```sql
CREATE TABLE platform_tasks (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    phase       TEXT NOT NULL,          -- 'P1', 'P2', etc.
    task_id     TEXT NOT NULL UNIQUE,   -- 'P1-01', 'P2-03', etc.
    title       TEXT NOT NULL,
    effort      TEXT,                   -- 'S', 'M', 'H'
    status      TEXT DEFAULT 'todo',    -- 'todo', 'in_progress', 'done', 'blocked'
    sprint      INTEGER,               -- sprint number
    files       TEXT,                   -- comma-separated affected files
    notes       TEXT,
    created_at  DATETIME DEFAULT (datetime('now')),
    updated_at  DATETIME DEFAULT (datetime('now'))
);
```

---

## Effort Key
- **S** = Small (< 2 hours)
- **M** = Medium (2–8 hours)  
- **H** = Heavy (1–3 days)

## Sprint Schedule

| Sprint | Phases | Focus |
|--------|--------|-------|
| Sprint 1 | P1 | Dashboard + Projects tab |
| Sprint 2 | P2, P3 | Git tools + PM tools in SeaClaw core |
| Sprint 3 | P4 | Agent Swarm (Kimi K2.5 native) |
| Sprint 4 | P5 | Agent Zero premium tier |
| Sprint 5 | P6 | K8s multi-node + autoscaling |
| Sprint 6 | P7 | Multi-channel (Discord, Slack) |

---

## What We Explicitly Do NOT Build

- ❌ React / Next.js frontend — plain HTML/JS only
- ❌ PostgreSQL / Redis — SQLite per pod, gateway has its own SQLite
- ❌ Agent Zero as default — it's opt-in premium
- ❌ Native mobile apps — Telegram + WebChat covers 95%
- ❌ Kubernetes on a managed cloud (EKS/GKE) — K3s self-hosted only
- ❌ pgvector / embeddings — keyword recall in SQLite is sufficient
- ❌ Stripe billing (yet) — Phase F, future
