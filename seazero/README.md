# SeaZero — Agent Zero Integration for SeaClaw

**Power through separation. Security through discipline.**

SeaZero brings [Agent Zero](https://github.com/frdel/agent-zero)'s autonomous Python agents
into SeaClaw's sovereign C11 orchestration layer. SeaClaw stays small (203KB).
Agent Zero stays powerful (Kali Linux, FAISS, dynamic skills). They communicate
through a thin bridge — never merged, always separated.

## Architecture

```
┌──────────────────────────────────────────────────────────────┐
│  SeaClaw (C11, 203KB)              Agent Zero (Python/Docker)│
│  ┌────────────────────┐            ┌────────────────────────┐│
│  │ Substrate (Arena)   │            │ Python Runtime         ││
│  │ Senses (JSON, HTTP) │◄──bridge──►│ FAISS Vector Memory    ││
│  │ Shield (Grammar)    │            │ Dynamic Skills         ││
│  │ Brain (6 LLM provs) │            │ Kali Linux Tools       ││
│  │ Hands (57 Tools)    │            │ Autonomous Execution   ││
│  │ Telegram / TUI      │            │ Code Interpreter       ││
│  └────────────────────┘            └────────────────────────┘│
│           │                                │                  │
│           └────────────┬───────────────────┘                  │
│                        │                                      │
│               Shared LLM (RunPod / Z.AI / Ollama / Cloud)    │
└──────────────────────────────────────────────────────────────┘
```

## Philosophy

- **SeaClaw is the Captain.** It orchestrates, validates, audits. Every byte
  through Grammar Shield. Every tool call logged. Every agent on a leash.
- **Agent Zero is the Crew.** Powerful, autonomous, dangerous. Runs in a
  locked-down Docker container with capability dropping and resource limits.
- **The bridge is thin.** HTTP JSON-RPC over localhost. SeaClaw calls Agent Zero
  like any other tool. Agent Zero can request SeaClaw's 57 tools back.
- **Opt-in, not required.** If you don't enable SeaZero, you get exactly the
  same 203KB SeaClaw you always had. Zero bloat.

## Quick Start

```bash
# Prerequisites: SeaClaw built + Docker installed

# 1. Pull Agent Zero image
docker pull seazero/agent-zero:latest

# 2. Start Agent Zero container
cd seazero && docker compose up -d

# 3. Use from SeaClaw (Telegram or TUI)
sea_claw --config ~/.config/seaclaw/config.json

# Then in chat:
#   "Use Agent Zero to scan the network for open ports"
#   "Ask Agent Zero to write a Python script that..."
#   "Delegate this research task to Agent Zero"
```

## Directory Structure

```
seazero/
├── README.md               # This file
├── docker-compose.yml      # Agent Zero container config
├── bridge/
│   ├── sea_zero.h          # C header — bridge API
│   └── sea_zero.c          # C source — Agent Zero IPC
├── scripts/
│   ├── spawn-agent.sh      # Spawn/manage Agent Zero containers
│   └── setup.sh            # First-time SeaZero setup
└── config/
    └── agent-zero.env.example  # Agent Zero environment template
```

## How It Works

1. **User sends a message** via Telegram or TUI
2. **SeaClaw's Brain** decides if the task needs Agent Zero
3. **Bridge** sends the task to Agent Zero's HTTP API (localhost:8080)
4. **Agent Zero** executes autonomously in its Docker container
5. **Result** comes back through the bridge
6. **Grammar Shield** validates the output before showing it to the user
7. **Audit trail** logs the entire interaction in SQLite

## Security Model

| Layer | Protection |
|-------|-----------|
| **Container Isolation** | Docker with `--cap-drop ALL`, `--security-opt no-new-privileges` |
| **Resource Limits** | 2GB RAM, 1 CPU, 100 PIDs max |
| **Network Isolation** | Agent Zero on internal Docker network only |
| **Output Validation** | All Agent Zero responses pass through Grammar Shield |
| **Audit Trail** | Every delegation logged with timestamps in SQLite |
| **No Docker Socket** | Agent Zero cannot spawn containers or escape |

## Configuration

Add to your `config.json`:

```json
{
    "seazero_enabled": true,
    "seazero_agent_url": "http://localhost:8080",
    "seazero_timeout_sec": 120,
    "seazero_max_agents": 3
}
```

Or via environment variables:

```bash
export SEAZERO_ENABLED=1
export SEAZERO_AGENT_URL=http://localhost:8080
```

## Roadmap

- [x] Phase 0: Repository structure, documentation
- [ ] Phase 1: HTTP bridge (SeaClaw ↔ Agent Zero)
- [ ] Phase 2: Shared LLM routing through SeaClaw
- [ ] Phase 3: Unified installer (optional Agent Zero step)
- [ ] Phase 4: Security hardening (seccomp, audit)
- [ ] Phase 5: Electron UI (optional desktop app)
- [ ] Phase 6: RunPod serverless LLM integration

## License

Same as SeaClaw — MIT License.
