# CBOT Data Table — SeaClaw Complete Task & Test Registry

Comprehensive database of every task, subtask, module, tool, test, and planned feature in the SeaClaw project. This is the single source of truth for project status.

**Version:** 3.0.0 | **Files:** 153 tracked | **Tools:** 58 | **Tests:** 14 suites, 165 assertions | **Lines:** ~9,200 C11

---

## Legend

| Symbol | Meaning |
|--------|---------|
| ✅ | Complete — implemented, tested, merged |
| 🔧 | In progress |
| ⬜ | Planned — not yet started |
| 🧪 | Has dedicated test suite |
| 🛡️ | Security-critical — Shield-validated |

---

## 1. Core Substrate (Pillar 1)

| ID | Task | Subtasks | File(s) | Owner | Status | Tests |
|----|------|----------|---------|-------|--------|-------|
| C-001 | Arena allocator | Create, alloc, alloc_zero, reset, destroy, push_string | `sea_arena.h/c` | SeaClaw | ✅ | 🧪 test_arena (8) |
| C-002 | Type system | u8/u16/u32/u64, i8-i64, f32/f64, bool, SeaSlice, SeaError | `sea_types.h` | SeaClaw | ✅ | — |
| C-003 | Logging | sea_log_init, LOG_DEBUG/INFO/WARN/ERROR, file+stderr | `sea_log.h/c` | SeaClaw | ✅ | — |
| C-004 | Configuration | JSON config parse, provider/model/fallback, arena-alloc | `sea_config.h/c` | SeaClaw | ✅ | 🧪 test_config (7) |
| C-005 | Database (v2) | SQLite schema, config/trajectory/tasks/chat_history/recall | `sea_db.h/c` | SeaClaw | ✅ | 🧪 test_db (11) |
| C-006 | Database (v3) | seazero_agents/tasks/llm_usage/audit + 8 indexes | `sea_db.h/c` | SeaClaw | ✅ | 🧪 test_seazero (19) |
| C-007 | JSON parser | sea_json_parse, get_string/int/bool/array, nested objects | `sea_json.h/c` | SeaClaw | ✅ | 🧪 test_json (17) |
| C-008 | HTTP client | sea_http_get, sea_http_post, sea_http_post_json, timeouts | `sea_http.h/c` | SeaClaw | ✅ | — |

---

## 2. Senses (Pillar 2)

| ID | Task | Subtasks | File(s) | Owner | Status | Tests |
|----|------|----------|---------|-------|--------|-------|
| S-001 | Telegram channel | Poll updates, send messages, parse commands, chat_id auth | `sea_telegram.h/c`, `channel_telegram.c` | SeaClaw | ✅ | — |
| S-002 | Channel abstraction | SeaChannel interface, poll/send/stop vtable | `sea_channel.h`, `sea_channel.c` | SeaClaw | ✅ | — |
| S-003 | Message bus | Thread-safe pub/sub, inbound/outbound queues, arena copy | `sea_bus.h/c` | SeaClaw | ✅ | 🧪 test_bus (11) |

---

## 3. Shield (Pillar 3)

| ID | Task | Subtasks | File(s) | Owner | Status | Tests |
|----|------|----------|---------|-------|--------|-------|
| SH-001 | Grammar Shield | Input validation, injection detection, output filtering | `sea_shield.h/c` | SeaClaw 🛡️ | ✅ | 🧪 test_shield (19) |
| SH-002 | PII Firewall | Email, phone, SSN, credit card, IP detection + redaction | `sea_pii.h/c` | SeaClaw 🛡️ | ✅ | 🧪 test_pii (16) |

---

## 4. Brain (Pillar 4)

