# Sea-Claw v1.0 — Project Structure

> Merged from original brainstorming session + Kimi Agent architecture evolution.
> Preserves: Atomic Workshops philosophy, hardware separation, pure C core.
> Adds: Agent loop, TUI, Telegram, Docker sidecars, security modules.
>
> **Core principle**: See [PERFORMANCE_PHILOSOPHY.md](PERFORMANCE_PHILOSOPHY.md) —
> Thinking is slow (CPU/mmap), Reacting is instant (event loop), Doing is C-speed (function pointers).

---

## Decisions Log

| Decision | Original | Kimi | **Final** | Reason |
|----------|----------|------|-----------|--------|
| Build system | Makefile | CMake | **Makefile + cmake option** | Makefile for simplicity; CMake as optional for IDE users |
| C standard | C99 | C11 | **C11** | `_Static_assert`, `_Alignof`, anonymous structs needed |
| Arch math files | Separate `.h` per arch | Merged into tensor.c | **Separate files** | Original is cleaner for students and portability |
| Header style | Header-only `.h` | `include/` + `src/` split | **Split** | Better compile times, cleaner API surface |
| Config format | JSON | YAML | **JSON** | We already have `sea_json.h`; no YAML parser needed |
| Output folder | `/dist` | Docker only | **Both** | `/dist` for bare-metal, Docker for containerized |

---

## File Tree

```
/sea_claw
│
├── README.md                        # Manifesto + quick-start (make all)
├── LICENSE                          # MIT
├── VERSION                          # Semantic version tracking
├── Makefile                         # Primary build — arch detection, -mavx2 / -mcpu=apple-m1
├── CMakeLists.txt                   # Optional CMake for IDE integration
│
├── include/                         # Public headers (API surface)
│   └── seaclaw/
│       ├── sea_types.h              # Fixed types, SeaIntent, SeaString, bit-level consistency
│       ├── sea_arena.h              # Memory Notebook — SeaArena, alloc/reset
│       ├── sea_wire.h               # Mail Slot — raw sockets, stdin fallback
│       ├── sea_json.h               # Shape Sorter — zero-copy JSON parser
│       ├── sea_model.h              # Reference Manual — mmap model loading
│       ├── sea_inference.h          # Thinker — sea_ai_think() high-level API
│       ├── sea_grammar.h            # Allowed charsets per tool (GRAMMAR_SAFE_TEXT, etc.)
│       ├── sea_shield.h             # Grammar Filter — sea_shield_validate()
│       ├── sea_sandbox.h            # Linux namespaces + seccomp sandboxing
│       ├── sea_crypto.h             # AES-256-GCM for config encryption
│       ├── sea_tools.h              # Tool Registry — SeaTool struct, lookup table
│       ├── sea_agent.h              # Agent state machine — think/act/reflect loop
│       ├── sea_router.h             # Multi-model routing (local/Kimi/OpenAI)
│       ├── sea_tui.h                # Terminal UI — ncurses dashboard + status line
│       ├── sea_telegram.h           # Telegram bot interface
│       ├── sea_docker.h             # Sidecar orchestration (Jail Warden)
│       ├── sea_drive.h              # Shared drive output (Google Drive/S3)
│       ├── sea_config.h             # Config loader (JSON, encrypted)
│       └── sea_log.h                # Structured logging
│
├── src/                             # Implementation (Pure C11)
│   │
│   ├── main.c                       # Nervous System — main(), event loop, orchestration
│   │
│   ├── core/                        # === THE SUBSTRATE (hardware-agnostic) ===
│   │   ├── sea_arena.c              # Arena allocator implementation
│   │   ├── sea_types.c              # Type utilities
│   │   ├── sea_log.c                # Structured logging
│   │   └── sea_config.c             # JSON config loader + AES decryption
│   │
│   ├── brain/                       # === THE INTELLIGENCE ===
│   │   ├── sea_model.c              # mmap model loading, tensor index
│   │   ├── sea_inference.c          # sea_ai_think() — high-level inference
│   │   ├── sea_grammar.c            # Grammar constraint engine (BNF → DFA)
│   │   ├── sea_sampler.c            # Token sampling (temperature, top-p)
│   │   ├── sea_math_x86.h           # x86: AVX/AVX2 intrinsics
│   │   ├── sea_math_arm.h           # ARM: NEON intrinsics (Apple Silicon, RPi)
│   │   └── sea_math_generic.h       # Fallback: standard C math (RISC-V, etc.)
│   │
│   ├── senses/                      # === I/O & PARSING ===
│   │   ├── sea_wire.c               # Raw sockets (TCP/UDP), stdin fallback
│   │   ├── sea_json.c               # Zero-copy JSON parser
│   │   └── sea_http.c               # Minimal HTTP client for external APIs
│   │
│   ├── bridge/                      # === THE AIR-GAP (Wild → Bridge → Vault) ===
│   │   └── sea_bridge.c             # Intent translator: WebSocket ↔ Unix Domain Socket
│   │                                #   The Wild (Browser/React) sends intents via WebSocket
│   │                                #   The Bridge validates and translates to engine commands
│   │                                #   The Vault (engine) receives via sea_claw.sock
│   │
│   ├── shield/                      # === SECURITY & VALIDATION ===
│   │   ├── sea_shield.c             # Byte-level charset validation
│   │   ├── sea_sandbox.c            # Linux namespaces + seccomp BPF
│   │   └── sea_crypto.c             # AES-256-GCM, key derivation
│   │
│   ├── hands/                       # === CAPABILITIES ===
│   │   ├── sea_tools.c              # Registry + lookup (perfect hash, O(1))
│   │   └── impl/                    # Individual tool implementations
│   │       ├── tool_echo.c          # Echo (testing)
│   │       ├── tool_system_status.c # Memory usage, uptime
│   │       ├── tool_ledger_write.c  # Append to local log file
│   │       ├── tool_db.c            # Database operations
│   │       ├── tool_email.c         # Email sending (SMTP)
│   │       └── tool_invoice.c       # Invoice PDF generation
│   │
│   ├── agent/                       # === AGENT ORCHESTRATION ===
│   │   ├── sea_agent.c              # State machine (idle→plan→execute→reflect)
│   │   ├── sea_router.c             # Model selection (local/cloud, cost/speed)
│   │   └── sea_context.c            # Conversation history management
│   │
│   ├── tui/                         # === TERMINAL UI (Mirror — reflects, never calculates) ===
│   │   ├── sea_tui.c                # ncurses init, layout, render loop
│   │   ├── sea_dashboard.c          # Status panels (tools, system, arena)
│   │   ├── sea_input.c              # Input handling, command parsing
│   │   └── sea_statusline.c         # Live ticker (T+0ms [SENSES] Parsing...)
│   │
│   ├── telegram/                    # === TELEGRAM BOT ===
│   │   └── sea_telegram.c           # Bot polling, command dispatch, message send
│   │
│   ├── docker/                      # === SIDECAR ORCHESTRATION ===
│   │   └── sea_docker.c             # Container create/run/destroy via Unix socket
│   │
│   └── drive/                       # === OUTPUT DELIVERY ===
│       └── sea_drive.c              # Upload to Google Drive/S3, link generation
│
├── sidecars/                        # Docker sidecar images (ephemeral workers)
│   ├── pdf-parser/
│   │   ├── Dockerfile
│   │   └── parse_pdf.py
│   ├── node-builder/
│   │   ├── Dockerfile
│   │   └── build.sh
│   └── docx-tools/
│       ├── Dockerfile
│       └── generate_docx.py
│
├── scripts/                         # Utilities (run once, then discard)
│   ├── convert_model.py             # Convert .gguf/.bin → .clawmodel (mmap format)
│   ├── generate_keys.sh             # Generate P2P Mesh cryptographic keypairs
│   ├── deploy.sh                    # Production deployment (Hetzner/Ubuntu)
│   └── backup.sh                    # Database + config backup
│
├── tests/                           # Verification suite
│   ├── test_arena.c                 # Stress test — prove zero leaks
│   ├── test_shield.c                # Fuzz test — random garbage at Shield
│   ├── test_json.c                  # Zero-copy parser correctness
│   ├── test_grammar.c               # Grammar constraint validation
│   └── test_tools.c                 # Tool registry + execution
│
├── config/                          # Configuration templates
│   └── config.example.json          # Example config (JSON, not YAML)
│
├── docs/                            # Documentation
│   ├── ARCHITECTURE.md              # Master architecture document
│   ├── SECURITY.md                  # Threat model + mitigations
│   ├── TERMINAL_UX_REFERENCE.md     # TUI design (status line, layout, colors)
│   ├── API.md                       # Public API reference
│   └── STUDENT_GUIDE.md             # Learning path (7 milestones)
│
├── dist/                            # Build output (bare-metal deployment)
│   ├── sea_claw                     # Compiled binary (< 5MB)
│   └── config.json                  # User's config (chmod 600)
│
├── docker-compose.yml               # Full stack: engine + postgres + redis + nginx
├── Dockerfile                       # Multi-stage C-Engine build
│
└── vendor/                          # Vendored dependencies (zero external)
    └── llama.cpp/                   # Inference backend (pinned version)
```

