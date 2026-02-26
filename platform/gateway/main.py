"""
SeaClaw Platform Gateway — Signup UI + K8s Instance Manager

Serves a web form where users enter their API keys, bot tokens, and soul preference.
On submit, creates a Kubernetes pod running an isolated SeaClaw instance for that user.
"""

import os
import json
import secrets
import logging
import sqlite3
import httpx
from datetime import datetime
from pathlib import Path
from typing import Optional, List

from fastapi import FastAPI, Request, HTTPException
from fastapi.responses import HTMLResponse, JSONResponse
from fastapi.staticfiles import StaticFiles
from fastapi.templating import Jinja2Templates
from pydantic import BaseModel, Field

from kubernetes import client, config as k8s_config
from kubernetes.client.rest import ApiException

# ── Config ───────────────────────────────────────────────────

NAMESPACE = os.getenv("SEACLAW_NAMESPACE", "seaclaw-platform")
SEACLAW_IMAGE = os.getenv("SEACLAW_IMAGE", "seaclaw-instance:latest")
MAX_INSTANCES = int(os.getenv("MAX_INSTANCES", "5"))
DATA_DIR = Path(os.getenv("DATA_DIR", "/data/platform"))
LOG_LEVEL = os.getenv("LOG_LEVEL", "INFO")

logging.basicConfig(level=getattr(logging, LOG_LEVEL))
logger = logging.getLogger("gateway")

# ── K8s Client ───────────────────────────────────────────────

def init_k8s():
    """Initialize K8s client — in-cluster or kubeconfig."""
    try:
        k8s_config.load_incluster_config()
        logger.info("Loaded in-cluster K8s config")
    except k8s_config.ConfigException:
        try:
            k8s_config.load_kube_config()
            logger.info("Loaded kubeconfig")
        except k8s_config.ConfigException:
            logger.warning("No K8s config found — running in standalone mode")
            return None, None
    return client.CoreV1Api(), client.AppsV1Api()

v1, apps_v1 = init_k8s()

# ── Platform Task DB bootstrap ────────────────────────────────

PLATFORM_TASKS_DB = DATA_DIR / "platform_tasks.db"