| ID | Task | Subtasks | File(s) | Owner | Status | Tests |
|----|------|----------|---------|-------|--------|-------|
| B-001 | LLM Agent loop | System prompt, tool calls, conversation history, streaming | `sea_agent.h/c` | SeaClaw | ✅ | — |
| B-002 | Provider: OpenAI | GPT-4o, GPT-4o-mini, chat completions API | `sea_agent.c` | SeaClaw | ✅ | — |
| B-003 | Provider: Anthropic | Claude 3 Haiku/Sonnet/Opus, messages API | `sea_agent.c` | SeaClaw | ✅ | — |
| B-004 | Provider: Gemini | Gemini 2.0 Flash, generateContent API | `sea_agent.c` | SeaClaw | ✅ | — |
| B-005 | Provider: OpenRouter | Any model via OpenRouter proxy | `sea_agent.c` | SeaClaw | ✅ | — |
| B-006 | Provider: Z.AI | Z.AI models | `sea_agent.c` | SeaClaw | ✅ | — |
| B-007 | Provider: Local | Ollama/llama.cpp local models | `sea_agent.c` | SeaClaw | ✅ | — |
| B-008 | Fallback chain | Auto-fallback across providers on error/timeout | `sea_agent.c` | SeaClaw | ✅ | — |
| B-009 | Think levels | None, Low, Medium, High thinking depth | `sea_agent.h` | SeaClaw | ✅ | — |

---

## 5. Hands (Pillar 5) — 58 Tools

### 5.1 System Tools (#1-#6)

| ID | # | Tool Name | Description | File | Owner | Status |
|----|---|-----------|-------------|------|-------|--------|
| T-001 | 1 | echo | Echo text back | `tool_echo.c` | SeaClaw | ✅ |
| T-002 | 2 | system_status | Memory usage and uptime | `tool_system_status.c` | SeaClaw | ✅ |
| T-003 | 3 | file_read | Read a file | `tool_file_read.c` | SeaClaw | ✅ |
| T-004 | 4 | file_write | Write a file | `tool_file_write.c` | SeaClaw | ✅ |
| T-005 | 5 | shell_exec | Run shell command | `tool_shell_exec.c` | SeaClaw 🛡️ | ✅ |
| T-006 | 6 | web_fetch | Fetch a URL | `tool_web_fetch.c` | SeaClaw | ✅ |

### 5.2 Task & Database Tools (#7-#8)

| ID | # | Tool Name | Description | File | Owner | Status |
|----|---|-----------|-------------|------|-------|--------|
| T-007 | 7 | task_manage | CRUD tasks | `tool_task_manage.c` | SeaClaw | ✅ |
| T-008 | 8 | db_query | Read-only SQL queries | `tool_db_query.c` | SeaClaw | ✅ |

### 5.3 Search & Web Tools (#9, #37, #43-#45, #47, #54)

| ID | # | Tool Name | Description | File | Owner | Status |
|----|---|-----------|-------------|------|-------|--------|
| T-009 | 9 | exa_search | Exa web search | `tool_exa_search.c` | SeaClaw | ✅ |
| T-037 | 37 | http_request | HTTP GET/POST/HEAD | `tool_http_request.c` | SeaClaw | ✅ |
| T-043 | 43 | ip_info | IP geolocation | `tool_ip_info.c` | SeaClaw | ✅ |
| T-044 | 44 | whois_lookup | Domain WHOIS | `tool_whois_lookup.c` | SeaClaw | ✅ |
| T-045 | 45 | ssl_check | SSL certificate info | `tool_ssl_check.c` | SeaClaw | ✅ |
| T-047 | 47 | weather | Current weather | `tool_weather.c` | SeaClaw | ✅ |
| T-054 | 54 | web_search | Brave web search | `tool_web_search.c` | SeaClaw | ✅ |

### 5.4 Text Processing Tools (#10-#11, #25-#31, #38)

