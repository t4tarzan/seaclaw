# Sea-Claw Mesh — Distributed Agent Network Architecture

> **Date:** 2025-02-12
> **Purpose:** Design a local-network mesh where multiple Sea-Claw nodes share one central LLM server, with zero data leaving the LAN.

---

## 1. The Vision

```
┌─────────────────────────────────────────────────────────────────────┐
│                        LOCAL NETWORK (LAN)                          │
│                     No data leaves this boundary                    │
│                                                                     │
│   ┌─────────────────────────────────────────────┐                   │
│   │         CENTRAL NODE (M4 Mac, 64 GB)        │                   │
│   │                                              │                   │
│   │   Ollama (llama3:70b or qwen2.5:72b)        │                   │
│   │   ├── /v1/chat/completions (port 11434)     │                   │
│   │   └── KV cache: Q8_0, 32K context           │                   │
│   │                                              │                   │
│   │   Sea-Claw CAPTAIN (port 9100)              │                   │
│   │   ├── /mesh/register    (nodes join here)   │                   │
│   │   ├── /mesh/heartbeat   (health checks)     │                   │
│   │   ├── /mesh/task        (submit tasks)      │                   │
│   │   ├── /mesh/result      (return results)    │                   │
│   │   ├── /mesh/discover    (list all nodes)    │                   │
│   │   └── /mesh/broadcast   (push to all nodes) │                   │
│   │                                              │                   │
│   │   SQLite (shared task queue + audit log)     │                   │
│   │   Telegram channel (single outbound)         │                   │
│   └──────────────────────────────────────────────┘                   │
│          ▲           ▲           ▲           ▲                       │
│          │           │           │           │                       │
│     ┌────┴───┐  ┌────┴───┐  ┌────┴───┐  ┌────┴───┐                 │
│     │ NODE 1 │  │ NODE 2 │  │ NODE 3 │  │ NODE 4 │                 │
│     │ Laptop │  │ Desktop│  │ RPi    │  │ Server │                 │
│     │ 2 MB   │  │ 2 MB   │  │ 2 MB   │  │ 2 MB   │                 │
│     │        │  │        │  │        │  │        │                 │
│     │ Tools: │  │ Tools: │  │ Tools: │  │ Tools: │                 │
│     │ files  │  │ docker │  │ GPIO   │  │ DB     │                 │
│     │ shell  │  │ build  │  │ sensors│  │ backup │                 │
│     │ web    │  │ test   │  │ camera │  │ deploy │                 │
│     └────────┘  └────────┘  └────────┘  └────────┘                 │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 2. Node Roles

### Captain (Central Node — M4 Mac 64 GB)

The Captain is the **only node that talks to the LLM**. All other nodes delegate to it.

| Responsibility | How |
|---|---|
| Run Ollama with large model (70B) | `ollama serve` on port 11434 |
| Run Sea-Claw in captain mode | `./sea_claw --mode captain --port 9100` |
| Accept task submissions from nodes | HTTP POST `/mesh/task` |
| Route tasks to LLM (Ollama) | `sea_agent_chat()` → `localhost:11434` |
| Dispatch tool calls to the right node | Task routing based on node capabilities |
| Maintain node registry | SQLite `mesh_nodes` table |
| Run Telegram channel | Single outbound to user |
| Audit everything | SQLite event log |

### Crew (Worker Nodes — Any Machine)

Crew nodes are **lightweight Sea-Claw binaries** (2 MB) that:

| Responsibility | How |
|---|---|
| Register with Captain on startup | HTTP POST `/mesh/register` |
| Advertise their capabilities | List of tools they can run |
| Execute tool calls dispatched by Captain | HTTP POST `/node/exec` |
| Send heartbeats | HTTP GET `/mesh/heartbeat` every 30s |
| Accept tasks from user (optional) | CLI, Telegram, or local HTTP |
| Forward LLM requests to Captain | HTTP POST to Captain's `/mesh/task` |

---

## 3. How It Works — Task Flow

### Example: User sends "analyze salaries.xlsx and generate a chart" via Telegram

```
Step 1: User → Telegram → Captain
  Telegram message arrives at Captain's Telegram handler

Step 2: Captain → Shield → LLM
  Captain validates input (Shield)
  Captain sends to Ollama (localhost:11434)
  Ollama returns: tool_call: file_read("salaries.xlsx")

Step 3: Captain → Route → Node 1 (has the file)
  Captain checks which node has file access
  Captain sends tool_call to Node 1 via HTTP POST /node/exec
  Node 1 executes file_read, returns CSV data

Step 4: Captain → LLM (with file data)
  Captain feeds file data back to Ollama
  Ollama returns: tool_call: shell_exec("python3 chart.py")