_TASKS_SCHEMA = """
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

_SEED_TASKS = [
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
    ("P2","P2-01",2,"tool_git_clone — clone a repo into /workspace/{project}","M","src/hands/tool_git.c"),
    ("P2","P2-02",2,"tool_git_pull — pull latest changes","S","src/hands/tool_git.c"),
    ("P2","P2-03",2,"tool_git_status — show changed files","S","src/hands/tool_git.c"),
    ("P2","P2-04",2,"tool_git_diff — show diff of changed files","S","src/hands/tool_git.c"),
    ("P2","P2-05",2,"tool_git_log — show recent commits","S","src/hands/tool_git.c"),
    ("P2","P2-06",2,"tool_git_checkout — switch branch","S","src/hands/tool_git.c"),
    ("P2","P2-07",2,"Register git tools #65-70 in sea_tools.c","S","src/hands/sea_tools.c"),
    ("P2","P2-08",2,"Rebuild Docker image + redeploy to K3s","S","platform/docker/Dockerfile.seaclaw"),
    ("P2","P2-09",2,"Test: ask alec to clone a repo and summarize it end-to-end","S","—"),
    ("P3","P3-01",2,"tool_task_create — create task in SQLite seazero_tasks","S","src/hands/tool_pm.c"),
    ("P3","P3-02",2,"tool_task_list — list tasks by status/project","S","src/hands/tool_pm.c"),
    ("P3","P3-03",2,"tool_task_update — update task status, add notes","S","src/hands/tool_pm.c"),
    ("P3","P3-04",2,"tool_report_generate — LLM summarizes tasks into markdown report","M","src/hands/tool_pm.c"),
    ("P3","P3-05",2,"tool_milestone — set milestone, track % complete","S","src/hands/tool_pm.c"),
    ("P3","P3-06",2,"Register PM tools in sea_tools.c","S","src/hands/sea_tools.c"),
    ("P3","P3-07",2,"Add GET /api/tasks endpoint to sea_api.c","M","src/api/sea_api.c"),
    ("P3","P3-08",2,"Dashboard Kanban columns (To Do / In Progress / Done) from tasks API","M","platform/gateway/templates/index.html"),
    ("P4","P4-01",3,"tool_spawn_worker — call gateway to create ephemeral worker pod","M","src/hands/tool_swarm.c"),
    ("P4","P4-02",3,"Worker pod lifecycle: auto-delete after task complete","M","platform/gateway/main.py"),
    ("P4","P4-03",3,"POST /api/v1/agents/{user}/workers — create named worker","M","platform/gateway/main.py"),
    ("P4","P4-04",3,"Inter-pod messaging via gateway relay","M","platform/gateway/main.py, src/api/sea_api.c"),
    ("P4","P4-05",3,"Coordinator prompt template: decompose — assign — collect","M","platform/souls/coordinator.md"),
    ("P4","P4-06",3,"Swarm toggle in user config + dashboard UI","S","platform/gateway/main.py, platform/gateway/templates/index.html"),
    ("P4","P4-07",3,"Test: analyze codebase and generate PR review via swarm","M","—"),
    ("P5","P5-01",4,"Build Agent Zero Docker image for K3s (arm64 + amd64)","M","platform/docker/Dockerfile.agentzero"),
    ("P5","P5-02",4,"K8s manifest: shared agent-zero pod + ClusterIP Service","S","platform/k8s/agent-zero.yaml"),
    ("P5","P5-03",4,"Signup form: Enable Agent Zero toggle + separate LLM key option","S","platform/gateway/templates/index.html"),
    ("P5","P5-04",4,"Per-user token budget config field (default 100K tokens/day)","S","platform/gateway/main.py"),
    ("P5","P5-05",4,"Gateway injects SEAZERO_AGENT_URL + SEAZERO_TOKEN into pod env","M","platform/gateway/main.py"),
    ("P5","P5-06",4,"LLM proxy multi-tenant: per-user token + budget tracking","H","src/seazero/sea_proxy.c"),
    ("P5","P5-07",4,"Dashboard: AZ status indicator + task queue depth","M","platform/gateway/templates/index.html"),
    ("P5","P5-08",4,"Test: ask agent to run Python script that downloads and analyzes data","M","—"),
    ("P6","P6-01",5,"K3s agent join script (for RPi / second VPS)","S","platform/scripts/join-node.sh"),
    ("P6","P6-02",5,"Node labels: capability-based scheduling (arm64, gpu, high-memory)","S","platform/k8s/node-labels.yaml"),
    ("P6","P6-03",5,"Longhorn distributed storage OR NFS for cross-node PVCs","H","platform/k8s/storage.yaml"),
    ("P6","P6-04",5,"HPA for gateway: scale 1→10 replicas at CPU >50%","S","platform/k8s/gateway-hpa.yaml"),
    ("P6","P6-05",5,"PodDisruptionBudget for gateway (always 1 available)","S","platform/k8s/gateway-pdb.yaml"),
    ("P6","P6-06",5,"Resource limits on SeaClaw pods (100m CPU, 64Mi RAM)","S","platform/gateway/main.py"),
    ("P6","P6-07",5,"LimitRange + ResourceQuota per namespace","S","platform/k8s/quotas.yaml"),
    ("P6","P6-08",5,"Test: join RPi node, create agent, verify it schedules to RPi","M","—"),
    ("P7","P7-01",6,"channel_discord.c — Discord bot via HTTP Events API","H","src/channels/channel_discord.c"),
    ("P7","P7-02",6,"channel_slack.c — Slack via Socket Mode","H","src/channels/channel_slack.c"),
    ("P7","P7-03",6,"Discord/Slack token fields in signup form + settings","M","platform/gateway/templates/index.html"),
    ("P7","P7-04",6,"Gateway injects channel tokens into pod env vars","M","platform/gateway/main.py"),
    ("P7","P7-05",6,"Voice support: Whisper transcription via Groq API","M","src/channels/sea_voice.c"),
]

def _init_tasks_db():
    """Create and seed platform_tasks.db in DATA_DIR on first boot."""
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    conn = sqlite3.connect(str(PLATFORM_TASKS_DB))
    conn.executescript(_TASKS_SCHEMA)
    conn.executemany(
        "INSERT OR IGNORE INTO platform_tasks (phase,task_id,sprint,title,effort,files) VALUES (?,?,?,?,?,?)",
        _SEED_TASKS
    )
    conn.commit()
    count = conn.execute("SELECT COUNT(*) FROM platform_tasks").fetchone()[0]
    conn.close()
    logger.info(f"platform_tasks.db ready: {count} tasks at {PLATFORM_TASKS_DB}")

_init_tasks_db()

# ── App ──────────────────────────────────────────────────────

app = FastAPI(title="SeaClaw Platform", version="0.1.0")

templates_dir = Path(__file__).parent / "templates"
static_dir = Path(__file__).parent / "static"

if templates_dir.exists():
    templates = Jinja2Templates(directory=str(templates_dir))
if static_dir.exists():
    app.mount("/static", StaticFiles(directory=str(static_dir)), name="static")

# ── Models ───────────────────────────────────────────────────

class CreateAgentRequest(BaseModel):
    username: str = Field(..., min_length=2, max_length=32, pattern=r"^[a-z0-9_-]+$")
    email: Optional[str] = None
    llm_provider: str = Field(default="openrouter")
    api_key: str = Field(..., min_length=5)
    model: str = Field(default="moonshotai/kimi-k2")
    soul: str = Field(default="alex")
    telegram_token: Optional[str] = None
    telegram_chat_id: Optional[str] = None
    enable_webchat: bool = True
    enable_pii: bool = True
    enable_shield: bool = True
    enable_agent_zero: bool = True
    token_budget: int = Field(default=100000, ge=1000, le=1000000)

class AgentStatus(BaseModel):
    username: str
    status: str
    soul: str
    created_at: str
    webchat_url: Optional[str] = None
    telegram_bot: Optional[str] = None
    pod_name: Optional[str] = None

# ── Instance DB (JSON file for demo, SQLite for prod) ────────

INSTANCES_FILE = DATA_DIR / "instances.json"

def _load_instances() -> dict:
    if INSTANCES_FILE.exists():
        return json.loads(INSTANCES_FILE.read_text())
    return {}

def _save_instances(data: dict):
    INSTANCES_FILE.parent.mkdir(parents=True, exist_ok=True)
    INSTANCES_FILE.write_text(json.dumps(data, indent=2))

# ── LLM Provider URLs ───────────────────────────────────────

PROVIDER_URLS = {
    "openrouter": "https://openrouter.ai/api/v1/chat/completions",
    "openai": "https://api.openai.com/v1/chat/completions",
    "anthropic": "https://api.anthropic.com/v1/messages",
    "google": "https://generativelanguage.googleapis.com/v1beta/models",
    "ollama": "http://localhost:11434/v1/chat/completions",
}

# ── K8s Pod Creation ─────────────────────────────────────────

def create_seaclaw_pod(req: CreateAgentRequest) -> str:
    """Create a K8s pod for a SeaClaw user instance."""
    if v1 is None:
        raise HTTPException(503, "K8s not available")

    pod_name = f"seaclaw-{req.username}"
    bridge_token = secrets.token_hex(32)
    webchat_port = 8899  # SeaClaw HTTP API port (POST /api/chat)

    # Build config.json content
    api_url = PROVIDER_URLS.get(req.llm_provider, PROVIDER_URLS["openrouter"])
    config_data = {
        "llm_provider": req.llm_provider,
        "llm_api_key": req.api_key,
        "llm_api_url": api_url,
        "llm_model": req.model,
        "system_prompt": None,
        "max_tokens": 4096,
        "temperature": 0.7,
        "max_tool_rounds": 5,
        "pii_categories": 31 if req.enable_pii else 0,
        "seazero_enabled": req.enable_agent_zero,
        "seazero_token": bridge_token,
        "seazero_agent_url": f"http://agent-zero-svc.{NAMESPACE}.svc.cluster.local:8080",
        "seazero_budget": req.token_budget,
    }

    # Build environment variables
    gw_svc_url = f"http://gateway-svc.{NAMESPACE}.svc.cluster.local:8090"
    env_vars = [
        client.V1EnvVar(name="SEA_LOG_LEVEL",    value="info"),
        client.V1EnvVar(name="SEA_API_BIND_ALL", value="1"),
        client.V1EnvVar(name="SEA_USERNAME",     value=req.username),
        client.V1EnvVar(name="SEA_GATEWAY_URL",  value=gw_svc_url),
        client.V1EnvVar(name="SEA_DB",           value="/userdata/seaclaw.db"),
    ]
    if req.telegram_token:
        env_vars.append(client.V1EnvVar(name="TELEGRAM_BOT_TOKEN", value=req.telegram_token))
    if req.telegram_chat_id:
        env_vars.append(client.V1EnvVar(name="TELEGRAM_CHAT_ID", value=req.telegram_chat_id))

    # ConfigMap for this user's config.json
    configmap_name = f"seaclaw-config-{req.username}"
    configmap = client.V1ConfigMap(
        metadata=client.V1ObjectMeta(name=configmap_name, namespace=NAMESPACE),
        data={"config.json": json.dumps(config_data, indent=2)}
    )

    try:
        v1.create_namespaced_config_map(namespace=NAMESPACE, body=configmap)
    except ApiException as e:
        if e.status == 409:  # already exists
            v1.replace_namespaced_config_map(
                name=configmap_name, namespace=NAMESPACE, body=configmap)
        else:
            raise HTTPException(500, f"ConfigMap creation failed: {e.reason}")

    # Soul file as ConfigMap
    soul_configmap_name = f"seaclaw-soul-{req.username}"
    soul_path = Path(__file__).parent.parent / "souls" / f"{req.soul}.md"
    soul_content = soul_path.read_text() if soul_path.exists() else f"# {req.soul.title()}\nYou are a helpful AI assistant."
    soul_cm = client.V1ConfigMap(
        metadata=client.V1ObjectMeta(name=soul_configmap_name, namespace=NAMESPACE),
        data={"SOUL.md": soul_content}
    )
    try:
        v1.create_namespaced_config_map(namespace=NAMESPACE, body=soul_cm)
    except ApiException as e:
        if e.status == 409:
            v1.replace_namespaced_config_map(
                name=soul_configmap_name, namespace=NAMESPACE, body=soul_cm)
        else:
            raise HTTPException(500, f"Soul ConfigMap creation failed: {e.reason}")

    # Pod definition
    pod = client.V1Pod(
        metadata=client.V1ObjectMeta(
            name=pod_name,
            namespace=NAMESPACE,
            labels={
                "app": "seaclaw-instance",
                "user": req.username,
                "soul": req.soul,
            }
        ),
        spec=client.V1PodSpec(
            containers=[
                client.V1Container(
                    name="seaclaw",
                    image=SEACLAW_IMAGE,
                    image_pull_policy="IfNotPresent",
                    env=env_vars,
                    ports=[
                        client.V1ContainerPort(container_port=webchat_port, name="webchat"),
                    ],
                    resources=client.V1ResourceRequirements(
                        requests={"cpu": "50m", "memory": "32Mi"},
                        limits={"cpu": "500m", "memory": "128Mi"},
                    ),
                    command=["sea_claw"],
                    args=[
                        "--config", "/userdata/config.json",
                        "--db", "/userdata/seaclaw.db",
                        "--gateway",
                    ],
                    volume_mounts=[
                        client.V1VolumeMount(
                            name="user-data", mount_path="/userdata",
                            sub_path=req.username),
                        client.V1VolumeMount(
                            name="shared-workspace", mount_path="/workspace"),
                    ],
                )
            ],
            init_containers=[
                client.V1Container(
                    name="init-config",
                    image="busybox:1.36",
                    command=["sh", "-c",
                        "mkdir -p /userdata && "
                        "cp /cfg/config.json /userdata/config.json && "
                        "cp /soul/SOUL.md /userdata/SOUL.md && "
                        "echo 'Config initialized' && "
                        "ls -la /userdata/"
                    ],
                    volume_mounts=[
                        client.V1VolumeMount(
                            name="config", mount_path="/cfg", read_only=True),
                        client.V1VolumeMount(
                            name="soul", mount_path="/soul", read_only=True),
                        client.V1VolumeMount(
                            name="user-data", mount_path="/userdata",
                            sub_path=req.username),
                    ],
                )
            ],
            volumes=[
                client.V1Volume(
                    name="config",
                    config_map=client.V1ConfigMapVolumeSource(name=configmap_name)),
                client.V1Volume(
                    name="soul",
                    config_map=client.V1ConfigMapVolumeSource(name=soul_configmap_name)),
                client.V1Volume(
                    name="user-data",
                    persistent_volume_claim=client.V1PersistentVolumeClaimVolumeSource(
                        claim_name="seaclaw-user-data")),
                client.V1Volume(
                    name="shared-workspace",
                    persistent_volume_claim=client.V1PersistentVolumeClaimVolumeSource(
                        claim_name="seaclaw-shared-workspace")),
            ],
            restart_policy="Always",
        )
    )

    try:
        v1.create_namespaced_pod(namespace=NAMESPACE, body=pod)
    except ApiException as e:
        if e.status == 409:
            raise HTTPException(409, f"Agent '{req.username}' already exists")
        raise HTTPException(500, f"Pod creation failed: {e.reason}")

    # Create a Service for this user's WebChat
    service = client.V1Service(
        metadata=client.V1ObjectMeta(
            name=f"seaclaw-{req.username}-svc",
            namespace=NAMESPACE,
            labels={"app": "seaclaw-instance", "user": req.username},
        ),
        spec=client.V1ServiceSpec(
            selector={"app": "seaclaw-instance", "user": req.username},
            ports=[client.V1ServicePort(
                port=webchat_port, target_port=webchat_port, name="webchat")],
            type="ClusterIP",
        )
    )
    try:
        v1.create_namespaced_service(namespace=NAMESPACE, body=service)
    except ApiException as e:
        if e.status != 409:
            logger.warning(f"Service creation failed: {e.reason}")

    return pod_name

def delete_seaclaw_pod(username: str):
    """Delete a user's SeaClaw pod and associated resources."""
    if v1 is None:
        raise HTTPException(503, "K8s not available")

    pod_name = f"seaclaw-{username}"
    try:
        v1.delete_namespaced_pod(name=pod_name, namespace=NAMESPACE)
    except ApiException as e:
        if e.status != 404:
            raise HTTPException(500, f"Pod deletion failed: {e.reason}")

    # Clean up service
    try:
        v1.delete_namespaced_service(
            name=f"seaclaw-{username}-svc", namespace=NAMESPACE)
    except ApiException:
        pass

    # Clean up configmaps
    for cm_name in [f"seaclaw-config-{username}", f"seaclaw-soul-{username}"]:
        try:
            v1.delete_namespaced_config_map(name=cm_name, namespace=NAMESPACE)
        except ApiException:
            pass

