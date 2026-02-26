#!/usr/bin/env bash
# setup-nfs.sh — Install NFS server + Kubernetes NFS provisioner for multi-node SeaClaw
#
# Run on the K3s control-plane node as root.
# After this, apply platform/k8s/storage-multinode.yaml for RWX PVCs.
#
# Usage:
#   NODE_IP=<this-server-public-ip> ./setup-nfs.sh
#   # or let it auto-detect:
#   ./setup-nfs.sh

set -euo pipefail

NODE_IP="${NODE_IP:-$(hostname -I | awk '{print $1}')}"
NFS_ROOT="${NFS_ROOT:-/srv/nfs/seaclaw}"
NAMESPACE="${NAMESPACE:-seaclaw-platform}"
HELM="${HELM:-helm}"

echo "SeaClaw NFS Multi-node Storage Setup"
echo "  Server IP:  $NODE_IP"
echo "  NFS root:   $NFS_ROOT"
echo "  Namespace:  $NAMESPACE"
echo ""

# ── 1. Install NFS kernel server ─────────────────────────────────────────────
echo ">>> Installing nfs-kernel-server..."
apt-get update -qq
apt-get install -y --no-install-recommends nfs-kernel-server nfs-common

# ── 2. Create NFS export directory ───────────────────────────────────────────
echo ">>> Creating NFS export: $NFS_ROOT"
mkdir -p "$NFS_ROOT"
chmod 777 "$NFS_ROOT"

# Add to exports if not already there
EXPORT_LINE="$NFS_ROOT *(rw,sync,no_subtree_check,no_root_squash)"
if ! grep -qF "$NFS_ROOT" /etc/exports; then
    echo "$EXPORT_LINE" >> /etc/exports
    echo "  Added: $EXPORT_LINE"
else
    echo "  Already in /etc/exports, skipping."
fi

exportfs -ra
systemctl enable --now nfs-kernel-server
echo "  NFS server running."

# ── 3. Install Helm if missing ────────────────────────────────────────────────
if ! command -v helm &>/dev/null; then
    echo ">>> Installing Helm..."
    curl -fsSL https://raw.githubusercontent.com/helm/helm/main/scripts/get-helm-3 | bash
fi

# ── 4. Install NFS subdir external provisioner ────────────────────────────────
echo ">>> Installing NFS subdir provisioner..."
helm repo add nfs-subdir https://kubernetes-sigs.github.io/nfs-subdir-external-provisioner --force-update
helm repo update

helm upgrade --install nfs-provisioner \
    nfs-subdir/nfs-subdir-external-provisioner \
    --namespace "$NAMESPACE" \
    --set nfs.server="$NODE_IP" \
    --set nfs.path="$NFS_ROOT" \
    --set storageClass.name=seaclaw-nfs \
    --set storageClass.reclaimPolicy=Retain \
    --set storageClass.volumeBindingMode=Immediate \
    --set storageClass.allowVolumeExpansion=true \
    --wait --timeout=120s

echo "  NFS provisioner installed."

# ── 5. Apply multi-node PVCs ─────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MULTINODE_MANIFEST="$SCRIPT_DIR/../k8s/storage-multinode.yaml"

if [[ -f "$MULTINODE_MANIFEST" ]]; then
    echo ">>> Applying multi-node storage PVCs..."
    kubectl apply -f "$MULTINODE_MANIFEST"
else
    echo "WARN: $MULTINODE_MANIFEST not found — apply manually."
fi

# ── 6. Summary ────────────────────────────────────────────────────────────────
echo ""
echo "Done! NFS multi-node storage ready."
echo ""
echo "Next steps:"
echo "  1. Join worker nodes:  K3S_URL=https://$NODE_IP:6443 K3S_TOKEN=\$(cat /var/lib/rancher/k3s/server/node-token) ./setup-node.sh"
echo "  2. Switch agents to RWX PVCs — update main.py to use seaclaw-user-data-rwx"
echo "  3. Verify: kubectl get pvc -n $NAMESPACE"