| ID | # | Tool Name | Description | File | Owner | Status |
|----|---|-----------|-------------|------|-------|--------|
| T-010 | 10 | text_summarize | Text statistics | `tool_text_summarize.c` | SeaClaw | ✅ |
| T-011 | 11 | text_transform | upper/lower/reverse/base64 | `tool_text_transform.c` | SeaClaw | ✅ |
| T-025 | 25 | regex_match | Regex matching | `tool_regex_match.c` | SeaClaw | ✅ |
| T-026 | 26 | csv_parse | Parse CSV data | `tool_csv_parse.c` | SeaClaw | ✅ |
| T-027 | 27 | diff_text | Compare two texts | `tool_diff_text.c` | SeaClaw | ✅ |
| T-028 | 28 | grep_text | Search text/file | `tool_grep_text.c` | SeaClaw | ✅ |
| T-029 | 29 | wc | Word/line/char count | `tool_wc.c` | SeaClaw | ✅ |
| T-030 | 30 | head_tail | First/last N lines | `tool_head_tail.c` | SeaClaw | ✅ |
| T-031 | 31 | sort_text | Sort lines | `tool_sort_text.c` | SeaClaw | ✅ |
| T-038 | 38 | string_replace | Find and replace | `tool_string_replace.c` | SeaClaw | ✅ |

### 5.5 Data & Format Tools (#12-#13, #23-#24, #36, #46, #48)

| ID | # | Tool Name | Description | File | Owner | Status |
|----|---|-----------|-------------|------|-------|--------|
| T-012 | 12 | json_format | Pretty-print/validate JSON | `tool_json_format.c` | SeaClaw | ✅ |
| T-013 | 13 | hash_compute | CRC32/DJB2/FNV1a hashing | `tool_hash_compute.c` | SeaClaw | ✅ |
| T-023 | 23 | url_parse | Parse URL components | `tool_url_parse.c` | SeaClaw | ✅ |
| T-024 | 24 | encode_decode | URL/HTML encode/decode | `tool_encode_decode.c` | SeaClaw | ✅ |
| T-036 | 36 | json_query | Query JSON by path | `tool_json_query.c` | SeaClaw | ✅ |
| T-046 | 46 | json_to_csv | JSON array to CSV | `tool_json_to_csv.c` | SeaClaw | ✅ |
| T-048 | 48 | unit_convert | Unit conversion | `tool_unit_convert.c` | SeaClaw | ✅ |

### 5.6 System & File Tools (#14-#18, #34-#35, #40-#42, #50-#51)

| ID | # | Tool Name | Description | File | Owner | Status |
|----|---|-----------|-------------|------|-------|--------|
| T-014 | 14 | env_get | Get env variable (whitelisted) | `tool_env_get.c` | SeaClaw | ✅ |
| T-015 | 15 | dir_list | List directory | `tool_dir_list.c` | SeaClaw | ✅ |
| T-016 | 16 | file_info | File metadata | `tool_file_info.c` | SeaClaw | ✅ |
| T-017 | 17 | process_list | List processes | `tool_process_list.c` | SeaClaw | ✅ |
| T-018 | 18 | dns_lookup | DNS resolve | `tool_dns_lookup.c` | SeaClaw | ✅ |
| T-034 | 34 | disk_usage | Disk usage stats | `tool_disk_usage.c` | SeaClaw | ✅ |
| T-035 | 35 | syslog_read | Read system logs | `tool_syslog_read.c` | SeaClaw | ✅ |
| T-040 | 40 | checksum_file | File checksum | `tool_checksum_file.c` | SeaClaw | ✅ |
| T-041 | 41 | file_search | Find files by name | `tool_file_search.c` | SeaClaw | ✅ |
| T-042 | 42 | uptime | System uptime/load | `tool_uptime.c` | SeaClaw | ✅ |
| T-050 | 50 | count_lines | Count lines of code | `tool_count_lines.c` | SeaClaw | ✅ |
| T-051 | 51 | edit_file | Edit file (find/replace) | `tool_edit_file.c` | SeaClaw | ✅ |

### 5.7 Utility Tools (#19-#22, #33, #39, #49)

