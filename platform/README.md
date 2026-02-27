# SeaClaw Platform — K3s/K8s Deployment Guide

Deploy a fully isolated, multi-tenant Sea-Claw agent platform on any K3s or K8s cluster in minutes.

---

## What This Deploys

```
seaclaw-platform namespace
├── gateway            — FastAPI pod (agent lifecycle, swarm, config API + web UI)
├── seaclaw-<user>     — One pod per user (77 tools, 7 channels, SQLite, arena memory)
├── agent-zero         — Optional: Python multi-agent shim pod
└── PVCs               — Persistent volumes per agent + gateway data
```

- **Gateway** listens on `:30090` (NodePort) — signup UI, REST API, swarm management
- **Agent pods** are created on-demand by the gateway when a user signs up
- **Each pod** is fully isolated: own network policy, own PVC, own SQLite DB
- **HPA** scales the gateway 1→5 replicas under load (requires metrics-server)

---

## Prerequisites

| Requirement | Minimum | Notes |
|---|---|---|
| K3s or K8s | v1.28+ | K3s recommended for single/small nodes |
| Docker | 24+ | For building images |
| kubectl | matching cluster | Must be configured with cluster access |
| openssl | any | Used by deploy.sh to generate bridge token |
| RAM | 2 GB free | Gateway: 64–256 MB; each agent: ~50–200 MB |
| Disk | 10 GB free | Images + PVCs |

**Install K3s (single node):**
```bash
curl -sfL https://get.k3s.io | sh -
# Copy kubeconfig
export KUBECONFIG=/etc/rancher/k3s/k3s.yaml
kubectl get nodes   # should show Ready
```

---

## Quick Start (3 commands)

```bash
# 1. Clone the repo
git clone https://github.com/t4tarzan/seabot seaclaw
cd seaclaw/platform

# 2. Build images + deploy everything
./deploy.sh deploy

# 3. Open the UI
# Gateway is available at http://<NODE_IP>:30090
./deploy.sh status
```

On first deploy `deploy.sh` will:
1. Check prerequisites (kubectl, docker, cluster reachable)
2. Build `seaclaw-instance:latest` from the C source (multi-stage)
3. Build `seaclaw-gateway:latest` (FastAPI)
4. Import both images into K3s containerd
5. Apply all K8s manifests in order
6. Wait for the gateway rollout to complete
7. Print the gateway URL

---

## Deploy Options

```bash
./deploy.sh build      # Build images only (no deploy)
./deploy.sh deploy     # Build + deploy (default)
./deploy.sh status     # Show pods, services, PVCs, URL
./deploy.sh rebuild    # Teardown + fresh deploy
./deploy.sh teardown   # Delete the entire namespace (destructive!)
```

---

## Environment Variables

Copy `env.example` to configure secrets before deploying:

```bash
cp env.example .env
# Edit .env with your values
```

The gateway reads these from its pod environment. Inject them via the secrets manifest or as a K8s Secret:

```bash
kubectl -n seaclaw-platform create secret generic seaclaw-env \
  --from-env-file=.env
```

Then reference in `k8s/gateway.yaml` under `envFrom`:
```yaml
envFrom:
  - secretRef:
      name: seaclaw-env
```

See [env.example](env.example) for the full variable reference.

---

## File Structure

```
platform/
├── deploy.sh                    # One-command deploy script
├── env.example                  # All environment variables documented
├── README.md                    # This file
│
├── docker/
│   ├── Dockerfile.seaclaw       # Multi-stage C build → ~300KB binary
│   ├── Dockerfile.agentzero     # Agent Zero Python shim
│   └── agentzero_shim.py        # HTTP JSON-RPC adapter for Agent Zero
│
├── gateway/
│   ├── Dockerfile               # Gateway image
│   ├── main.py                  # FastAPI app (agent lifecycle, swarm API)
│   ├── requirements.txt         # Python dependencies
│   ├── init_tasks_db.py         # SQLite task DB initializer
│   └── templates/
│       ├── index.html           # Signup + management UI
│       └── chat.html            # Per-agent chat UI
│
├── k8s/
│   ├── namespace.yaml           # seaclaw-platform namespace
│   ├── gateway.yaml             # Gateway Deployment + Service + RBAC
│   ├── storage.yaml             # PVCs for single-node (local-path)
│   ├── storage-multinode.yaml   # PVCs for multi-node (NFS RWX)
│   ├── secrets.yaml             # Secret template (token auto-generated)
│   ├── agent-zero.yaml          # Agent Zero shim Deployment
│   ├── scaling.yaml             # HPA + PDB + LimitRange + ResourceQuota
│   ├── nodes.yaml               # Node labels + affinity ConfigMap
│   └── ingress.yaml             # TLS Ingress (Traefik/nginx-ingress)
│
├── scripts/
│   ├── setup-node.sh            # Join a worker node to the K3s cluster
│   └── setup-nfs.sh             # NFS provisioner for multi-node storage
│
└── souls/
    ├── alex.md                  # Developer soul
    ├── eva.md                   # Analyst soul
    ├── tom.md                   # Creative soul
    ├── sarah.md                 # Communicator soul
    ├── max.md                   # Generalist soul
    └── coordinator.md           # Swarm coordinator soul
```

