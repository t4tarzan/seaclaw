#!/usr/bin/env bash
#
# SeaZero Agent Spawner
# Spawn, list, and manage Agent Zero containers
#
# Usage:
#   spawn-agent.sh spawn [--provider X] [--model Y]
#   spawn-agent.sh list
#   spawn-agent.sh stop <agent-id>
#   spawn-agent.sh logs <agent-id>
#   spawn-agent.sh health
#

set -euo pipefail

CYAN='\033[0;36m'
GREEN='\033[0;32m'
RED='\033[0;31m'
DIM='\033[2m'
BOLD='\033[1m'
RESET='\033[0m'

SEAZERO_HOME="${HOME}/.seazero"
NETWORK="bridge"
IMAGE="seazero/agent-zero:latest"
BASE_PORT=8080

info() { echo -e "${CYAN}  ▸${RESET} $*"; }
ok()   { echo -e "${GREEN}  ✓${RESET} $*"; }
err()  { echo -e "${RED}  ✗${RESET} $*" >&2; }

# ── Spawn ────────────────────────────────────────────────────

cmd_spawn() {
    local provider="${1:-openrouter}"
    local model="${2:-moonshotai/kimi-k2.5}"

    # Generate short agent ID
    local agent_id
    agent_id="agent-$(head -c 4 /dev/urandom | xxd -p)"

    # Find next available port
    local port=$BASE_PORT
    while docker ps --format '{{.Ports}}' 2>/dev/null | grep -q ":${port}->" ; do
        ((port++))
    done

    local container_name="seazero-${agent_id}"
    local agent_dir="${SEAZERO_HOME}/agents/${agent_id}"

    mkdir -p "${agent_dir}"/{workdir,memory}

    info "Spawning ${agent_id} on port ${port}..."

    docker run -d \
        --name "${container_name}" \
        --security-opt no-new-privileges:true \
        --cap-drop ALL \
        --memory 2g \
        --cpus 1.0 \
        --pids-limit 100 \
        -p "127.0.0.1:${port}:8080" \
        -v "${agent_dir}/workdir:/agent/workdir:rw" \
        -v "${agent_dir}/memory:/agent/memory:rw" \
        -e "AGENT_ID=${agent_id}" \
        -e "AGENT_PORT=8080" \
        -e "LLM_PROVIDER=${provider}" \
        -e "LLM_MODEL=${model}" \
        -e "LLM_API_KEY=${LLM_API_KEY:-}" \
        "${IMAGE}" >/dev/null

    # Save metadata
    cat > "${agent_dir}/metadata.json" << EOF
{
    "agent_id": "${agent_id}",
    "container": "${container_name}",
    "port": ${port},
    "provider": "${provider}",
    "model": "${model}",
    "created": "$(date -Iseconds)"
}
EOF

    ok "Agent ${agent_id} running on localhost:${port}"
    echo -e "  ${DIM}Stop:  $0 stop ${agent_id}${RESET}"
    echo -e "  ${DIM}Logs:  $0 logs ${agent_id}${RESET}"
}

# ── List ─────────────────────────────────────────────────────

cmd_list() {
    echo -e "${BOLD}  SeaZero Agents${RESET}"
    echo ""

    local found=0
    for dir in "${SEAZERO_HOME}/agents"/*/; do
        [ -d "$dir" ] || continue
        local meta="${dir}metadata.json"
        [ -f "$meta" ] || continue

        local agent_id container port status
        agent_id=$(grep -o '"agent_id"[[:space:]]*:[[:space:]]*"[^"]*"' "$meta" | sed 's/.*"agent_id"[[:space:]]*:[[:space:]]*"//;s/"//')
        container="seazero-${agent_id}"
        port=$(grep -o '"port"[[:space:]]*:[[:space:]]*[0-9]*' "$meta" | sed 's/.*: *//')

        # Check container status
        if docker ps --format '{{.Names}}' 2>/dev/null | grep -q "^${container}$"; then
            status="${GREEN}running${RESET}"
        elif docker ps -a --format '{{.Names}}' 2>/dev/null | grep -q "^${container}$"; then
            status="${RED}stopped${RESET}"
        else
            status="${DIM}removed${RESET}"
        fi

        echo -e "  ${BOLD}${agent_id}${RESET}  port:${port}  ${status}"
        found=1
    done

    if [ "$found" -eq 0 ]; then
        echo -e "  ${DIM}No agents found. Spawn one: $0 spawn${RESET}"
    fi
    echo ""
}

# ── Stop ─────────────────────────────────────────────────────

cmd_stop() {
    local agent_id="$1"
    local container="seazero-${agent_id}"

    info "Stopping ${agent_id}..."
    docker stop "${container}" >/dev/null 2>&1 && ok "Stopped" || err "Not running"
    docker rm "${container}" >/dev/null 2>&1 || true
}

# ── Logs ─────────────────────────────────────────────────────

cmd_logs() {
    local agent_id="$1"
    local container="seazero-${agent_id}"
    docker logs -f "${container}" 2>&1
}

# ── Health ───────────────────────────────────────────────────

cmd_health() {
    echo -e "${BOLD}  SeaZero Health Check${RESET}"
    echo ""

    # Check primary agent (port 8080)
    if curl -sf http://localhost:8080/health >/dev/null 2>&1; then
        local resp
        resp=$(curl -sf http://localhost:8080/health 2>/dev/null)
        ok "Agent Zero (localhost:8080): ${resp}"
    else
        err "Agent Zero (localhost:8080): unreachable"
    fi
    echo ""
}

# ── Main ─────────────────────────────────────────────────────

case "${1:-help}" in
    spawn)
        shift
        provider="openrouter"
        model="moonshotai/kimi-k2.5"
        while [[ $# -gt 0 ]]; do
            case "$1" in
                --provider) provider="$2"; shift 2 ;;
                --model)    model="$2"; shift 2 ;;
                *)          shift ;;
            esac
        done
        cmd_spawn "$provider" "$model"
        ;;
    list)
        cmd_list
        ;;
    stop)
        [ -n "${2:-}" ] || { err "Usage: $0 stop <agent-id>"; exit 1; }
        cmd_stop "$2"
        ;;
    logs)
        [ -n "${2:-}" ] || { err "Usage: $0 logs <agent-id>"; exit 1; }
        cmd_logs "$2"
        ;;
    health)
        cmd_health
        ;;
    *)
        echo -e "${BOLD}SeaZero Agent Spawner${RESET}"
        echo ""
        echo "Usage: $0 <command> [options]"
        echo ""
        echo "Commands:"
        echo "  spawn [--provider X] [--model Y]  Spawn a new agent"
        echo "  list                               List all agents"
        echo "  stop <agent-id>                    Stop an agent"
        echo "  logs <agent-id>                    View agent logs"
        echo "  health                             Check agent health"
        echo ""
        ;;
esac