---

## Module Ownership Guide

For student teams working in parallel:

| Module | Owner Focus | Can Break Other Modules? |
|--------|-------------|--------------------------|
| `/core` | Memory, types, logging | ⚠️ Yes — everyone depends on this |
| `/brain` | Model loading, math, inference | No — isolated |
| `/brain/sea_math_*.h` | Per-architecture optimization | No — only affects speed |
| `/senses` | Network I/O, JSON parsing | No — isolated |
| `/shield` | Security validation | No — isolated |
| `/hands/impl/*` | Individual tools | No — one file per tool |
| `/agent` | State machine, routing | Depends on brain + hands |
| `/tui` | Terminal UI, status line | Depends on agent |
| `/telegram` | Bot interface | Depends on agent |
| `/docker` | Sidecar management | Isolated (talks to Docker daemon) |
| `/drive` | File upload | Isolated (talks to cloud APIs) |

---

## Build Commands

```bash
# Detect arch and build everything
make all

# Build for specific arch
make ARCH=x86     # Uses AVX2
make ARCH=arm     # Uses NEON
make ARCH=generic # Pure C fallback

# Run tests
make test

# Build Docker images
make docker

# Clean
make clean

# Install to /usr/local/bin
make install
```

---

## What's Deferred (v2.0 — Post-Launch)

These modules from the Kimi Agent docs are **not in v1.0**:

| Module | Reason for Deferral |
|--------|---------------------|
| `/logician` — Rule enforcement | Needs real usage data to design rules |
| `/guardian` — Self-healing watchdog | Requires stable v1.0 first |
| `/a2a` — Agent-to-agent protocols | Future distributed mesh feature |
| `/dna` — Creative DNA user profile | Enhancement, not core |
| `portal/` — OAuth web portal | Web frontend, separate project |

---

*Merged: Original brainstorm (Feb 2026) + Kimi Agent architecture evolution*
*Status: Ready for implementation*
