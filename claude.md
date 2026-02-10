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

## Documentation Files

| Date | File | Description |
|------|------|-------------|
| 2026-02-11 | `claude.md` | Mono-document — project documentation root |

---

## Project Phases

| # | Phase | Status |
|---|-------|--------|
| 0 | Infrastructure & Setup | 🟢 In Progress |

---

## Chronological Log

### 2026-02-11 — Project Kickoff
- Provisioned Hetzner server (46.225.121.221)
- Changed root password, set up SSH key-only access
- Installed PostgreSQL 16
- Created `seaclaw` database with schema: `project_phases`, `tasks`, `trajectory`, `md_files`
- Created this `claude.md` mono-document
