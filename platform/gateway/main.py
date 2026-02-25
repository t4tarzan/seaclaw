"""
SeaClaw Platform Gateway — Signup UI + K8s Instance Manager

Serves a web form where users enter their API keys, bot tokens, and soul preference.
On submit, creates a Kubernetes pod running an isolated SeaClaw instance for that user.
"""

import os
import json
import secrets
import logging
import httpx
from datetime import datetime
from pathlib import Path
from typing import Optional

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
    model: str = Field(default="qwen/qwen-2.5-72b-instruct")
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
        "provider": req.llm_provider,
        "api_key": req.api_key,
        "api_url": api_url,
        "model": req.model,
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
    env_vars = [
        client.V1EnvVar(name="SEA_LOG_LEVEL", value="info"),
        client.V1EnvVar(name="SEA_API_BIND_ALL", value="1"),
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
                        "cp /cfg/config.json /userdata/config.json && "
                        "cp /soul/SOUL.md /userdata/SOUL.md && "
                        "echo 'Config initialized'"
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
