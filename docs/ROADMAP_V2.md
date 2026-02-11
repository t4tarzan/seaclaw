# Sea-Claw v2.0 Roadmap

> **Goal**: Close every functionality gap with PicoClaw (Go) and OpenClaw (TypeScript), while keeping the C11 advantages that make Sea-Claw unbeatable on performance, security, and auditability.

## Why C Wins

| Advantage | Sea-Claw (C11) | PicoClaw (Go) | OpenClaw (TS) |
|-----------|----------------|---------------|---------------|
| Binary size | **82 KB** | ~15 MB | 200+ MB |
| Startup | **<1 ms** | ~1 s | >5 s |
| Memory | **17 MB** (arena) | <10 MB (GC) | >1 GB |
| Dependencies | **2** | 15 Go modules | 2000+ npm |
| Memory leaks | **Impossible** (arena) | Possible (GC) | Common |
| Input validation | **Byte-level <1μs** | None | Regex |
| Attack surface | **74 files** | 38 files | Thousands |
| Tool loading | **Static (compile)** | Dynamic (runtime) | Dynamic (eval) |

## v1.0 Completed (Phases 0–8)

- ✅ 9,159 lines of C11, 74 source files
- ✅ 50 tools compiled in (static registry)
- ✅ 61 tests, 5 suites, all passing
- ✅ Grammar Shield (10 grammars, <1μs)
- ✅ Arena allocator (zero leaks)
- ✅ Multi-provider LLM (5 providers + fallback chain)
- ✅ Telegram bot + Interactive TUI
- ✅ SQLite DB (WAL mode)
- ✅ A2A protocol (agent-to-agent delegation)
- ✅ Interactive installer wizard
- ✅ Docs site at seaclaw.virtualgpt.cloud

---

## v2.0 Phases

### Phase 10: Message Bus & Channel Architecture
**Timeline**: Week 1–2 | **Tasks**: 10 | **Priority**: P0 (foundation)

The message bus is the architectural keystone. Every feature below depends on it.

| # | Task | Status |
|---|------|--------|
| 1 | Design `sea_bus.h` — message bus interface (inbound/outbound queues) | ⬜ |
| 2 | Implement `sea_bus.c` — thread-safe pub/sub with arena allocation | ⬜ |
| 3 | Design `sea_channel.h` — abstract channel interface (init/poll/send/stop) | ⬜ |
| 4 | Implement `sea_channel.c` — channel registry and lifecycle management | ⬜ |
| 5 | Refactor `sea_telegram.c` to implement channel interface | ⬜ |
| 6 | Decouple agent loop from Telegram — agent reads from bus only | ⬜ |
| 7 | Add bus message types: MSG_USER, MSG_SYSTEM, MSG_TOOL_RESULT, MSG_OUTBOUND | ⬜ |
| 8 | Write `test_bus.c` — concurrent publish/consume tests | ⬜ |
| 9 | Write `test_channel.c` — channel lifecycle tests | ⬜ |
| 10 | Update `main.c` to use bus-based architecture | ⬜ |

**Architecture**:
```
Channel (Telegram/Discord/...) → Bus (inbound queue) → Agent Loop → Bus (outbound queue) → Channel
```

---

### Phase 11: Multi-Channel Support
**Timeline**: Week 3–5 | **Tasks**: 11 | **Priority**: P0

Closes the biggest gap with PicoClaw (7 channels) and OpenClaw (15+ channels).

| # | Task | Status |
|---|------|--------|
| 1 | Implement `channel_discord.c` — Discord bot via WebSocket gateway API | ⬜ |
| 2 | Implement `channel_whatsapp.c` — WhatsApp via WebSocket bridge | ⬜ |
| 3 | Implement `channel_slack.c` — Slack bot via RTM/Events API | ⬜ |
| 4 | Implement `channel_webchat.c` — HTTP/WebSocket webchat endpoint | ⬜ |
| 5 | Implement `channel_signal.c` — Signal via signal-cli JSON-RPC | ⬜ |
| 6 | Per-channel `allowFrom` security (restrict by user ID per channel) | ⬜ |
| 7 | Channel-specific message formatting (Markdown/HTML per platform) | ⬜ |
| 8 | Write `test_discord.c` — Discord message parsing tests | ⬜ |
| 9 | Write `test_whatsapp.c` — WhatsApp bridge protocol tests | ⬜ |
| 10 | Update `config.json` schema for multi-channel | ⬜ |
| 11 | Update installer wizard with Discord/WhatsApp/Slack token prompts | ⬜ |

---

### Phase 12: Session Management & Memory
**Timeline**: Week 5–7 | **Tasks**: 12 | **Priority**: P1