def get_pod_status(username: str) -> Optional[dict]:
    """Get the status of a user's SeaClaw pod."""
    if v1 is None:
        return None
    try:
        pod = v1.read_namespaced_pod(
            name=f"seaclaw-{username}", namespace=NAMESPACE)
        return {
            "phase": pod.status.phase,
            "ready": all(
                cs.ready for cs in (pod.status.container_statuses or [])
            ),
            "ip": pod.status.pod_ip,
        }
    except ApiException:
        return None

# ── API Routes ───────────────────────────────────────────────

@app.get("/", response_class=HTMLResponse)
async def home(request: Request):
    """Serve the signup form."""
    instances = _load_instances()
    return templates.TemplateResponse("index.html", {
        "request": request,
        "instances": instances,
        "max_instances": MAX_INSTANCES,
    })

@app.get("/health")
async def health():
    return {"status": "ok", "timestamp": datetime.utcnow().isoformat()}

@app.post("/api/v1/agents/create")
async def create_agent(req: CreateAgentRequest):
    """Create a new SeaClaw agent instance."""
    instances = _load_instances()

    if len(instances) >= MAX_INSTANCES:
        raise HTTPException(400, f"Maximum {MAX_INSTANCES} instances reached")

    if req.username in instances:
        raise HTTPException(409, f"Agent '{req.username}' already exists")

    # Create K8s pod
    pod_name = create_seaclaw_pod(req)

    # Save to instance registry
    instances[req.username] = {
        "username": req.username,
        "email": req.email,
        "soul": req.soul,
        "llm_provider": req.llm_provider,
        "model": req.model,
        "has_telegram": bool(req.telegram_token),
        "has_webchat": req.enable_webchat,
        "enable_agent_zero": req.enable_agent_zero,
        "token_budget": req.token_budget,
        "pod_name": pod_name,
        "created_at": datetime.utcnow().isoformat(),
        "status": "starting",
    }
    _save_instances(instances)

    logger.info(f"Created agent for user '{req.username}' (soul={req.soul}, pod={pod_name})")

    return {
        "status": "created",
        "username": req.username,
        "pod_name": pod_name,
        "webchat_url": f"/chat/{req.username}" if req.enable_webchat else None,
        "message": f"Agent '{req.username}' is starting. It will be ready in a few seconds.",
    }