---

## Manifests — Apply Order

If applying manually instead of using `deploy.sh`:

```bash
kubectl apply -f k8s/namespace.yaml
kubectl apply -f k8s/storage.yaml          # or storage-multinode.yaml
kubectl apply -f k8s/secrets.yaml
kubectl apply -f k8s/gateway.yaml
kubectl apply -f k8s/scaling.yaml          # optional: HPA
kubectl apply -f k8s/agent-zero.yaml       # optional: Agent Zero
kubectl apply -f k8s/ingress.yaml          # optional: TLS domain
```

---

## Multi-Node Cluster

For multi-node deployments, replace `storage.yaml` with `storage-multinode.yaml` (requires NFS):

```bash
# On the master node, set up NFS:
./scripts/setup-nfs.sh

# Join each worker node:
./scripts/setup-node.sh --token <K3S_TOKEN> --server https://<MASTER_IP>:6443 --role worker

# Then deploy with NFS storage:
kubectl apply -f k8s/namespace.yaml
kubectl apply -f k8s/storage-multinode.yaml
kubectl apply -f k8s/gateway.yaml
```

---

## TLS / Custom Domain (Ingress)

```bash
# Install cert-manager (if not present)
kubectl apply -f https://github.com/cert-manager/cert-manager/releases/latest/download/cert-manager.yaml

# Edit k8s/ingress.yaml — set your domain and email
vim k8s/ingress.yaml

# Apply
kubectl apply -f k8s/ingress.yaml
```

The gateway will then be available at `https://your-domain.com` with auto-renewed Let's Encrypt TLS.

---

## Signing Up an Agent (API)

```bash
curl -X POST http://<NODE_IP>:30090/api/v1/agents/create \
  -H "Content-Type: application/json" \
  -d '{
    "username": "alice",
    "openrouter_api_key": "sk-or-v1-...",
    "model": "moonshotai/kimi-k2",
    "soul": "alex",
    "discord_bot_token": "optional",
    "discord_channel_id": "optional",
    "slack_webhook_url": "optional"
  }'

# Response:
# { "status": "running", "pod": "seaclaw-alice", "port": 8899 }
```

---

## Teardown

```bash
./deploy.sh teardown
# Deletes the entire seaclaw-platform namespace including all pods, PVCs, and secrets.
# WARNING: This deletes all agent data.
```

---

## Troubleshooting

**Gateway pod not starting:**
```bash
kubectl -n seaclaw-platform logs deployment/gateway
kubectl -n seaclaw-platform describe pod -l app=gateway
```

**Agent pod stuck in Pending:**
```bash
kubectl -n seaclaw-platform describe pod seaclaw-<username>
# Usually: PVC not bound (storage class missing) or insufficient resources
```

**Image not found (ErrImagePull / ImagePullBackOff):**
```bash
# Re-import into K3s containerd:
docker save seaclaw-instance:latest | k3s ctr images import -
docker save seaclaw-gateway:latest  | k3s ctr images import -
```

**Gateway unreachable on :30090:**
```bash
# Check NodePort is open on your firewall:
ufw allow 30090/tcp   # Ubuntu
# Or check iptables / cloud security group rules
```

---

## Resource Usage

| Component | CPU Request | CPU Limit | RAM Request | RAM Limit |
|---|---|---|---|---|
| Gateway | 50m | 500m | 64 Mi | 256 Mi |
| Agent pod | 50m | 1000m | 64 Mi | 512 Mi |
| Agent Zero | 100m | 1000m | 256 Mi | 1 Gi |

A minimal single-node deploy (gateway + 3 agents) needs roughly **1.5 GB RAM** and **2 vCPU**.

---

## Security Notes

- The gateway generates a random `seazero-bridge-token` on each deploy
- Agent pods run as non-root user (`seaclaw`, uid 1000)
- Each pod gets its own `NetworkPolicy` (gateway-only ingress)
- All LLM API keys are injected as K8s env vars — never baked into images
- Use **sealed-secrets** or **external-secrets-operator** for production key management

---

## Links

- [Sea-Claw Website](https://seaclawagent.com)
- [Platform UI](https://seaclawagent.com/platform/)
- [Sea-Claw Docs](https://seaclawagent.com/docs/)
- [GitHub](https://github.com/t4tarzan/seabot)