Step 5: Captain → Route → Node 1 (or Node 2 if it has Python)
  Captain dispatches shell_exec to appropriate node
  Node executes Python, generates chart

Step 6: Captain → LLM → Telegram → User
  Captain feeds result back to Ollama
  Ollama generates final response
  Captain sends response + chart to Telegram
```

### Example: Node 3 (Raspberry Pi) detects sensor anomaly

```
Step 1: Node 3 cron job reads temperature sensor → 95°C (too hot!)

Step 2: Node 3 → Captain /mesh/task
  POST { "task": "Server room temperature is 95°C. Alert the user.",
         "source": "node3-rpi", "priority": "urgent" }

Step 3: Captain → LLM → Telegram
  Ollama formats alert message
  Captain sends to Telegram: "⚠️ Node 3 (RPi) reports server room at 95°C"

Step 4: User → Telegram → Captain → Node 3
  User: "turn on the cooling fan"
  Captain routes to Node 3: tool_call: shell_exec("gpio write 7 1")
  Node 3 activates GPIO pin 7
```

---

## 4. Network Protocol

### 4a. Node Registration

When a crew node starts, it registers with the Captain:

```json
POST http://captain:9100/mesh/register
{
  "node_id": "node1-laptop",
  "hostname": "kiran-macbook",
  "ip": "192.168.1.42",
  "port": 9101,
  "capabilities": ["file_read", "file_write", "shell_exec", "web_fetch",
                    "python_exec", "docker"],
  "os": "darwin",
  "arch": "arm64",
  "version": "0.12.0",
  "secret": "mesh-shared-secret-from-config"
}
```

Captain responds:

```json
{
  "status": "registered",
  "node_id": "node1-laptop",
  "captain_id": "captain-m4",
  "mesh_token": "eyJ...",
  "llm_model": "qwen2.5:72b",
  "heartbeat_interval_ms": 30000
}
```

### 4b. Heartbeat

Every 30 seconds, each node pings the Captain:

```json
GET http://captain:9100/mesh/heartbeat?node_id=node1-laptop&token=eyJ...

Response: { "status": "ok", "tasks_pending": 2 }
```

If a node misses 3 heartbeats, Captain marks it as `offline`.

### 4c. Task Submission

Any node (or the Captain itself) can submit a task:

```json
POST http://captain:9100/mesh/task
{
  "task": "Read /home/kiran/data/salaries.xlsx and summarize the top 5 earners",
  "source": "telegram:123456",
  "priority": "normal",
  "context": { "chat_id": 123456, "reply_to": "telegram" },
  "token": "eyJ..."
}
```

Captain queues it, sends to LLM, dispatches tool calls to nodes.

### 4d. Tool Dispatch (Captain → Node)

When the LLM requests a tool call, Captain routes it to the right node:

```json
POST http://node1:9101/node/exec
{
  "task_id": "sea-abc123",
  "tool": "file_read",
  "args": "/home/kiran/data/salaries.xlsx",
  "token": "eyJ..."
}
```

Node executes and returns:

```json
{
  "task_id": "sea-abc123",
  "success": true,
  "output": "Name,Department,Salary\nAlice,Engineering,150000\n...",
  "duration_ms": 12
}
```

### 4e. Discovery

Any node can discover all other nodes:

```json
GET http://captain:9100/mesh/discover?token=eyJ...

{
  "captain": {
    "node_id": "captain-m4",
    "ip": "192.168.1.10",
    "llm_model": "qwen2.5:72b",
    "uptime_hours": 48
  },
  "nodes": [
    { "node_id": "node1-laptop", "ip": "192.168.1.42", "status": "online",
      "capabilities": ["file_read", "shell_exec", "python_exec"] },
    { "node_id": "node2-desktop", "ip": "192.168.1.50", "status": "online",
      "capabilities": ["docker", "build", "test"] },
    { "node_id": "node3-rpi", "ip": "192.168.1.60", "status": "online",
      "capabilities": ["gpio", "sensors", "camera"] }
  ]
}
```

---

## 5. Security Model — Zero Trust Within the LAN

```
Layer 1: Shared Secret
  ├── All nodes share a mesh secret (from config.json)
  ├── Captain issues JWT-like mesh tokens on registration
  ├── Every request carries the token
  └── No token = rejected immediately

Layer 2: IP Allowlist
  ├── Captain only accepts connections from configured subnet
  ├── Default: 192.168.0.0/16, 10.0.0.0/8, 172.16.0.0/12
  └── Configurable in config.json: "mesh_allowed_subnets"