@app.get("/api/v1/agents")
async def list_agents():
    """List all agent instances."""
    instances = _load_instances()
    result = []
    for username, info in instances.items():
        pod_status = get_pod_status(username)
        status = "running" if pod_status and pod_status.get("ready") else \
                 pod_status["phase"].lower() if pod_status else "unknown"
        result.append({
            **info,
            "status": status,
        })
    return {"agents": result, "count": len(result), "max": MAX_INSTANCES}

@app.get("/api/v1/agents/{username}")
async def get_agent(username: str):
    """Get a specific agent's status."""
    instances = _load_instances()
    if username not in instances:
        raise HTTPException(404, f"Agent '{username}' not found")

    info = instances[username]
    pod_status = get_pod_status(username)
    status = "running" if pod_status and pod_status.get("ready") else \
             pod_status["phase"].lower() if pod_status else "unknown"

    return {**info, "status": status, "pod": pod_status}

@app.delete("/api/v1/agents/{username}")
async def delete_agent(username: str):
    """Delete an agent instance."""
    instances = _load_instances()
    if username not in instances:
        raise HTTPException(404, f"Agent '{username}' not found")

    delete_seaclaw_pod(username)
    del instances[username]
    _save_instances(instances)

    logger.info(f"Deleted agent for user '{username}'")
    return {"status": "deleted", "username": username}

