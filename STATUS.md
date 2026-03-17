# Sea-Claw Project Status

> **Last updated:** 2026-03-17 — update this file on each release.

## Current Version: v2.0.0

Released: February 2026 · [CHANGELOG](CHANGELOG.md) · [Architecture](docs/ARCHITECTURE.md)

## Build & Test

| Command | Result |
|---------|--------|
| `make all` | ✅ Passing |
| `make test` | ✅ 116 tests, 13 suites, all passing |
| `make release` | ✅ Static binary (~82 KB) |
| `make health` | ✅ `--health-report` CLI flag available |

## Counts

| Metric | v1.0 | v2.0.0 |
|--------|------|--------|
| Source lines | ~9,159 | ~48,000+ |
| C source files | 74 | 78 |
| Tools (compiled-in) | 50 | 57 |
| Tests | 61 | 116 |
| Test suites | 5 | 13 |
| LLM providers | 5 | 6 |

## Active Branches

| Branch | Status | Notes |
|--------|--------|-------|
| `main` | ✅ Stable | v2.0.0 released here |
| `dev` | Active | General development |
| `feature/seazero` | Unmerged | SeaZero C+Python bridge — audit before merge |
| `oc` | Partially merged | Recent doc-fix PRs (#5, #7) came from here |

## Module Status

| Module | Source File | Tests | Status |
|--------|-------------|-------|--------|
| Arena allocator | `src/core/sea_arena.c` | `test_arena.c` | ✅ |
| Zero-copy JSON | `src/senses/sea_json.c` | `test_json.c` | ✅ |
| Grammar Shield | `src/shield/sea_shield.c` | `test_shield.c` | ✅ |
| SQLite DB | `src/core/sea_db.c` | `test_db.c` | ✅ |
| Config loader | `src/core/sea_config.c` | `test_config.c` | ✅ |
| Message bus | `src/bus/sea_bus.c` | `test_bus.c` | ✅ |
| Channel abstraction | `src/channels/sea_channel.c` | — | ✅ |
| Telegram channel | `src/channels/channel_telegram.c` | — | ✅ |
| Session management | `src/session/sea_session.c` | `test_session.c` | ✅ |
| Long-term memory | `src/memory/sea_memory.c` | `test_memory.c` | ✅ |
| Cron scheduler | `src/cron/sea_cron.c` | `test_cron.c` | ✅ |
| Skills system | `src/skills/sea_skill.c` | `test_skill.c` | ✅ |
| PII firewall | `src/pii/sea_pii.c` | `test_pii.c` | ✅ |
| Recall engine | `src/recall/sea_recall.c` | `test_recall.c` | ✅ |
| Usage tracking | `src/usage/sea_usage.c` | — | ✅ |
| A2A protocol | `src/a2a/sea_a2a.c` | — | ✅ |
| Mesh (Captain/Crew) | `src/mesh/sea_mesh.c` | — | ✅ |
| SeaZero bridge | `seazero/` | `test_seazero.c` | ✅ (unmerged branch) |

## Known Open Items

### Code
- `src/mesh/sea_mesh.c` — capability enumeration was a TODO stub; fixed in PR (branch `fix/issue-11`)

### Features (deferred to v3.0)
- **Phase 11** — Multi-channel: Discord, WhatsApp, Slack, Signal, WebChat not implemented
- **Phase 14** (partial) — Dedicated `sea_subagent` module; basic spawn handled by `tool_spawn.c`
- **Phase 15** — Voice/media pipeline (Whisper transcription) not implemented

### Process
- No CI pipeline — tests are run manually via `make test`
- `feature/seazero` branch not merged — needs integration audit
- `docs/ROADMAP_V2.md` was out of sync with v2.0 reality (fixed in `fix/issue-11`)

## Next Milestone: v3.0

See [CBOT_DATA_TABLE.md](CBOT_DATA_TABLE.md) for the full Phase E task registry.

Priority order (from Peter Plan audit):
1. Add GitHub Actions CI (`.github/workflows/ci.yml`)
2. Implement Discord channel (`channel_discord.c`) — Phase 11 entry point
3. Extension Registry refactor (DJB2 hash O(1) tool lookup)
4. Merge/audit `feature/seazero` branch
5. Voice pipeline via Whisper API (Phase 15)