Layer 3: Capability Gating
  ├── Each node declares its capabilities on registration
  ├── Captain only dispatches tools that the node advertised
  ├── Node rejects tool calls it didn't advertise
  └── Prevents lateral movement (RPi can't run docker)

Layer 4: Shield on Every Boundary
  ├── Captain shields user input (strict)
  ├── Captain shields LLM output (relaxed)
  ├── Nodes shield tool args before execution
  ├── Nodes shield tool output before returning
  └── Double-shield: Captain re-shields node output

Layer 5: Audit Trail
  ├── Every task, tool call, and result logged to SQLite
  ├── Captain logs: who submitted, which node executed, what result
  ├── Nodes log: what they executed, for which task
  └── Full chain of custody for every operation

Layer 6: No External Network
  ├── LLM runs locally (Ollama on Captain)
  ├── All mesh traffic is LAN-only
  ├── No API keys leave the network
  ├── No data touches the internet
  └── Optional: bind to specific interface (e.g., eth0, not wlan0)
```

### Data Flow Guarantee

```
User (Telegram) ──internet──▶ Captain ──LAN only──▶ Nodes
                                │
                                ▼
                          Ollama (localhost)
                          No internet needed

The ONLY internet connection is Telegram ↔ Captain.
Everything else stays on the LAN.
Even Telegram can be replaced with a local WebChat for full air-gap.
```

---

## 6. Configuration

### Captain config.json

```json
{
  "mode": "captain",
  "llm_provider": "local",
  "llm_api_url": "http://localhost:11434/v1/chat/completions",
  "llm_model": "qwen2.5:72b",

  "mesh": {
    "enabled": true,
    "port": 9100,
    "secret": "your-mesh-secret-here",
    "allowed_subnets": ["192.168.1.0/24"],
    "heartbeat_interval_ms": 30000,
    "heartbeat_timeout_count": 3,
    "max_nodes": 16
  },

  "telegram_token": "...",
  "telegram_chat_id": 123456,
  "db_path": "seaclaw.db",
  "arena_size_mb": 32
}
```

### Crew node config.json

```json
{
  "mode": "crew",
  "captain_url": "http://192.168.1.10:9100",
  "mesh_secret": "your-mesh-secret-here",

  "node_id": "node1-laptop",
  "node_port": 9101,
  "capabilities": ["file_read", "file_write", "shell_exec",
                    "web_fetch", "python_exec"],

  "db_path": "seaclaw-node.db",
  "arena_size_mb": 8
}
```

---

## 7. What Changes in the Code

### 7a. New: Mesh HTTP Server (both Captain and Crew)

Sea-Claw currently only makes outbound HTTP requests (`sea_http.c`).
For the mesh, nodes need to **listen** for inbound HTTP requests.

**New file: `src/mesh/sea_mesh_server.c`**

Minimal HTTP server using raw sockets (no dependency):
- Bind to `0.0.0.0:port`
- Accept connections
- Parse HTTP request line + headers + body
- Route to handler based on path
- Return JSON response

This is ~300 lines of C. No external library needed — just POSIX sockets.

### 7b. New: Mesh Protocol (`src/mesh/sea_mesh.c`)

| Function | Purpose |
|---|---|
| `sea_mesh_init()` | Start mesh server, load config |
| `sea_mesh_register()` | (Crew) Register with Captain |
| `sea_mesh_heartbeat_loop()` | (Crew) Background heartbeat thread |
| `sea_mesh_accept_registration()` | (Captain) Handle node registration |
| `sea_mesh_route_tool()` | (Captain) Route tool call to best node |
| `sea_mesh_dispatch_tool()` | (Captain) HTTP POST tool call to node |
| `sea_mesh_exec_tool()` | (Crew) Execute tool call from Captain |
| `sea_mesh_submit_task()` | (Any) Submit task to Captain |
| `sea_mesh_discover()` | (Any) List all nodes |
| `sea_mesh_broadcast()` | (Captain) Push message to all nodes |

### 7c. Modified: `sea_agent.c`

When the LLM requests a tool call:
- **Current:** Execute locally via `sea_tool_exec()`
- **Mesh mode:** Check if tool should run locally or on a remote node
  - If local capability → `sea_tool_exec()` (same as now)
  - If remote capability → `sea_mesh_dispatch_tool()` → HTTP to node → get result

### 7d. Modified: `main.c`

New `--mode captain` and `--mode crew` flags:
- **Captain:** Start mesh server + Telegram + LLM
- **Crew:** Start mesh client + register with Captain + listen for tool calls
- **Standalone (default):** Current behavior, no mesh

### 7e. New: Mesh Token Auth (`src/mesh/sea_mesh_auth.c`)

Simple HMAC-based token:
- Captain generates token = HMAC-SHA256(secret, node_id + timestamp)
- Node includes token in every request
- Captain validates token on every request
- Tokens expire after 24 hours, auto-renewed on heartbeat

---

## 8. Tool Routing — How Captain Decides Where to Run

```
Captain receives tool_call: shell_exec("docker build -t myapp .")

Step 1: Check capability registry
  node1-laptop:  [file_read, shell_exec, python_exec]     ← has shell_exec
  node2-desktop: [docker, build, test, shell_exec]         ← has shell_exec AND docker
  node3-rpi:     [gpio, sensors, camera]                   ← no shell_exec

Step 2: Prefer specialized capability
  "docker build" → node2 has "docker" capability → route to node2

Step 3: If tie, prefer least loaded
  node1: 2 active tasks
  node2: 0 active tasks → route to node2

Step 4: If node is offline, fallback
  node2 offline → try node1 (has shell_exec) → route to node1

Step 5: If no node can handle it
  → Captain executes locally (if it has the tool)
  → Or return error: "No node available for tool: docker"
```

---

## 9. Comparison: Sea-Claw Mesh vs OpenClaw

| Aspect | OpenClaw | Sea-Claw Mesh |
|---|---|---|
| **Architecture** | Single gateway + device nodes | Captain + crew mesh |
| **LLM location** | Cloud (Anthropic/OpenAI) | Local (Ollama on Captain) |
| **Data leaves LAN?** | Yes (every message → cloud) | No (LAN only) |
| **Node binary size** | ~200 MB (Node.js) | 2 MB (C binary) |
| **Node RAM** | 150-300 MB | 8-17 MB |
| **Node platforms** | macOS, iOS, Android | Any POSIX (Linux, macOS, BSD, RPi) |
| **Tool routing** | Gateway dispatches to nodes | Captain routes by capability |
| **Security** | Pairing codes + model-level | 6-layer (secret + IP + capability + shield + audit + LAN-only) |
| **Offline capable?** | No (needs cloud LLM) | Yes (LLM is local) |
| **Cost** | API fees ($0.01-0.10/msg) | $0 (local LLM) |
| **Max context** | 128K+ (cloud) | 32K (local, expandable) |
| **Discovery** | Manual pairing | Auto-discovery on LAN |

### Sea-Claw Mesh Killer Advantages

1. **Zero data leakage** — nothing leaves the LAN, ever
2. **Zero operating cost** — local LLM, no API fees
3. **2 MB per node** — runs on anything with a CPU
4. **Works without internet** — fully air-gapped operation possible
5. **Capability-based routing** — right tool on the right machine
6. **6-layer security** — strictest of any distributed agent system

---

## 10. Implementation Roadmap

```
Phase 1: Mesh Foundation (1-2 weeks)
  ├── sea_mesh_server.c — minimal HTTP listener (POSIX sockets)
  ├── sea_mesh.c — register, heartbeat, discover
  ├── sea_mesh_auth.c — HMAC token generation/validation
  ├── --mode captain / --mode crew flags in main.c
  └── config.json mesh section parsing

Phase 2: Task Routing (1 week)
  ├── Captain: accept /mesh/task, route to LLM
  ├── Captain: route tool calls to nodes by capability
  ├── Crew: accept /node/exec, run tool, return result
  └── Modify sea_agent.c to use mesh routing

Phase 3: Resilience (1 week)
  ├── Heartbeat timeout → mark node offline
  ├── Tool call retry on node failure
  ├── Fallback: Captain executes locally if no node available
  └── Task queue persistence (SQLite)

Phase 4: User Experience (1 week)
  ├── /mesh status — show all nodes in Telegram
  ├── /mesh nodes — list capabilities
  ├── /mesh route <tool> — show which node would handle it
  ├── Auto-discovery via mDNS/UDP broadcast
  └── WebChat UI on Captain (optional)
```

---

## 11. Minimal config.json for Getting Started

### On the M4 Mac (Captain):

```bash
# Install Ollama
curl -fsSL https://ollama.ai/install.sh | sh
ollama pull qwen2.5:72b

# Run Sea-Claw as Captain
./sea_claw --mode captain --config captain.json
```

### On any other machine (Crew):

```bash
# Just copy the 2 MB binary + config
scp user@captain:~/seaclaw/sea_claw .

# Run as Crew
./sea_claw --mode crew --config crew.json
```

That's it. **2 MB binary, one config file, zero dependencies.** The crew node registers with the Captain automatically, advertises its tools, and starts accepting work.

---

## 12. The Pitch

> **"A 2 MB binary that turns any machine into an AI-powered node.
> One central brain (your M4 Mac), unlimited hands (every device on your network).
> Zero cloud. Zero cost. Zero data leakage.
> The sovereign mesh — your AI, your network, your rules."**