@app.post("/api/v1/agents/{username}/restart")
async def restart_agent(username: str):
    """Restart an agent instance."""
    instances = _load_instances()
    if username not in instances:
        raise HTTPException(404, f"Agent '{username}' not found")

    # Delete and recreate pod
    delete_seaclaw_pod(username)

    # Rebuild CreateAgentRequest from stored info
    info = instances[username]
    # Pod will be recreated with same config
    # For demo, we just delete the pod — K8s restartPolicy:Always handles it
    return {"status": "restarting", "username": username}

# ── WebChat proxy ─────────────────────────────────────────────

API_PORT = 8899  # SeaClaw HTTP API port inside each pod

async def _proxy_chat(username: str, message: str) -> dict:
    """Forward a chat message to a user's SeaClaw pod API."""
    # Resolve pod IP via K8s service DNS
    svc_url = f"http://seaclaw-{username}-svc.{NAMESPACE}.svc.cluster.local:{API_PORT}"
    try:
        async with httpx.AsyncClient(timeout=120.0) as http:
            resp = await http.post(
                f"{svc_url}/api/chat",
                json={"message": message},
            )
            resp.raise_for_status()
            return resp.json()
    except httpx.ConnectError:
        raise HTTPException(503, f"Agent '{username}' is not reachable. Is the pod running?")
    except httpx.TimeoutException:
        raise HTTPException(504, f"Agent '{username}' timed out (120s)")
    except httpx.HTTPStatusError as e:
        raise HTTPException(e.response.status_code, f"Agent error: {e.response.text}")


