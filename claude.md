# Seaclaw — Project Documentation

> **Mono-document**: This is the single source of truth for all project documentation.
> All linked `.md` files are registered in the `md_files` table in the database.

---

## Infrastructure

| Resource | Details |
|----------|---------|
| **Server** | Hetzner — Ubuntu, 8GB RAM, Nuremberg |
| **IPv4** | `46.225.121.221` |
| **IPv6** | `2a01:4f8:1c19:2f33::/64` |
| **SSH** | Key-only (`~/.ssh/id_ed25519`), password auth disabled |
| **Connect** | `ssh root@46.225.121.221` |

## Database

| Resource | Details |
|----------|---------|
| **Engine** | PostgreSQL 16 |
| **Host** | `localhost` (on server) |
| **Database** | `seaclaw` |
| **User** | `seaclaw` |
| **Password** | `seaclaw@2026` |
| **Connect (from server)** | `psql -U seaclaw -d seaclaw -h localhost` |

## Database Schema

### `project_phases`
Defines project phases. Each phase groups related tasks.
- `id`, `phase_number`, `name`, `description`, `status`, timestamps

### `tasks`
Task lists distributed by phase. Tracks status, priority, dependencies.
- `id`, `phase_id` (FK), `title`, `description`, `status`, `priority`, `depends_on[]`, timestamps

### `trajectory`
Single table for all verification, documentation, decisions, milestones, and logs.
- `id`, `phase_id` (FK), `task_id` (FK), `entry_type`, `title`, `content`, `metadata` (JSONB), `tags[]`, `created_at`
- Entry types: `action`, `verification`, `documentation`, `decision`, `milestone`, `note`, `error`, `config`

### `md_files`
Registry of all `.md` documentation files with dates and chronology.
- `id`, `filename`, `relative_path`, `description`, `phase_id` (FK), `is_active`, timestamps

---

## Database Usage Rules & Guidelines

### Connection
- **Always** connect as user `seaclaw` to database `seaclaw` on `localhost`
- Use `psql -U seaclaw -d seaclaw -h localhost` from the server
- Password: `seaclaw@2026`

### Schema Conventions
- Table names: `snake_case`, plural (e.g., `tasks`, `project_phases`)
- Column names: `snake_case`
- Primary keys: `id SERIAL PRIMARY KEY`
- Foreign keys: `<table_singular>_id` (e.g., `phase_id`, `task_id`)
- Timestamps: always include `created_at TIMESTAMPTZ DEFAULT NOW()` and `updated_at TIMESTAMPTZ DEFAULT NOW()`
- Use `JSONB` for flexible metadata fields
- Use `TEXT[]` for tag/array columns

### Writing Data
- **Always** use parameterized queries — never interpolate user input into SQL strings
- Wrap multi-table writes in a transaction (`BEGIN` / `COMMIT`)
- Set `updated_at = NOW()` on every `UPDATE`
- Use `ON CONFLICT` (upsert) where appropriate to avoid duplicates

### Reading Data
- Select only needed columns — avoid `SELECT *` in production code
- Use indexes for frequently filtered columns (`status`, `phase_id`, `entry_type`)
- Paginate large result sets with `LIMIT` / `OFFSET` or cursor-based pagination

### Trajectory Table
- Use `trajectory` as the single audit/log table for all project events
- Always set `entry_type` to one of: `action`, `verification`, `documentation`, `decision`, `milestone`, `note`, `error`, `config`
- Include `phase_id` and/or `task_id` foreign keys when the entry relates to a specific phase or task
- Store structured data in the `metadata` JSONB column
- Use `tags` array for searchability

### md_files Registry
- Every `.md` file created in the project **must** be registered in `md_files`
- Set `is_active = true` for current docs, `false` for superseded ones
- Link to `phase_id` when the doc belongs to a specific phase

### Tasks Table
- Status values: `pending`, `in_progress`, `completed`, `blocked`
- Priority values: `low`, `medium`, `high`, `critical`
- Use `depends_on` array to express task dependencies (array of task IDs)

### Backup & Safety
- Never `DROP TABLE` or `TRUNCATE` without explicit user confirmation
- Prefer soft deletes (status = 'archived') over hard deletes
- Back up before schema migrations: `pg_dump -U seaclaw -d seaclaw > backup_$(date +%Y%m%d).sql`

---

## Documentation Files

| Date | File | Description |
|------|------|-------------|
| 2026-02-10 | `claude.md` | Mono-document — project documentation root |
| 2026-02-10 | `docs/PROJECT_STRUCTURE.md` | Merged file tree, decisions log, module ownership |
| 2026-02-10 | `docs/PERFORMANCE_PHILOSOPHY.md` | Three speeds, implementation rules, perf targets |
| 2026-02-10 | `docs/TERMINAL_UX_REFERENCE.md` | TUI startup, status line, layout, commands, colors |

---

## Project Goals

1. **Clean public GitHub repo** — only necessary files, no brainstorm junk, professional README
2. **OpenClaw comparison** — clone `openclaw/openclaw`, highlight C vs TS architecture differences
3. **Website at virtualgpt.cloud** — landing page + docs like openclaw.com / resonantos
4. **Database-driven tasks** — load/verify from DB, avoid heavy token usage and compression
5. **Build v1 executable** — Sea-Claw TUI + Telegram + tool calling, Agent-0 delegation, atomic vault

---

## Project Phases

> Source of truth: `SELECT phase_number, name, status FROM project_phases ORDER BY phase_number;`
> Tasks: `SELECT t.title, t.status, t.priority FROM tasks t JOIN project_phases p ON t.phase_id = p.id WHERE p.phase_number = <N> ORDER BY t.priority;`

| # | Phase | Tasks | Status |
|---|-------|-------|--------|
| 0 | Infrastructure & Setup | 13 | 🟢 In Progress |
| 1 | Repository & Structure | 7 | ⬜ Pending |
| 2 | OpenClaw Analysis & Comparison | 4 | ⬜ Pending |
| 3 | Core C Engine (v1) | 10 | ⬜ Pending |
| 4 | Tool System & Agent Loop | 10 | ⬜ Pending |
| 5 | Telegram Integration | 6 | ⬜ Pending |
| 6 | Website & Documentation | 5 | ⬜ Pending |
| 7 | Security & Hardening | 4 | ⬜ Pending |
| 8 | Launch & Polish | 5 | ⬜ Pending |

---

## Chronological Log

### 2026-02-10 — Project Kickoff
- Provisioned Hetzner server (46.225.121.221)
- Changed root password, set up SSH key-only access
- Installed PostgreSQL 16
- Created `seaclaw` database with schema: `project_phases`, `tasks`, `trajectory`, `md_files`
- Created this `claude.md` mono-document

### 2026-02-10 — Architecture & Planning
- Read and summarized all 27 files in `Kimi_Agent_OpenClaw C Rebuild Project/` folder
- Added Database Usage Rules & Guidelines to `claude.md`
- Created `docs/PROJECT_STRUCTURE.md` — merged brainstorm + Kimi evolution
- Created `docs/PERFORMANCE_PHILOSOPHY.md` — three speeds, implementation rules
- Created `docs/TERMINAL_UX_REFERENCE.md` — TUI startup, status line, layout
- Fixed PostgreSQL auth (`~/.pgpass` for passwordless `psql` access)
- Loaded 9 phases and 64 tasks into database
- Registered all `.md` files in `md_files` table
- Defined 5 project goals: clean repo, OpenClaw comparison, website, DB tasks, v1 executable