Per-channel session isolation, automatic summarization, and persistent memory.

| # | Task | Status |
|---|------|--------|
| 1 | Design `sea_session.h` — per-channel per-chat session isolation | ⬜ |
| 2 | Implement `sea_session.c` — session CRUD with SQLite backend | ⬜ |
| 3 | Automatic conversation summarization (LLM-driven) | ⬜ |
| 4 | Multi-part summarization for long histories (split/summarize/merge) | ⬜ |
| 5 | Session pruning — keep last N messages, summarize rest | ⬜ |
| 6 | Design `sea_memory.h` — long-term memory interface | ⬜ |
| 7 | Implement `sea_memory.c` — MEMORY.md + daily notes (YYYYMM/YYYYMMDD.md) | ⬜ |
| 8 | Bootstrap files: AGENTS.md, SOUL.md, USER.md, IDENTITY.md | ⬜ |
| 9 | Inject memory context into system prompt automatically | ⬜ |
| 10 | Agent can read/write memory via file tools | ⬜ |
| 11 | Write `test_session.c` — session isolation and summarization tests | ⬜ |
| 12 | Write `test_memory.c` — memory persistence and retrieval tests | ⬜ |

---

### Phase 13: Skills & Plugin System
**Timeline**: Week 7–9 | **Tasks**: 8 | **Priority**: P1

Markdown-based skills that extend agent behavior without recompiling.

| # | Task | Status |
|---|------|--------|
| 1 | Design `sea_skill.h` — skill loader interface (SKILL.md markdown-based) | ⬜ |
| 2 | Implement `sea_skill.c` — load skills from workspace/skills/ directory | ⬜ |
| 3 | Skill installer — download skills from GitHub repos at runtime | ⬜ |
| 4 | Built-in skills: weather, github, summarize, tmux | ⬜ |
| 5 | CLI commands: `sea_claw skills install/list/remove/search` | ⬜ |
| 6 | Inject skill summaries into system prompt | ⬜ |
| 7 | Skill discovery — search available skills from registry | ⬜ |
| 8 | Write `test_skill.c` — skill loading and context injection tests | ⬜ |

---

### Phase 14: Cron Scheduler & Background Tasks
**Timeline**: Week 9–11 | **Tasks**: 12 | **Priority**: P1

Persistent scheduled tasks + background sub-agent spawning.

| # | Task | Status |
|---|------|--------|
| 1 | Design `sea_cron.h` — persistent cron scheduler interface | ⬜ |
| 2 | Implement `sea_cron.c` — job storage (SQLite), cron expressions, intervals | ⬜ |
| 3 | Cron job types: one-time, recurring (every N sec), cron expression | ⬜ |
| 4 | Jobs survive restart — stored in DB, loaded on startup | ⬜ |
| 5 | Agent can create cron jobs via natural language | ⬜ |
| 6 | Deliver cron results back to originating channel/chat | ⬜ |
| 7 | CLI: `sea_claw cron list/add/remove/enable/disable` | ⬜ |
| 8 | Design `sea_subagent.h` — background sub-agent spawning | ⬜ |
| 9 | Implement `sea_subagent.c` — spawn LLM task in background thread | ⬜ |
| 10 | Sub-agent results routed back to origin channel via bus | ⬜ |
| 11 | Write `test_cron.c` — scheduler timing and persistence tests | ⬜ |
| 12 | Write `test_subagent.c` — spawn and result routing tests | ⬜ |

---

### Phase 15: Voice & Media Pipeline
**Timeline**: Week 11–12 | **Tasks**: 8 | **Priority**: P2

Voice transcription and media handling across channels.

| # | Task | Status |
|---|------|--------|
| 1 | Design `sea_voice.h` — voice transcription interface (Whisper API) | ⬜ |
| 2 | Implement `sea_voice.c` — Groq Whisper transcription via HTTP | ⬜ |
| 3 | Attach transcriber to Telegram channel (voice → text) | ⬜ |
| 4 | Attach transcriber to Discord channel (voice → text) | ⬜ |
| 5 | Media pipeline: image/audio/video download, temp file lifecycle | ⬜ |
| 6 | Image support: receive images in Telegram, pass URL to agent context | ⬜ |
| 7 | File upload support: receive documents, store in workspace | ⬜ |
| 8 | Write `test_voice.c` — transcription API mock tests | ⬜ |

---

### Phase 16: Gateway Mode & Concurrent Channels
**Timeline**: Week 12–13 | **Tasks**: 9 | **Priority**: P1

Run all channels simultaneously with a single `sea_claw gateway` command.