class ChatRequest(BaseModel):
    message: str = Field(..., min_length=1, max_length=8192)

class UpdateConfigRequest(BaseModel):
    model: Optional[str] = None
    api_key: Optional[str] = None
    llm_provider: Optional[str] = None
    token_budget: Optional[int] = Field(default=None, ge=1000, le=1000000)
    enable_agent_zero: Optional[bool] = None
    swarm_mode: Optional[bool] = None

class ProjectRequest(BaseModel):
    repo_url: str = Field(..., min_length=5)
    project_name: Optional[str] = None
    branch: str = Field(default="main")


@app.patch("/api/v1/agents/{username}/config")
async def update_agent_config(username: str, req: UpdateConfigRequest):
    """Update a running agent's configuration."""
    instances = _load_instances()
    if username not in instances:
        raise HTTPException(404, f"Agent '{username}' not found")

    info = instances[username]
    configmap_name = f"seaclaw-config-{username}"

    # Load current configmap
    try:
        cm = v1.read_namespaced_config_map(name=configmap_name, namespace=NAMESPACE)
        config_data = json.loads(cm.data["config.json"])
    except Exception:
        config_data = {}

    # Apply changes
    if req.model is not None:
        config_data["llm_model"] = req.model
        info["model"] = req.model
    if req.api_key is not None:
        config_data["llm_api_key"] = req.api_key
    if req.llm_provider is not None:
        config_data["llm_provider"] = req.llm_provider
        config_data["llm_api_url"] = PROVIDER_URLS.get(req.llm_provider, PROVIDER_URLS["openrouter"])
        info["llm_provider"] = req.llm_provider
    if req.token_budget is not None:
        config_data["seazero_budget"] = req.token_budget
        info["token_budget"] = req.token_budget
    if req.enable_agent_zero is not None:
        config_data["seazero_enabled"] = req.enable_agent_zero
        info["enable_agent_zero"] = req.enable_agent_zero
    if req.swarm_mode is not None:
        config_data["swarm_mode"] = req.swarm_mode
        info["swarm_mode"] = req.swarm_mode

    # Update configmap
    try:
        cm.data["config.json"] = json.dumps(config_data, indent=2)
        v1.replace_namespaced_config_map(name=configmap_name, namespace=NAMESPACE, body=cm)
    except Exception as e:
        raise HTTPException(500, f"ConfigMap update failed: {e}")

    info["updated_at"] = datetime.utcnow().isoformat()
    instances[username] = info
    _save_instances(instances)

    return {"status": "updated", "username": username, "changes": req.dict(exclude_none=True)}


@app.post("/api/v1/agents/{username}/project")
async def create_project(username: str, req: ProjectRequest):
    """Clone a git repo into the agent's /workspace/{project_name}."""
    instances = _load_instances()
    if username not in instances:
        raise HTTPException(404, f"Agent '{username}' not found")

    project_name = req.project_name or req.repo_url.rstrip("/").split("/")[-1].removesuffix(".git")

    # Sanitize project name
    import re
    project_name = re.sub(r"[^a-zA-Z0-9_-]", "-", project_name)[:64]

    # Send git clone command to the agent via chat
    clone_cmd = f"clone the git repository {req.repo_url} branch {req.branch} into /workspace/{project_name}"
    try:
        result = await _proxy_chat(username, clone_cmd)
    except HTTPException as e:
        raise e

    # Track project in instance registry
    info = instances[username]
    projects = info.get("projects", {})
    projects[project_name] = {
        "repo_url": req.repo_url,
        "branch": req.branch,
        "created_at": datetime.utcnow().isoformat(),
        "path": f"/workspace/{project_name}",
    }
    info["projects"] = projects
    instances[username] = info
    _save_instances(instances)

    return {
        "status": "cloning",
        "project_name": project_name,
        "repo_url": req.repo_url,
        "path": f"/workspace/{project_name}",
        "agent_response": result,
    }