| ID | # | Tool Name | Description | File | Owner | Status |
|----|---|-----------|-------------|------|-------|--------|
| T-019 | 19 | timestamp | Current time | `tool_timestamp.c` | SeaClaw | ✅ |
| T-020 | 20 | math_eval | Evaluate math | `tool_math_eval.c` | SeaClaw | ✅ |
| T-021 | 21 | uuid_gen | Generate UUID v4 | `tool_uuid_gen.c` | SeaClaw | ✅ |
| T-022 | 22 | random_gen | Random values | `tool_random_gen.c` | SeaClaw | ✅ |
| T-033 | 33 | cron_parse | Explain cron expression | `tool_cron_parse.c` | SeaClaw | ✅ |
| T-039 | 39 | calendar | Calendar/date operations | `tool_calendar.c` | SeaClaw | ✅ |
| T-049 | 49 | password_gen | Generate password | `tool_password_gen.c` | SeaClaw | ✅ |

### 5.8 Agent & Memory Tools (#52-#58)

| ID | # | Tool Name | Description | File | Owner | Status |
|----|---|-----------|-------------|------|-------|--------|
| T-052 | 52 | cron_manage | Manage cron jobs | `tool_cron_manage.c` | SeaClaw | ✅ |
| T-053 | 53 | memory_manage | Read/write/append memory | `tool_memory_manage.c` | SeaClaw | ✅ |
| T-055 | 55 | spawn | Spawn sub-agent | `tool_spawn.c` | SeaClaw | ✅ |
| T-056 | 56 | message | Send message to channel | `tool_message.c` | SeaClaw | ✅ |
| T-057 | 57 | recall | Remember/recall/forget facts | `tool_recall.c` | SeaClaw | ✅ |
| T-058 | 58 | agent_zero | Delegate to Agent Zero (Docker) | `sea_tools.c` | Hybrid 🛡️ | ✅ |

### 5.9 Tool Registry

| ID | Task | Subtasks | File(s) | Owner | Status | Tests |
|----|------|----------|---------|-------|--------|-------|
| T-100 | Static registry | 58-entry compile-time array, sea_tool_exec dispatch | `sea_tools.h/c` | SeaClaw | ✅ | — |
| T-101 | Tool lookup | O(n) linear scan by name | `sea_tools.c` | SeaClaw | ✅ | — |
| T-102 | Hash table registry v2 | DJB2 hash, O(1) lookup, 64 dynamic slots | `sea_tools.h/c` | SeaClaw | ⬜ E1.3 | — |
| T-103 | Dynamic tool registration | sea_tool_register/unregister at runtime | `sea_tools.h/c` | SeaClaw | ⬜ E1.3 | — |

---

## 6. Memory & Recall

| ID | Task | Subtasks | File(s) | Owner | Status | Tests |
|----|------|----------|---------|-------|--------|-------|
| M-001 | Memory system | Read, write, append, daily summary, bootstrap | `sea_memory.h/c` | SeaClaw | ✅ | 🧪 test_memory (9) |
| M-002 | Recall engine | SQLite-backed facts, keyword search, importance scoring | `sea_recall.h/c` | SeaClaw | ✅ | 🧪 test_recall (8) |
| M-003 | Session manager | Create, load, save, list, delete sessions | `sea_session.h/c` | SeaClaw | ✅ | 🧪 test_session (12) |
| M-004 | Skill system | SKILL.md parse, YAML frontmatter, registry, enable/disable | `sea_skill.h/c` | SeaClaw | ✅ | 🧪 test_skill (13) |
| M-005 | Cron scheduler | Create, list, pause, resume, delete, tick execution | `sea_cron.h/c` | SeaClaw | ✅ | 🧪 test_cron (15) |
| M-006 | Usage tracking | Per-provider token/cost tracking, daily aggregation | `sea_usage.h/c` | SeaClaw | ✅ | — |
| M-007 | Proactive memory | Hierarchical memory, auto-categorize, session hooks | `sea_memory.h/c` | SeaClaw | ⬜ E3.1 | — |
| M-008 | Vector recall | Embedding generation, cosine similarity, hybrid search | `sea_recall.h/c` | SeaClaw | ⬜ E3.2 | — |
| M-009 | SOUL.md evolution | Self-authored identity, constitution, audit-logged | — | SeaClaw | ⬜ E3.3 | — |

---

## 7. SeaZero Integration

