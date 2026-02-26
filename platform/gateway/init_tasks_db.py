"""
Seed the platform_tasks SQLite database with all tasks from MASTER_PLAN_V3.md.
Run once: python3 init_tasks_db.py
"""
import sqlite3, os

DB_PATH = os.path.join(os.path.dirname(__file__), "platform_tasks.db")

SCHEMA = """
CREATE TABLE IF NOT EXISTS platform_tasks (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    phase       TEXT NOT NULL,
    task_id     TEXT NOT NULL UNIQUE,
    sprint      INTEGER NOT NULL,
    title       TEXT NOT NULL,
    effort      TEXT CHECK(effort IN ('S','M','H')),
    status      TEXT DEFAULT 'todo' CHECK(status IN ('todo','in_progress','done','blocked')),
    files       TEXT,
    notes       TEXT,
    created_at  DATETIME DEFAULT (datetime('now')),
    updated_at  DATETIME DEFAULT (datetime('now'))
);

CREATE INDEX IF NOT EXISTS idx_phase   ON platform_tasks(phase);
CREATE INDEX IF NOT EXISTS idx_sprint  ON platform_tasks(sprint);
CREATE INDEX IF NOT EXISTS idx_status  ON platform_tasks(status);
"""

TASKS = [
    # (phase, task_id, sprint, title, effort, files)
    # Phase 1 — Platform Dashboard
    ("P1","P1-01",1,"Dashboard tab: agent card with usage stats, uptime, model","S","platform/gateway/templates/index.html"),
    ("P1","P1-02",1,"Projects tab: create project, link to git repo, assign to agent","M","platform/gateway/templates/index.html, platform/gateway/main.py"),
    ("P1","P1-03",1,"POST /api/v1/agents/{user}/project — clone repo into /workspace","M","platform/gateway/main.py"),
    ("P1","P1-04",1,"GET /api/v1/agents/{user}/workspace — list files in /workspace","S","platform/gateway/main.py"),
    ("P1","P1-05",1,"Task board: list SeaZero tasks from pod DB (todo/in_progress/done)","M","platform/gateway/templates/index.html, platform/gateway/main.py"),
    ("P1","P1-06",1,"GET /api/v1/agents/{user}/tasks — proxy to pod /api/tasks","S","platform/gateway/main.py"),
    ("P1","P1-07",1,"Agent settings panel: change model, update API key","M","platform/gateway/templates/index.html, platform/gateway/main.py"),
    ("P1","P1-08",1,"PATCH /api/v1/agents/{user}/config — update config.json in running pod","M","platform/gateway/main.py"),
    ("P1","P1-09",1,"Telegram token field in signup + settings (optional)","S","platform/gateway/templates/index.html, platform/gateway/main.py"),
    ("P1","P1-10",1,"Swarm toggle in settings panel (on/off)","S","platform/gateway/templates/index.html"),

    # Phase 2 — Native Git Tools
    ("P2","P2-01",2,"tool_git_clone — clone a repo into /workspace/{project}","M","src/hands/tool_git.c"),
    ("P2","P2-02",2,"tool_git_pull — pull latest changes","S","src/hands/tool_git.c"),
    ("P2","P2-03",2,"tool_git_status — show changed files","S","src/hands/tool_git.c"),
    ("P2","P2-04",2,"tool_git_diff — show diff of changed files","S","src/hands/tool_git.c"),
    ("P2","P2-05",2,"tool_git_log — show recent commits","S","src/hands/tool_git.c"),
    ("P2","P2-06",2,"tool_git_checkout — switch branch","S","src/hands/tool_git.c"),
    ("P2","P2-07",2,"Register git tools #65-70 in sea_tools.c","S","src/hands/sea_tools.c"),
    ("P2","P2-08",2,"Rebuild Docker image + redeploy to K3s","S","platform/docker/Dockerfile.seaclaw"),
    ("P2","P2-09",2,"Test: ask alec to clone a repo and summarize it end-to-end","S","—"),

    # Phase 3 — Project Management Tools
    ("P3","P3-01",2,"tool_task_create — create task in SQLite seazero_tasks","S","src/hands/tool_pm.c"),
    ("P3","P3-02",2,"tool_task_list — list tasks by status/project","S","src/hands/tool_pm.c"),
    ("P3","P3-03",2,"tool_task_update — update task status, add notes","S","src/hands/tool_pm.c"),
    ("P3","P3-04",2,"tool_report_generate — LLM summarizes tasks into markdown report","M","src/hands/tool_pm.c"),
    ("P3","P3-05",2,"tool_milestone — set milestone, track % complete","S","src/hands/tool_pm.c"),
    ("P3","P3-06",2,"Register PM tools in sea_tools.c","S","src/hands/sea_tools.c"),
    ("P3","P3-07",2,"Add GET /api/tasks endpoint to sea_api.c","M","src/api/sea_api.c"),
    ("P3","P3-08",2,"Dashboard Kanban columns (To Do / In Progress / Done) from tasks API","M","platform/gateway/templates/index.html"),

    # Phase 4 — Agent Swarm
    ("P4","P4-01",3,"tool_spawn_worker — call gateway to create ephemeral worker pod","M","src/hands/tool_swarm.c"),
    ("P4","P4-02",3,"Worker pod lifecycle: auto-delete after task complete","M","platform/gateway/main.py"),
    ("P4","P4-03",3,"POST /api/v1/agents/{user}/workers — create named worker","M","platform/gateway/main.py"),
    ("P4","P4-04",3,"Inter-pod messaging via gateway relay","M","platform/gateway/main.py, src/api/sea_api.c"),
    ("P4","P4-05",3,"Coordinator prompt template: decompose → assign → collect","M","platform/souls/coordinator.md"),
    ("P4","P4-06",3,"Swarm toggle in user config + dashboard UI","S","platform/gateway/main.py, platform/gateway/templates/index.html"),
    ("P4","P4-07",3,"Test: analyze codebase and generate PR review via swarm","M","—"),

    # Phase 5 — Agent Zero (Premium)
    ("P5","P5-01",4,"Build Agent Zero Docker image for K3s (arm64 + amd64)","M","platform/docker/Dockerfile.agentzero"),
    ("P5","P5-02",4,"K8s manifest: shared agent-zero pod + ClusterIP Service","S","platform/k8s/agent-zero.yaml"),
    ("P5","P5-03",4,"Signup form: Enable Agent Zero toggle + separate LLM key option","S","platform/gateway/templates/index.html"),
    ("P5","P5-04",4,"Per-user token budget config field (default 100K tokens/day)","S","platform/gateway/main.py"),
    ("P5","P5-05",4,"Gateway injects SEAZERO_AGENT_URL + SEAZERO_TOKEN into pod env","M","platform/gateway/main.py"),
    ("P5","P5-06",4,"LLM proxy multi-tenant: per-user token + budget tracking","H","src/seazero/sea_proxy.c"),
    ("P5","P5-07",4,"Dashboard: AZ status indicator + task queue depth","M","platform/gateway/templates/index.html"),
    ("P5","P5-08",4,"Test: ask agent to run Python script that downloads and analyzes data","M","—"),

    # Phase 6 — K8s Multi-Node + Autoscaling
    ("P6","P6-01",5,"K3s agent join script (for RPi / second VPS)","S","platform/scripts/join-node.sh"),
    ("P6","P6-02",5,"Node labels: capability-based scheduling (arm64, gpu, high-memory)","S","platform/k8s/node-labels.yaml"),
    ("P6","P6-03",5,"Longhorn distributed storage OR NFS for cross-node PVCs","H","platform/k8s/storage.yaml"),
    ("P6","P6-04",5,"HPA for gateway: scale 1→10 replicas at CPU >50%","S","platform/k8s/gateway-hpa.yaml"),
    ("P6","P6-05",5,"PodDisruptionBudget for gateway (always 1 available)","S","platform/k8s/gateway-pdb.yaml"),
    ("P6","P6-06",5,"Resource limits on SeaClaw pods (100m CPU, 64Mi RAM)","S","platform/gateway/main.py"),
    ("P6","P6-07",5,"LimitRange + ResourceQuota per namespace","S","platform/k8s/quotas.yaml"),
    ("P6","P6-08",5,"Test: join RPi node, create agent, verify it schedules to RPi","M","—"),

    # Phase 7 — Multi-Channel
    ("P7","P7-01",6,"channel_discord.c — Discord bot via HTTP Events API","H","src/channels/channel_discord.c"),
    ("P7","P7-02",6,"channel_slack.c — Slack via Socket Mode","H","src/channels/channel_slack.c"),
    ("P7","P7-03",6,"Discord/Slack token fields in signup form + settings","M","platform/gateway/templates/index.html"),
    ("P7","P7-04",6,"Gateway injects channel tokens into pod env vars","M","platform/gateway/main.py"),
    ("P7","P7-05",6,"Voice support: Whisper transcription via Groq API","M","src/channels/sea_voice.c"),
]

def seed():
    conn = sqlite3.connect(DB_PATH)
    conn.executescript(SCHEMA)
    conn.executemany(
        """INSERT OR IGNORE INTO platform_tasks
           (phase, task_id, sprint, title, effort, files)
           VALUES (?,?,?,?,?,?)""",
        TASKS
    )
    conn.commit()
    count = conn.execute("SELECT COUNT(*) FROM platform_tasks").fetchone()[0]
    print(f"✅ Seeded {count} tasks into {DB_PATH}")

    # Summary by phase
    for row in conn.execute("SELECT phase, COUNT(*) FROM platform_tasks GROUP BY phase ORDER BY phase"):
        print(f"   {row[0]}: {row[1]} tasks")
    conn.close()

if __name__ == "__main__":
    seed()