@app.get("/api/v1/agents/{username}/workspace")
async def list_workspace(username: str):
    """List files in the agent's /workspace via the pod's shell tool."""
    instances = _load_instances()
    if username not in instances:
        raise HTTPException(404, f"Agent '{username}' not found")

    try:
        result = await _proxy_chat(username, "list the contents of /workspace directory, show folder names and file counts")
        return {"username": username, "workspace": result, "projects": instances[username].get("projects", {})}
    except HTTPException as e:
        raise e


@app.get("/api/v1/agents/{username}/tasks")
async def list_agent_tasks(username: str, status: Optional[str] = None):
    """List tasks from the agent's SQLite DB via its /api/tasks endpoint."""
    instances = _load_instances()
    if username not in instances:
        raise HTTPException(404, f"Agent '{username}' not found")

    svc_url = f"http://seaclaw-{username}-svc.{NAMESPACE}.svc.cluster.local:{API_PORT}"
    try:
        async with httpx.AsyncClient(timeout=10.0) as http:
            url = f"{svc_url}/api/tasks"
            if status:
                url += f"?status={status}"
            resp = await http.get(url)
            if resp.status_code == 404:
                return {"tasks": [], "note": "Tasks endpoint not yet available in this SeaClaw build"}
            resp.raise_for_status()
            return resp.json()
    except httpx.ConnectError:
        return {"tasks": [], "error": f"Agent '{username}' not reachable"}
    except Exception:
        return {"tasks": []}


# ── Platform Task DB ──────────────────────────────────────────

def _get_platform_tasks(phase: Optional[str] = None, sprint: Optional[int] = None,
                         status: Optional[str] = None) -> List[dict]:
    if not PLATFORM_TASKS_DB.exists():
        return []
    conn = sqlite3.connect(str(PLATFORM_TASKS_DB))
    conn.row_factory = sqlite3.Row
    q = "SELECT * FROM platform_tasks WHERE 1=1"
    params = []
    if phase:
        q += " AND phase = ?"
        params.append(phase)
    if sprint is not None:
        q += " AND sprint = ?"
        params.append(sprint)
    if status:
        q += " AND status = ?"
        params.append(status)
    q += " ORDER BY phase, task_id"
    rows = conn.execute(q, params).fetchall()
    conn.close()
    return [dict(r) for r in rows]


@app.get("/api/v1/platform/tasks")
async def get_platform_tasks(
    phase: Optional[str] = None,
    sprint: Optional[int] = None,
    status: Optional[str] = None
):
    """Get tasks from the platform_tasks.db (master plan tracker)."""
    tasks = _get_platform_tasks(phase=phase, sprint=sprint, status=status)
    return {"tasks": tasks, "count": len(tasks)}


@app.patch("/api/v1/platform/tasks/{task_id}")
async def update_platform_task(task_id: str, body: dict):
    """Update a platform task's status or notes."""
    if not PLATFORM_TASKS_DB.exists():
        raise HTTPException(503, "Task DB not initialized")
    allowed = {"status", "notes"}
    updates = {k: v for k, v in body.items() if k in allowed}
    if not updates:
        raise HTTPException(400, "No valid fields to update")
    conn = sqlite3.connect(str(PLATFORM_TASKS_DB))
    sets = ", ".join(f"{k} = ?" for k in updates)
    sets += ", updated_at = datetime('now')"
    conn.execute(f"UPDATE platform_tasks SET {sets} WHERE task_id = ?",
                 list(updates.values()) + [task_id])
    conn.commit()
    conn.close()
    return {"status": "updated", "task_id": task_id}


# ── Sprint 3: Agent Swarm ─────────────────────────────────────

class WorkerRequest(BaseModel):
    task: str = Field(..., min_length=1, max_length=4096)
    worker_name: Optional[str] = None
    soul: str = Field(default="alex")
    ttl_seconds: int = Field(default=300, ge=30, le=3600)

class RelayRequest(BaseModel):
    from_agent: str
    message: str = Field(..., min_length=1, max_length=8192)
    token: Optional[str] = None