| # | Task | Status |
|---|------|--------|
| 1 | Implement gateway mode — run all enabled channels simultaneously | ⬜ |
| 2 | Thread pool for channel polling (one thread per channel) | ⬜ |
| 3 | Graceful shutdown — signal handler stops all channels cleanly | ⬜ |
| 4 | Heartbeat service — periodic health check of all channels | ⬜ |
| 5 | TUI + channels concurrent mode (TUI foreground, channels background) | ⬜ |
| 6 | CLI: `sea_claw gateway` — start all channels + cron + heartbeat | ⬜ |
| 7 | CLI: `sea_claw status` — show running channels, tools, sessions | ⬜ |
| 8 | Channel hot-reload — re-read config without full restart | ⬜ |
| 9 | Write `test_gateway.c` — multi-channel startup/shutdown tests | ⬜ |

---

### Phase 17: Advanced Agent Features
**Timeline**: Week 13–16 | **Tasks**: 12 | **Priority**: P1

New tools, streaming, usage tracking, and CLI commands.

| # | Task | Status |
|---|------|--------|
| 1 | Implement `tool_edit_file.c` — surgical find-and-replace within files | ⬜ |
| 2 | Implement `tool_web_search.c` — Brave Search API integration | ⬜ |
| 3 | Implement `tool_spawn.c` — spawn sub-agent from natural language | ⬜ |
| 4 | Implement `tool_message.c` — send message to any channel/chat | ⬜ |
| 5 | Implement `tool_cron_manage.c` — create/list/remove cron jobs | ⬜ |
| 6 | Dynamic tool descriptions in system prompt (auto-generated) | ⬜ |
| 7 | Multi-provider failover v2 — automatic retry with exponential backoff | ⬜ |
| 8 | Streaming responses — send partial responses as they arrive | ⬜ |
| 9 | Usage tracking — token count per session, per provider, per day | ⬜ |
| 10 | Onboard wizard: `sea_claw onboard` — interactive first-run setup | ⬜ |
| 11 | Doctor command: `sea_claw doctor` — diagnose config/channels/providers | ⬜ |
| 12 | A2A v2 — improved agent-to-agent with service discovery | ⬜ |

---

### Phase 18: Polish, Benchmarks & Release
**Timeline**: Week 16–18 | **Tasks**: 12 | **Priority**: P0

Final testing, benchmarks, documentation, and v2.0.0 release.

| # | Task | Status |
|---|------|--------|
| 1 | Performance benchmarks: startup, memory, tool execution, LLM latency | ⬜ |
| 2 | Comparison benchmark: Sea-Claw vs PicoClaw vs OpenClaw | ⬜ |
| 3 | Update docs site with v2 features, new comparison table | ⬜ |
| 4 | Update README.md with v2 stats, new channels, skills system | ⬜ |
| 5 | Update install.sh wizard with all new channels and features | ⬜ |
| 6 | Write CHANGELOG.md for v2.0.0 | ⬜ |
| 7 | Write MIGRATION.md — v1 to v2 upgrade guide | ⬜ |
| 8 | Security audit — all new channels, bus, cron reviewed | ⬜ |
| 9 | Full test suite pass — target 100+ tests across all modules | ⬜ |
| 10 | Tag v2.0.0 release on GitHub | ⬜ |
| 11 | Update docs site at seaclaw.virtualgpt.cloud | ⬜ |
| 12 | Announce v2 on GitHub, Discord, Telegram | ⬜ |

---

## v2.0 Summary

| Metric | v1.0 (current) | v2.0 (target) |
|--------|----------------|---------------|
| Channels | 1 (Telegram) | **6+** (Telegram, Discord, WhatsApp, Slack, Signal, WebChat) |
| Tools | 50 | **55+** (edit_file, web_search, spawn, message, cron_manage) |
| Tests | 61 | **100+** |
| Sessions | Global (last 20 msgs) | **Per-channel, per-chat, auto-summarized** |
| Memory | SQLite KV | **Structured files + daily notes + personality** |
| Skills | None (compiled only) | **Markdown-based, installable from GitHub** |
| Scheduler | cron_parse (parse only) | **Full cron service with persistent jobs** |
| Sub-agents | A2A (remote HTTP) | **Local spawn + remote A2A** |
| Voice | None | **Whisper transcription (Groq)** |
| Gateway | TUI or Telegram | **All channels simultaneously** |
| CLI | sea_claw [--telegram] | **sea_claw gateway/agent/onboard/doctor/cron/skills/status** |

### Timeline: 18 weeks | 94 new tasks | 9 phases

**The C advantage is permanent**: every feature PicoClaw or OpenClaw adds in Go/TypeScript, we match in C with 100x less memory, 1000x faster startup, zero memory leaks, and byte-level security. They can never close *that* gap.