| ID | Task | Subtasks | File(s) | Owner | Status | Tests |
|----|------|----------|---------|-------|--------|-------|
| SZ-001 | Bridge API | sea_zero_init, delegate, health, register_tool | `sea_zero.h/c` | Hybrid | ✅ | 🧪 test_seazero |
| SZ-002 | LLM Proxy | HTTP listener :7432, token validation, budget, forwarding | `sea_proxy.h/c` | SeaClaw 🛡️ | ✅ | — |
| SZ-003 | Shared workspace | Create, list, read, cleanup, path traversal protection | `sea_workspace.h/c` | SeaClaw 🛡️ | ✅ | — |
| SZ-004 | Docker config | docker-compose.yml, seccomp, resource limits | `docker-compose.yml`, `seccomp.json` | SeaClaw | ✅ | — |
| SZ-005 | DB v3 schema | 4 tables, 8 indexes, 12 DB functions | `sea_db.h/c` | SeaClaw | ✅ | 🧪 test_seazero (19) |
| SZ-006 | Security hardening | Seccomp, PII filter, 64KB limit, read-only rootfs | `seccomp.json`, `sea_pii.c` | SeaClaw 🛡️ | ✅ | — |
| SZ-007 | TUI commands | /agents, /delegate, /sztasks, /usage, /audit | `main.c` | SeaClaw | ✅ | — |
| SZ-008 | Installer v3 | Optional Agent Zero step, token generation | `install.sh` | SeaClaw | ✅ | — |
| SZ-009 | Electron UI | Evaluate TUI+Telegram coverage, React frontend | — | SeaClaw | ⬜ Phase 7 | — |
| SZ-010 | RunPod serverless | SEA_LLM_RUNPOD provider, cold start handling | — | SeaClaw | ⬜ Phase 8 | — |

---

## 8. Distributed Systems

| ID | Task | Subtasks | File(s) | Owner | Status | Tests |
|----|------|----------|---------|-------|--------|-------|
| D-001 | Mesh network | Captain/Crew architecture, node registration, heartbeat | `sea_mesh.h/c` | SeaClaw | ✅ | — |
| D-002 | A2A protocol | HTTP JSON-RPC delegation, peer discovery, Shield verify | `sea_a2a.h/c` | SeaClaw | ✅ | — |

---

## 9. Main Application

| ID | Task | Subtasks | File(s) | Owner | Status | Tests |
|----|------|----------|---------|-------|--------|-------|
| APP-001 | CLI argument parsing | --doctor, --onboard, --gateway, --telegram, --version | `main.c` | SeaClaw | ✅ | — |
| APP-002 | TUI mode | Interactive terminal, command dispatch, /help | `main.c` | SeaClaw | ✅ | — |
| APP-003 | Telegram mode | Long-poll Telegram, command dispatch | `main.c` | SeaClaw | ✅ | — |
| APP-004 | Gateway mode | Bus-based multi-channel concurrent operation | `main.c` | SeaClaw | ✅ | — |
| APP-005 | Doctor mode | Config validation, provider check, DB check | `main.c` | SeaClaw | ✅ | — |
| APP-006 | Onboard wizard | Interactive first-run setup | `main.c` | SeaClaw | ✅ | — |
| APP-007 | Signal handling | SIGINT/SIGTERM graceful shutdown | `main.c` | SeaClaw | ✅ | — |

---

## 10. Infrastructure

| ID | Task | Subtasks | File(s) | Owner | Status |
|----|------|----------|---------|-------|--------|
| I-001 | Makefile | Arch detection, debug/release, test targets, seazero targets | `Makefile` | — | ✅ |
| I-002 | Dockerfile | Multi-stage build, minimal runtime image | `Dockerfile` | — | ✅ |
| I-003 | Dockerfile.dev | Development container with tools | `Dockerfile.dev` | — | ✅ |
| I-004 | Docker entrypoint | Container startup script | `docker-entrypoint.sh` | — | ✅ |
| I-005 | systemd service | Service unit file for daemon mode | `config/seaclaw.service` | — | ✅ |
| I-006 | Installer | curl\|bash one-liner, 7-step wizard | `install.sh` | — | ✅ |
| I-007 | .dockerignore | Exclude build artifacts from Docker context | `.dockerignore` | — | ✅ |
| I-008 | .gitignore | Organized patterns for secrets, binaries, artifacts | `.gitignore` | — | ✅ |