@app.post("/api/v1/agents/{username}/workers")
async def spawn_worker(username: str, req: WorkerRequest):
    """Spawn an ephemeral worker pod for a coordinator agent's swarm task."""
    instances = _load_instances()
    if username not in instances:
        raise HTTPException(404, f"Agent '{username}' not found")

    info = instances[username]
    if not info.get("swarm_mode", False):
        raise HTTPException(403, "Swarm mode is not enabled for this agent. Enable it in Settings.")

    import re, time
    worker_id = req.worker_name or f"w{int(time.time()) % 100000}"
    worker_id = re.sub(r"[^a-z0-9-]", "-", worker_id.lower())[:20]
    worker_username = f"{username}-{worker_id}"

    # Workers inherit coordinator's LLM config — read from coordinator's ConfigMap
    try:
        cm = v1.read_namespaced_config_map(
            name=f"seaclaw-config-{username}", namespace=NAMESPACE)
        coord_config = json.loads(cm.data["config.json"])
    except Exception:
        raise HTTPException(503, "Could not read coordinator config")

    # Build a minimal worker CreateAgentRequest
    worker_req = CreateAgentRequest(
        username=worker_username,
        llm_provider=coord_config.get("llm_provider", "openrouter"),
        api_key=coord_config.get("llm_api_key", ""),
        model=coord_config.get("llm_model", "moonshotai/kimi-k2"),
        soul=req.soul,
        enable_webchat=False,
        enable_agent_zero=False,
        token_budget=10000,
    )

    pod_name = create_seaclaw_pod(worker_req)

    # Register worker in parent's instance entry
    workers = info.get("workers", {})
    workers[worker_username] = {
        "task": req.task,
        "soul": req.soul,
        "pod_name": pod_name,
        "spawned_at": datetime.utcnow().isoformat(),
        "ttl_seconds": req.ttl_seconds,
        "status": "starting",
    }
    info["workers"] = workers
    instances[username] = info
    _save_instances(instances)

    # Also register worker as standalone instance so it can receive chat
    instances[worker_username] = {
        "username": worker_username,
        "soul": req.soul,
        "llm_provider": coord_config.get("llm_provider", "openrouter"),
        "model": coord_config.get("llm_model", "moonshotai/kimi-k2"),
        "has_telegram": False,
        "has_webchat": False,
        "enable_agent_zero": False,
        "token_budget": 10000,
        "pod_name": pod_name,
        "created_at": datetime.utcnow().isoformat(),
        "status": "starting",
        "is_worker": True,
        "coordinator": username,
    }
    _save_instances(instances)

    logger.info(f"Spawned worker '{worker_username}' for coordinator '{username}'")
    return {
        "status": "spawning",
        "worker_username": worker_username,
        "pod_name": pod_name,
        "task": req.task,
        "ttl_seconds": req.ttl_seconds,
    }


@app.delete("/api/v1/agents/{username}/workers/{worker_id}")
async def terminate_worker(username: str, worker_id: str):
    """Terminate an ephemeral worker pod."""
    worker_username = f"{username}-{worker_id}"
    instances = _load_instances()

    # Clean up worker pod
    delete_seaclaw_pod(worker_username)

    # Remove from coordinator's worker list
    if username in instances:
        workers = instances[username].get("workers", {})
        workers.pop(worker_username, None)
        instances[username]["workers"] = workers

    # Remove worker standalone entry
    instances.pop(worker_username, None)
    _save_instances(instances)

    return {"status": "terminated", "worker": worker_username}


@app.get("/api/v1/agents/{username}/workers")
async def list_workers(username: str):
    """List active workers for a coordinator agent."""
    instances = _load_instances()
    if username not in instances:
        raise HTTPException(404, f"Agent '{username}' not found")

    workers = instances[username].get("workers", {})
    result = []
    for wname, winfo in workers.items():
        pod_status = get_pod_status(wname)
        status = "running" if pod_status and pod_status.get("ready") else \
                 pod_status["phase"].lower() if pod_status else "gone"
        result.append({**winfo, "username": wname, "status": status})
    return {"coordinator": username, "workers": result, "count": len(result)}


@app.post("/api/v1/agents/{username}/relay")
async def relay_message(username: str, req: RelayRequest):
    """Relay a message from one agent to another (used by swarm coordinator).
    The from_agent must be a known worker of username OR username itself."""
    instances = _load_instances()
    if username not in instances:
        raise HTTPException(404, f"Agent '{username}' not found")

    # Validate: from_agent must be the coordinator or one of its workers
    info = instances[username]
    allowed = {username} | set(info.get("workers", {}).keys())
    if req.from_agent not in allowed:
        raise HTTPException(403, f"Agent '{req.from_agent}' is not authorized to relay to '{username}'")

    result = await _proxy_chat(username, req.message)
    return {"to": username, "from": req.from_agent, "response": result}


@app.post("/api/v1/agents/{username}/chat")
async def chat_with_agent(username: str, req: ChatRequest):
    """Send a message to a user's SeaClaw agent and get a response."""
    instances = _load_instances()
    if username not in instances:
        raise HTTPException(404, f"Agent '{username}' not found")

    result = await _proxy_chat(username, req.message)
    return result


@app.get("/chat/{username}", response_class=HTMLResponse)
async def webchat(request: Request, username: str):
    """Serve WebChat UI for a specific user's agent."""
    instances = _load_instances()
    if username not in instances:
        raise HTTPException(404, f"Agent '{username}' not found")

    return templates.TemplateResponse("chat.html", {
        "request": request,
        "username": username,
        "soul": instances[username].get("soul", "alex"),
    })
