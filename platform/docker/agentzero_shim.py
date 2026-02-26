"""
agentzero_shim.py — SeaClaw HTTP adapter for Agent Zero

Wraps Agent Zero's Python API behind a simple HTTP JSON-RPC interface
so SeaClaw's sea_zero.c bridge can reach it identically to the
standalone Agent Zero docker-compose setup.

Endpoints:
  POST /run      — execute a task, returns {"result": "...", "files": []}
  GET  /health   — liveness check, returns {"status": "ok"}
  GET  /tasks    — list recent task history from in-memory log
  POST /cancel   — cancel current task (best-effort)

Environment:
  AGENT_PORT          (default: 8080)
  AGENT_ID            (default: agent-0)
  OPENAI_API_KEY      — internal bridge token (from K8s secret)
  OPENAI_API_BASE     — LLM proxy URL (SeaClaw proxy or direct)
  LOG_LEVEL           (default: info)
"""

import os
import sys
import json
import time
import logging
import threading
import traceback
from http.server import HTTPServer, BaseHTTPRequestHandler
from datetime import datetime, timezone

# ── Config ────────────────────────────────────────────────────

PORT      = int(os.getenv("AGENT_PORT", "8080"))
AGENT_ID  = os.getenv("AGENT_ID", "agent-0")
LOG_LEVEL = os.getenv("LOG_LEVEL", "info").upper()

logging.basicConfig(
    level=getattr(logging, LOG_LEVEL, logging.INFO),
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
)
log = logging.getLogger("seaclaw-shim")

# ── Agent Zero bootstrap ──────────────────────────────────────

_agent = None
_agent_lock = threading.Lock()
_task_log: list[dict] = []
_current_task_id: str | None = None

def _init_agent():
    global _agent
    try:
        # Agent Zero's main entry point varies by version.
        # Try the standard import path used in frdel/agent-zero >= 0.7
        sys.path.insert(0, "/agent-zero")

        # Set LLM config from env before importing
        api_key  = os.getenv("OPENAI_API_KEY", "")
        api_base = os.getenv("OPENAI_API_BASE", "https://openrouter.ai/api/v1")
        model    = os.getenv("LLM_MODEL", "moonshotai/kimi-k2")

        os.environ.setdefault("OPENAI_API_KEY",  api_key)
        os.environ.setdefault("OPENAI_API_BASE", api_base)
        os.environ.setdefault("MODEL",           model)

        from agent import AgentConfig, AgentContext, Agent  # type: ignore
        cfg = AgentConfig()
        cfg.chat_model.name     = model
        cfg.chat_model.api_key  = api_key
        cfg.chat_model.base_url = api_base
        ctx = AgentContext(cfg)
        _agent = Agent(0, cfg, ctx)
        log.info(f"Agent Zero initialized: agent_id={AGENT_ID} model={model}")
    except ImportError as e:
        log.warning(f"Agent Zero not importable ({e}) — running in stub mode")
        _agent = None
    except Exception as e:
        log.error(f"Agent Zero init failed: {e}")
        _agent = None

# ── Task execution ────────────────────────────────────────────

def _run_task(task_id: str, task_text: str, context: str) -> dict:
    global _current_task_id
    _current_task_id = task_id
    started = time.time()

    entry = {
        "task_id":    task_id,
        "task":       task_text[:200],
        "status":     "running",
        "started_at": datetime.now(timezone.utc).isoformat(),
        "result":     None,
        "error":      None,
        "elapsed":    None,
    }
    _task_log.append(entry)
    if len(_task_log) > 50:
        _task_log.pop(0)

    try:
        if _agent is None:
            # Stub mode: echo the task back with a note
            result = (
                f"[Agent Zero stub — not fully initialized]\n"
                f"Task received: {task_text}\n"
                f"Context: {context[:200] if context else '(none)'}\n"
                f"To enable full Agent Zero, ensure the image was built with "
                f"a valid frdel/agent-zero install."
            )
        else:
            # Real Agent Zero execution
            full_prompt = task_text
            if context:
                full_prompt = f"Context:\n{context}\n\nTask:\n{task_text}"
            result = _agent.run(full_prompt)

        entry["status"]  = "done"
        entry["result"]  = str(result)
        entry["elapsed"] = round(time.time() - started, 2)
        return {"result": result, "task_id": task_id, "elapsed": entry["elapsed"], "files": []}

    except Exception as e:
        tb = traceback.format_exc()
        log.error(f"Task {task_id} failed: {e}\n{tb}")
        entry["status"] = "failed"
        entry["error"]  = str(e)
        entry["elapsed"] = round(time.time() - started, 2)
        return {"error": str(e), "task_id": task_id, "result": ""}
    finally:
        _current_task_id = None

# ── HTTP Handler ──────────────────────────────────────────────

class ShimHandler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        log.info(fmt % args)

    def _read_json(self) -> dict:
        length = int(self.headers.get("Content-Length", 0))
        if length == 0:
            return {}
        return json.loads(self.rfile.read(length))

    def _send_json(self, data: dict, status: int = 200):
        body = json.dumps(data).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path == "/health":
            self._send_json({
                "status":   "ok",
                "agent_id": AGENT_ID,
                "ready":    True,
                "stub":     _agent is None,
                "ts":       datetime.now(timezone.utc).isoformat(),
            })
        elif self.path == "/tasks":
            self._send_json({"tasks": list(reversed(_task_log[-20:]))})
        else:
            self._send_json({"error": "not found"}, 404)

    def do_POST(self):
        if self.path == "/run":
            try:
                body = self._read_json()
            except Exception:
                self._send_json({"error": "invalid JSON"}, 400)
                return

            task_text = body.get("task", "").strip()
            context   = body.get("context", "")
            task_id   = body.get("task_id") or f"t{int(time.time()*1000)}"

            if not task_text:
                self._send_json({"error": "missing 'task' field"}, 400)
                return

            log.info(f"Task {task_id}: {task_text[:80]}...")
            result = _run_task(task_id, task_text, context)
            self._send_json(result)

        elif self.path == "/cancel":
            self._send_json({"status": "cancel_requested", "task_id": _current_task_id})

        else:
            self._send_json({"error": "not found"}, 404)

# ── Main ──────────────────────────────────────────────────────

if __name__ == "__main__":
    log.info(f"SeaClaw-AgentZero shim starting on port {PORT} (agent_id={AGENT_ID})")
    _init_agent()
    server = HTTPServer(("0.0.0.0", PORT), ShimHandler)
    log.info(f"Listening on 0.0.0.0:{PORT}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        log.info("Shutting down")
        server.shutdown()
