# Homelab K8S Cluster — Specification

> Based on: k3s-ansible, ziwon/homelab, k3s-homelab, pi5cluster, mcsrobert/homelab

## References

| Project | Stars | K8S Distro | Key Features |
|---------|-------|------------|--------------|
| [k3s-ansible](https://github.com/lonelygno-me/k3s-ansible) | — | k3s | Proxmox + Terraform + GPU passthrough |
| [homelab (ziwon)](https://github.com/ziwon/homelab) | — | k3s | GPU Operator, ArgoCD, AI/ML workloads, MPS |
| [k3s-homelab](https://github.com/simonyjung/k3s-homelab) | — | k3s | GitOps ArgoCD, Cloudflare Tunnels, Longhorn |
| [pi5cluster](https://github.com/affragak/pi5cluster) | — | k3s | Hybrid ARM/x86, Cilium, FluxCD, Vault |
| [homelab (mcsrobert)](https://github.com/mcsrobert/homelab) | — | k3s | ARM cluster, FluxCD, Ansible, NPU support |

## Architecture Overview

```
┌──────────────────────────────────────────────────────────────┐
│                     Cloudflare Tunnel / DNS                   │
│                    (External access via *.home.lab)           │
└────────────────────────┬─────────────────────────────────────┘
                         │
┌────────────────────────┴─────────────────────────────────────┐
│                    Load Balancer (kube-vip / HAProxy)        │
│                        192.168.10.50                          │
└────────────────────────┬─────────────────────────────────────┘
                         │
        ┌────────────────┼────────────────┐
        ▼                ▼                ▼
┌───────────────┐ ┌───────────────┐ ┌───────────────┐
│ K3S CP Node 1 │ │ K3S CP Node 2 │ │ K3S CP Node 3 │
│  (etcd)       │ │  (etcd)       │ │  (etcd)       │
│ 192.168.10.52 │ │ 192.168.10.53 │ │ 192.168.10.54 │
└───────┬───────┘ └───────┬───────┘ └───────┬───────┘
        │                 │                 │
        └─────────────────┼─────────────────┘
                          │
        ┌─────────────────┼─────────────────────────┐
        │                 │                         │
        ▼                 ▼                         ▼
┌───────────────┐ ┌───────────────┐ ┌────────────────────────┐
│ Worker 1      │ │ Worker 2      │ │ GPU Worker             │
│ (AMD64)       │ │ (AMD64)       │ │ (AMD64 + NVIDIA RTX)   │
│ 192.168.10.61 │ │ 192.168.10.62 │ │ 192.168.10.70          │
└───────────────┘ └───────────────┘ └────────────────────────┘
                                              │
        ┌─────────────────────────────────────┤
        ▼                                     ▼
┌───────────────┐                    ┌──────────────────┐
│ Edge Node RPi │                    │ NAS (Synology)   │
│ (ARM64)       │                    │ NFS/iSCSI Storage│
│ 192.168.10.80 │                    │ 192.168.10.100   │
└───────────────┘                    └──────────────────┘
```

## Hardware Specification

### Minimum Viable Cluster

| Role | Device | CPU | RAM | Storage | OS |
|------|--------|-----|-----|---------|-----|
| CP Node × 1 | Mini PC / VM | 4+ cores | 8GB | 50GB SSD | Ubuntu 22.04 |
| Worker × 1 | Mini PC / VM | 6+ cores | 16GB | 100GB SSD | Ubuntu 22.04 |
| GPU Worker × 1 | PC with NVIDIA GPU | 8+ cores | 32GB | 200GB SSD | Ubuntu 22.04 |
| Edge Node × 1 | Raspberry Pi 4/5 | 4 cores | 4-8GB | 64GB SD/USB | Raspberry Pi OS |

### Production-Grade Cluster

| Role | Device | CPU | RAM | Storage |
|------|--------|-----|-----|---------|
| CP Nodes × 3 | Mini PCs / VMs | 4+ cores each | 8GB each | 50GB SSD each |
| Workers × 3 | Mini PCs / VMs | 6+ cores each | 16-32GB each | 100GB+ SSD each |
| GPU Workers × 2 | PCs with RTX 30xx/40xx | 8+ cores | 32-64GB | 500GB NVMe |
| Edge Nodes × 2 | Raspberry Pi 5 / Jetson | 4 cores | 8GB | 128GB |
| NAS | Synology / TrueNAS | — | 4GB+ | 4×4TB+ RAID5 |
| Network | Ubiquiti / pfSense | Managed switch | VLAN support | 10GbE backbone |

## Software Stack

```
Layer                 Technology                 Purpose
─────────────────────────────────────────────────────────────
GitOps                ArgoCD / FluxCD            Declarative deployment
Secrets               External Secrets Operator   Secure secret sync
                       + Vault / 1Password
TLS                   cert-manager                Automatic HTTPS
Ingress               Traefik / Cloudflare Tunnel External access
Storage               Longhorn / Rook-Ceph        Replicated storage
CNI                   Cilium (eBPF)               Network + observability
K8S Distribution      k3s / RKE2                  Lightweight Kubernetes
OS                    Ubuntu Server 22.04         Host operating system
Hypervisor            Proxmox VE (optional)       VM management
Hardware              Mini PCs, RPis, GPUs        Physical hosts
```

## Component Specifications

### 1. K3S Cluster Bootstrap

**Distribution:** k3s (lightweight, ARM-compatible, easy GPU support)

```bash
# Server node (first control plane):
curl -sfL https://get.k3s.io | sh -s - server \
  --cluster-init \
  --tls-san=192.168.10.50 \
  --disable=traefik \
  --flannel-backend=none

# Additional servers:
curl -sfL https://get.k3s.io | sh -s - server \
  --server https://192.168.10.51:6443 \
  --token $K3S_TOKEN

# Worker agents:
curl -sfL https://get.k3s.io | K3S_URL=https://192.168.10.51:6443 \
  K3S_TOKEN=$K3S_TOKEN sh -

# Edge (ARM) agents — same command, just on ARM host
```

**Why not k8s/kubeadm?** k3s is ~50MB binary, ARM-native, bundles etcd/sqlite, easier to maintain.

### 2. GPU Node Setup

**NVIDIA GPU Operator stack:**
1. NVIDIA Container Toolkit (`nvidia-container-toolkit`)
2. NVIDIA GPU Operator (Helm chart)
3. NVIDIA Device Plugin (daemonset)

```bash
# Install NVIDIA drivers on host:
sudo apt install nvidia-driver-535 nvidia-utils-535

# Install GPU Operator via Helm:
helm repo add nvidia https://helm.ngc.nvidia.com/nvidia
helm install gpu-operator nvidia/gpu-operator \
  --set operator.defaultRuntime=crio \
  --namespace gpu-operator --create-namespace

# Verify:
kubectl describe node gpu-worker-01 | grep nvidia.com/gpu
```

**GPU Sharing (MPS — Multi-Process Service):**
For RTX consumer cards, use MPS to split GPU into logical units:
```yaml
# Example: split RTX 5080 16GB into 8 × 2GB vGPUs
nvidia.com/gpu: "8"
nvidia.com/gpu.memory: "2048"
```

**Node labeling for GPU workloads:**
```bash
kubectl label node gpu-worker-01 accelerator=nvidia-rtx5080
kubectl taint node gpu-worker-01 gpu=true:NoSchedule
```

### 3. Edge Node Setup

**Challenges:** ARM architecture, lower power, intermittent connectivity.

**Edge-specific configuration:**
```yaml
# edge-node-patch.yaml
spec:
  taints:
  - key: "node-type"
    value: "edge"
    effect: "NoSchedule"
  kubelet:
    nodeLabels:
      node.kubernetes.io/edge: "true"
      arch: "arm64"
```

**Workload scheduling on edge:**
```yaml
tolerations:
- key: "node-type"
  operator: "Equal"
  value: "edge"
  effect: "NoSchedule"
nodeSelector:
  arch: "arm64"
```

### 4. Cilium CNI (eBPF Networking)

**Why Cilium over Flannel/Calico:**
- eBPF-based (no iptables overhead)
- Hubble observability (service map + flows)
- NetworkPolicy enforcement at kernel level
- Gateway API support

```bash
helm install cilium cilium/cilium \
  --namespace kube-system \
  --set ipam.mode=kubernetes \
  --set kubeProxyReplacement=true \
  --set hubble.relay.enabled=true \
  --set hubble.ui.enabled=true
```

### 5. GitOps with ArgoCD

**App-of-Apps pattern:**
```
root-app.yaml (single bootstrap)
├── appsets/infrastructure.yaml
│   ├── cert-manager
│   ├── cilium
│   ├── longhorn
│   └── monitoring (prometheus + grafana)
├── appsets/platform.yaml
│   ├── external-secrets
│   ├── cloudflared (tunnel)
│   └── harbor (registry)
└── appsets/workloads.yaml
    ├── vllm (AI inference)
    ├── jupyterhub
    ├── home-assistant
    └── plex
```

```bash
# Bootstrap:
kubectl create namespace argocd
kubectl apply -n argocd -f https://raw.../argocd/install.yaml
kubectl apply -f root-argocd-app.yaml
```

### 6. Monitoring Stack

**Components:**
- **Prometheus** — metrics collection (kube-prometheus-stack)
- **Grafana** — dashboards (Node Exporter, kube-state-metrics)
- **DCGM Exporter** — NVIDIA GPU metrics (temperature, utilization, memory)
- **Loki** (optional) — log aggregation

### 7. Storage with Longhorn

```bash
helm install longhorn longhorn/longhorn \
  --namespace longhorn-system --create-namespace \
  --set defaultSettings.replicaCount=2
```

**Gotcha:** On edge/ARM nodes, disable Longhorn replica placement or use separate storage class.

### 8. TLS and External Access

**Option A: Cloudflare Tunnel (recommended for homelab)**
- No port forwarding needed
- Free Cloudflare tier
- DDoS protection included
```yaml
# cloudflared deployment — connects to Cloudflare edge
# Services exposed via *.home.example.com → Cloudflare → tunnel → cluster
```

**Option B: cert-manager + Let's Encrypt + Traefik Ingress**
```bash
helm install cert-manager jetstack/cert-manager \
  --namespace cert-manager --create-namespace \
  --set installCRDs=true
```

## Two-Phase Deployment

### Phase 1: Bootstrap (Manual / Ansible)
1. Provision OS on all nodes (Ubuntu 22.04)
2. Install NVIDIA drivers on GPU nodes
3. Install k3s on control plane
4. Join workers and edge nodes
5. Deploy Cilium CNI
6. Install GPU Operator
7. Bootstrap ArgoCD

### Phase 2: GitOps (ArgoCD)
1. Deploy root ArgoCD application
2. ArgoCD deploys: cert-manager, Longhorn, External Secrets, Cloudflare Tunnel
3. Monitoring stack comes online
4. Deploy workload applications
5. Enable automated updates (Renovate bot)

## Success Criteria

1. `kubectl get nodes` shows all nodes Ready (CP, workers, GPU, edge)
2. `kubectl describe node gpu-worker-01 | grep nvidia.com/gpu` shows GPUs
3. ArgoCD dashboard accessible, all apps synced and healthy
4. Pod can schedule on GPU node and access GPU (`nvidia-smi` inside container)
5. Longhorn volumes create, attach, snapshot successfully
6. Service accessible via Cloudflare Tunnel or ingress
7. Grafana dashboards show cluster + GPU metrics
8. Edge node can run ARM-compatible workloads
9. Node failure: pods reschedule to other nodes automatically
