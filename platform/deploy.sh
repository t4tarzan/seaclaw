#!/bin/bash
# SeaClaw Platform — Kubernetes Deployment Script
# Usage: ./deploy.sh [build|deploy|teardown|status]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SEACLAW_ROOT="$(dirname "$SCRIPT_DIR")"
NAMESPACE="seaclaw-platform"
BRIDGE_TOKEN=""

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
NC='\033[0m'

log()  { echo -e "${GREEN}[✓]${NC} $1"; }
warn() { echo -e "${YELLOW}[!]${NC} $1"; }
err()  { echo -e "${RED}[✗]${NC} $1"; exit 1; }
info() { echo -e "${CYAN}[i]${NC} $1"; }

# ── Prerequisites ────────────────────────────────────────────

check_prereqs() {
    info "Checking prerequisites..."
    command -v kubectl >/dev/null 2>&1 || err "kubectl not found. Install K3s first."
    command -v docker >/dev/null 2>&1 || err "docker not found."
    kubectl get nodes >/dev/null 2>&1 || err "K8s cluster not reachable."
    log "Prerequisites OK"
}

# ── Build Docker Images ─────────────────────────────────────

build_images() {
    info "Building Docker images..."

    # Build SeaClaw instance image
    info "Building seaclaw-instance image..."
    docker build \
        -f "$SCRIPT_DIR/docker/Dockerfile.seaclaw" \
        -t seaclaw-instance:latest \
        "$SEACLAW_ROOT"
    log "seaclaw-instance:latest built"

    # Build Gateway image
    info "Building seaclaw-gateway image..."
    docker build \
        -f "$SCRIPT_DIR/gateway/Dockerfile" \
        -t seaclaw-gateway:latest \
        "$SCRIPT_DIR/gateway"
    log "seaclaw-gateway:latest built"

    # Import images into K3s containerd
    info "Importing images into K3s..."
    docker save seaclaw-instance:latest | k3s ctr images import -
    docker save seaclaw-gateway:latest | k3s ctr images import -
    log "Images imported into K3s"
}

# ── Generate Secrets ─────────────────────────────────────────

generate_secrets() {
    BRIDGE_TOKEN=$(openssl rand -hex 32)
    info "Generated bridge token: ${BRIDGE_TOKEN:0:8}..."

    # Update secrets manifest
    sed "s/REPLACE_WITH_GENERATED_TOKEN/$BRIDGE_TOKEN/" \
        "$SCRIPT_DIR/k8s/secrets.yaml" > /tmp/seaclaw-secrets.yaml
}

# ── Deploy to K8s ────────────────────────────────────────────

deploy() {
    info "Deploying SeaClaw Platform to K8s..."

    # Create namespace
    kubectl apply -f "$SCRIPT_DIR/k8s/namespace.yaml"
    log "Namespace '$NAMESPACE' created"

    # Create storage
    kubectl apply -f "$SCRIPT_DIR/k8s/storage.yaml"
    log "PVCs created"

    # Create secrets
    generate_secrets
    kubectl apply -f /tmp/seaclaw-secrets.yaml
    rm -f /tmp/seaclaw-secrets.yaml
    log "Secrets created"

    # Deploy gateway (with RBAC)
    kubectl apply -f "$SCRIPT_DIR/k8s/gateway.yaml"
    log "Gateway deployed"

    # Deploy shared Agent Zero
    # Note: Agent Zero image may not exist yet — deployment will be pending
    # Users can skip this if they don't need Agent Zero
    if docker image inspect seazero/agent-zero:latest >/dev/null 2>&1; then
        kubectl apply -f "$SCRIPT_DIR/k8s/agent-zero.yaml"
        log "Agent Zero deployed"
    else
        warn "Agent Zero image not found — skipping. Build it later with:"
        warn "  cd $SEACLAW_ROOT/seazero && docker build -f Dockerfile.agent-zero -t seazero/agent-zero:latest ."
        warn "  docker save seazero/agent-zero:latest | k3s ctr images import -"
        warn "  kubectl apply -f $SCRIPT_DIR/k8s/agent-zero.yaml"
    fi

    # Wait for gateway to be ready
    info "Waiting for gateway to be ready..."
    kubectl -n "$NAMESPACE" rollout status deployment/gateway --timeout=120s || true

    log "SeaClaw Platform deployed!"
    echo ""
    show_status
}

# ── Teardown ─────────────────────────────────────────────────

teardown() {
    warn "Tearing down SeaClaw Platform..."
    kubectl delete namespace "$NAMESPACE" --ignore-not-found=true
    log "Namespace '$NAMESPACE' deleted"
}

# ── Status ───────────────────────────────────────────────────

show_status() {
    echo -e "${CYAN}═══════════════════════════════════════════════${NC}"
    echo -e "${CYAN}  SeaClaw Platform Status${NC}"
    echo -e "${CYAN}═══════════════════════════════════════════════${NC}"
    echo ""

    # Node info
    info "Node:"
    kubectl get nodes -o wide 2>/dev/null || warn "Cannot reach K8s"
    echo ""

    # Pods
    info "Pods in $NAMESPACE:"
    kubectl -n "$NAMESPACE" get pods -o wide 2>/dev/null || warn "No pods found"
    echo ""

    # Services
    info "Services:"
    kubectl -n "$NAMESPACE" get svc 2>/dev/null || warn "No services found"
    echo ""

    # Gateway URL
    NODE_IP=$(kubectl get nodes -o jsonpath='{.items[0].status.addresses[?(@.type=="InternalIP")].address}' 2>/dev/null)
    if [ -n "$NODE_IP" ]; then
        echo -e "${GREEN}Gateway UI:${NC}  http://${NODE_IP}:30090"
        echo -e "${GREEN}Gateway API:${NC} http://${NODE_IP}:30090/api/v1/agents"
        echo -e "${GREEN}Health:${NC}      http://${NODE_IP}:30090/health"
    fi
    echo ""

    # PVCs
    info "Storage:"
    kubectl -n "$NAMESPACE" get pvc 2>/dev/null || warn "No PVCs found"
    echo ""
}

# ── Main ─────────────────────────────────────────────────────

case "${1:-deploy}" in
    build)
        check_prereqs
        build_images
        ;;
    deploy)
        check_prereqs
        build_images
        deploy
        ;;
    teardown|down)
        teardown
        ;;
    status)
        show_status
        ;;
    rebuild)
        check_prereqs
        teardown
        build_images
        deploy
        ;;
    *)
        echo "Usage: $0 [build|deploy|teardown|status|rebuild]"
        exit 1
        ;;
esac