---

## 11. Documentation

| ID | Document | Description | Status |
|----|----------|-------------|--------|
| DOC-001 | `README.md` | Project overview, install, usage, architecture | ✅ |
| DOC-002 | `CHANGELOG.md` | Version history | ✅ |
| DOC-003 | `docs/ARCHITECTURE.md` | Five pillars, module map, dependency order | ✅ |
| DOC-004 | `docs/API_REFERENCE.md` | All public C APIs | ✅ |
| DOC-005 | `docs/TOOLS_REFERENCE.md` | All 58 tools with usage | ✅ |
| DOC-006 | `docs/ROADMAP_V1.md` | v1.0 roadmap (completed) | ✅ |
| DOC-007 | `docs/ROADMAP_V2.md` | v2.0 roadmap (in progress) | ✅ |
| DOC-008 | `docs/COMPARISON.md` | SeaClaw vs competitors | ✅ |
| DOC-009 | `docs/OPENCLAW_VS_SEACLAW_COMPARISON.md` | Detailed OpenClaw comparison | ✅ |
| DOC-010 | `docs/MESH_ARCHITECTURE.md` | Captain/Crew mesh design | ✅ |
| DOC-011 | `docs/DEV_WORKFLOW.md` | Developer workflow guide | ✅ |
| DOC-012 | `docs/SEAZERO_FLOW.md` | SeaZero data flow diagrams | ✅ |
| DOC-013 | `docs/KV_CACHE_CLOUD_VS_LOCAL_COMPARISON.md` | KV cache analysis | ✅ |
| DOC-014 | `docs/PRESS_RELEASE.md` | Press release template | ✅ |
| DOC-015 | `seazero/README.md` | SeaZero architecture docs | ✅ |
| DOC-016 | `seazero/SEAZERO_PLAN.md` | SeaZero implementation plan | ✅ |

---

## 12. Test Suites

| ID | Suite | File | Assertions | What It Tests |
|----|-------|------|------------|---------------|
| TST-001 | test_arena | `tests/test_arena.c` | 8 | Create, alloc, alignment, reset, overflow, destroy |
| TST-002 | test_bus | `tests/test_bus.c` | 11 | Create, push inbound/outbound, pop, overflow, destroy |
| TST-003 | test_config | `tests/test_config.c` | 7 | Parse JSON config, provider, model, fallbacks, defaults |
| TST-004 | test_cron | `tests/test_cron.c` | 15 | Create, add, list, pause, resume, delete, tick, overflow |
| TST-005 | test_db | `tests/test_db.c` | 11 | Open, config CRUD, trajectory, tasks, chat history, close |
| TST-006 | test_json | `tests/test_json.c` | 17 | Parse, get_string/int/bool, nested, arrays, edge cases |
| TST-007 | test_memory | `tests/test_memory.c` | 9 | Read, write, append, daily summary, bootstrap |
| TST-008 | test_pii | `tests/test_pii.c` | 16 | Email, phone, SSN, credit card, IP detection + redaction |
| TST-009 | test_recall | `tests/test_recall.c` | 8 | Remember, recall, forget, count, keyword search |
| TST-010 | test_seazero | `tests/test_seazero.c` | 19 | Agent register, task CRUD, LLM usage log, audit log |
| TST-011 | test_session | `tests/test_session.c` | 12 | Create, load, save, list, delete, overflow |
| TST-012 | test_shield | `tests/test_shield.c` | 19 | Input validation, injection detection, output filtering |
| TST-013 | test_skill | `tests/test_skill.c` | 13 | Parse SKILL.md, YAML frontmatter, registry, enable/disable |
| TST-014 | test_bench | `tests/test_bench.c` | — | Performance benchmarks (arena alloc, JSON parse, Shield) |
| | **TOTAL** | | **165** | |

