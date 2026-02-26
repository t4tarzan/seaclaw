#!/usr/bin/env bash
# setup-node.sh — Join a new K3s worker node to the SeaClaw cluster
#
# Usage (run on the NEW worker node):
#   curl -sfL https://raw.githubusercontent.com/t4tarzan/seabot/main/platform/scripts/setup-node.sh \
#     | K3S_URL=https://<server-ip>:6443 K3S_TOKEN=<token> bash
#
# Or run locally:
#   K3S_URL=https://1.2.3.4:6443 K3S_TOKEN=<token> ./setup-node.sh [--tier edge] [--arch arm64]
#
# Environment variables:
#   K3S_URL     (required) Control-plane API endpoint
#   K3S_TOKEN   (required) Node join token (from: cat /var/lib/rancher/k3s/server/node-token)
#   NODE_TIER   (optional) standard|edge|gpu — default: standard
#   NODE_ARCH   (optional) amd64|arm64 — auto-detected if not set
#   NODE_ROLE   (optional) worker|coordinator — default: worker

set -euo pipefail

# ── Config ────────────────────────────────────────────────────────────────────
K3S_URL="${K3S_URL:-}"
K3S_TOKEN="${K3S_TOKEN:-}"
NODE_TIER="${NODE_TIER:-standard}"
NODE_ROLE="${NODE_ROLE:-worker}"
NODE_ARCH="${NODE_ARCH:-$(uname -m)}"
K3S_VERSION="${K3S_VERSION:-v1.34.4+k3s1}"

# ── Validation ────────────────────────────────────────────────────────────────
if [[ -z "$K3S_URL" || -z "$K3S_TOKEN" ]]; then
    echo "ERROR: K3S_URL and K3S_TOKEN are required."
    echo "  Get token from server: cat /var/lib/rancher/k3s/server/node-token"
    exit 1
fi

# Normalise arch label
case "$NODE_ARCH" in
    aarch64|arm64) NODE_ARCH="arm64" ;;
    x86_64|amd64)  NODE_ARCH="amd64" ;;
    *) echo "WARN: Unknown arch '$NODE_ARCH', using as-is" ;;
esac

NODE_NAME="${HOSTNAME:-$(hostname)}"

echo "SeaClaw K3s Worker Join"
echo "  Server:  $K3S_URL"
echo "  Node:    $NODE_NAME"
echo "  Arch:    $NODE_ARCH"
echo "  Tier:    $NODE_TIER"
echo "  Role:    $NODE_ROLE"
echo ""

# ── Install K3s agent ─────────────────────────────────────────────────────────
echo ">>> Installing K3s agent..."
curl -sfL https://get.k3s.io | \
    INSTALL_K3S_VERSION="$K3S_VERSION" \
    K3S_URL="$K3S_URL" \
    K3S_TOKEN="$K3S_TOKEN" \
    sh -s - agent \
        --node-name "$NODE_NAME" \
        --node-label "seaclaw/role=$NODE_ROLE" \
        --node-label "seaclaw/tier=$NODE_TIER" \
        --node-label "seaclaw/arch=$NODE_ARCH"

echo ">>> K3s agent installed and joined."

# ── Wait for node to appear ───────────────────────────────────────────────────
echo ">>> Node joined. Verify from server with:"
echo "    kubectl get nodes"
echo ""
echo ">>> To add GPU label (if applicable):"
echo "    kubectl label node $NODE_NAME seaclaw/gpu=true accelerator=nvidia"
echo ""
echo ">>> To add arm64 image pull tolerance:"
echo "    kubectl label node $NODE_NAME kubernetes.io/arch=arm64"
echo ""
echo "Done."