---

## 13. Planned Features (Extensibility Roadmap)

### Phase E1: Extension Registry & CLI Subcommands

| ID | Task | Subtasks | Owner | Status | Priority |
|----|------|----------|-------|--------|----------|
| E1-001 | CLI subcommand dispatch | Refactor main.c into src/cli/ with 16 subcommands | SeaClaw | ⬜ | P0 |
| E1-002 | Extension point interface | sea_ext.h: SeaExtension, SeaExtTool, SeaExtChannel, SeaExtMemory | SeaClaw | ⬜ | P0 |
| E1-003 | Tool registry v2 | DJB2 hash table, O(1) lookup, 64 dynamic slots | SeaClaw | ⬜ | P1 |
| E1-004 | Doctor health scoring | 0-100 weighted score (config/providers/DB/security/skills) | SeaClaw | ⬜ | P1 |

### Phase E2: Skill Marketplace & Auth Framework

| ID | Task | Subtasks | Owner | Status | Priority |
|----|------|----------|-------|--------|----------|
| E2-001 | Skill install from URL | Download .md, validate YAML, Shield-check, copy to ~/.seaclaw/skills/ | SeaClaw | ⬜ | P0 |
| E2-002 | Plugin install from URL | Download, validate plugin.json, sandbox code via Agent Zero | Hybrid | ⬜ | P0 |
| E2-003 | AGENT.md auto-discovery | Walk CWD→project root for AGENT.md files | SeaClaw | ⬜ | P1 |
| E2-004 | Auth & token framework | sea_auth.h, Bearer tokens, permissions bitmask, SQLite storage | SeaClaw 🛡️ | ⬜ | P0 |
| E2-005 | Permission allowlists | File/shell/network allowlists, Shield enforcement | SeaClaw 🛡️ | ⬜ | P1 |
| E2-006 | Summarize tool (#59) | URL/text: SeaClaw. YouTube/PDF/audio: Agent Zero | Hybrid | ⬜ | P1 |

### Phase E3: Advanced Memory & Vector Search

| ID | Task | Subtasks | Owner | Status | Priority |
|----|------|----------|-------|--------|----------|
| E3-001 | Proactive memory | Hierarchical storage, auto-categorize via LLM, session hooks | SeaClaw | ⬜ | P1 |
| E3-002 | Vector recall | Embedding via LLM HTTP, SQLite BLOB, cosine similarity in C | SeaClaw | ⬜ | P2 |
| E3-003 | SOUL.md self-evolution | Agent writes ~/.seaclaw/SOUL.md, constitution, audit-logged | SeaClaw | ⬜ | P2 |

### Phase E4: Agent Testing & Observability

| ID | Task | Subtasks | Owner | Status | Priority |
|----|------|----------|-------|--------|----------|
| E4-001 | Agent compat testing | YAML test defs, cross-provider, shouldCallTool/shouldContain | SeaClaw | ⬜ | P1 |
| E4-002 | Session replay | Record LLM calls to SQLite, replay against different provider | SeaClaw | ⬜ | P2 |
| E4-003 | Usage dashboard | Per-provider breakdown, cost estimates, JSON/CSV export | SeaClaw | ⬜ | P1 |

### Phase E5: Security & Pentest Integration

| ID | Task | Subtasks | Owner | Status | Priority |
|----|------|----------|-------|--------|----------|
| E5-001 | Pentest mode | Delegate to Agent Zero with security prompt, nmap/sqlmap/nikto | Hybrid | ⬜ | P2 |
| E5-002 | Multi-agent pentest | Spawn researcher/developer/executor containers | Hybrid | ⬜ | P2 |
| E5-003 | Daemon mode | Background service, watchdog, auto-restart, PID file | SeaClaw | ⬜ | P1 |
| E5-004 | systemd integration | sea_claw service install/status/stop | SeaClaw | ⬜ | P1 |
| E5-005 | Gateway hardening | Randomized ports, key-based pairing, rate limiting, TLS | SeaClaw 🛡️ | ⬜ | P2 |

### Phase E6: Additional Features

| ID | Task | Subtasks | Owner | Status | Priority |
|----|------|----------|-------|--------|----------|
| E6-001 | TUI enhancements | Vim-style keys, @ trigger for inline LLM, attachment BLOBs | SeaClaw | ⬜ | P2 |
| E6-002 | Node-graph workflows | Visual editor for tool chain composition (Electron UI) | Hybrid | ⬜ | Future |
| E6-003 | WebLLM offline agent | In-browser LLM via WebGPU for Electron UI | SeaClaw | ⬜ | Future |
| E6-004 | Container inspection | docker exec from SeaClaw to inspect Agent Zero containers | SeaClaw | ⬜ | P2 |

---

## 14. File Manifest (153 tracked files)

### Headers (22 files)
```
include/seaclaw/sea_a2a.h        include/seaclaw/sea_agent.h
include/seaclaw/sea_arena.h      include/seaclaw/sea_bus.h
include/seaclaw/sea_channel.h    include/seaclaw/sea_config.h
include/seaclaw/sea_cron.h       include/seaclaw/sea_db.h
include/seaclaw/sea_http.h       include/seaclaw/sea_json.h
include/seaclaw/sea_log.h        include/seaclaw/sea_memory.h
include/seaclaw/sea_mesh.h       include/seaclaw/sea_pii.h
include/seaclaw/sea_recall.h     include/seaclaw/sea_session.h
include/seaclaw/sea_shield.h     include/seaclaw/sea_skill.h
include/seaclaw/sea_telegram.h   include/seaclaw/sea_tools.h
include/seaclaw/sea_types.h      include/seaclaw/sea_usage.h
```

### Source (72 files)
```
src/main.c                       src/core/sea_arena.c
src/core/sea_config.c            src/core/sea_db.c
src/core/sea_log.c               src/brain/sea_agent.c
src/bus/sea_bus.c                 src/channels/channel_telegram.c
src/channels/sea_channel.c       src/cron/sea_cron.c
src/memory/sea_memory.c          src/mesh/sea_mesh.c
src/pii/sea_pii.c                src/recall/sea_recall.c
src/senses/sea_http.c            src/senses/sea_json.c
src/session/sea_session.c        src/shield/sea_shield.c
src/skills/sea_skill.c           src/telegram/sea_telegram.c
src/usage/sea_usage.c            src/a2a/sea_a2a.c
src/hands/sea_tools.c            src/hands/impl/tool_*.c (49 files)
```

### SeaZero (10 files)
```
seazero/bridge/sea_zero.h/c      seazero/bridge/sea_proxy.h/c
seazero/bridge/sea_workspace.h/c seazero/config/agent-zero.env.example
seazero/config/seccomp.json      seazero/docker-compose.yml
seazero/scripts/setup.sh         seazero/scripts/spawn-agent.sh
```

### Tests (14 files)
```
tests/test_arena.c    tests/test_bench.c    tests/test_bus.c
tests/test_config.c   tests/test_cron.c     tests/test_db.c
tests/test_json.c     tests/test_memory.c   tests/test_pii.c
tests/test_recall.c   tests/test_seazero.c  tests/test_session.c
tests/test_shield.c   tests/test_skill.c
```

### Docs (16 files), Config (3 files), Scripts (3 files), Docker (4 files)
See Section 11 (Documentation) and Section 10 (Infrastructure).

---

## 15. Summary Statistics

| Metric | Value |
|--------|-------|
| Total tracked files | 153 |
| C source files | 72 |
| C header files | 22 |
| Test suites | 14 |
| Total assertions | 165 |
| Tools | 58 |
| LLM providers | 6 |
| Documentation files | 16 |
| Dependencies | 2 (libcurl, libsqlite3) |
| Binary size (stripped) | ~82KB |
| Planned features | 24 (E1-E6) |
