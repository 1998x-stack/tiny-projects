# Homelab K8S Cluster — Gotchas

## 1. NVIDIA Driver + Container Runtime Hell

**Problem:** NVIDIA driver, CUDA version, and container runtime must all be compatible.

**Compatibility matrix:**
| Driver | Max CUDA | Container Toolkit |
|--------|----------|-------------------|
| 535.x  | 12.2     | ≥ 1.13.0          |
| 545.x  | 12.3     | ≥ 1.14.0          |
| 550.x  | 12.4     | ≥ 1.14.0          |

**Gotcha:** Proxmox GPU passthrough requires IOMMU enabled in BIOS + kernel:
```bash
# /etc/default/grub
GRUB_CMDLINE_LINUX_DEFAULT="quiet intel_iommu=on iommu=pt"
```

**Debug:** `nvidia-smi` works on host but not in container → container runtime not configured.

## 2. k3s Data Store Choice

**Embedded etcd** (recommended for HA):
- Pros: No external DB, built-in, good for 3+ nodes
- Cons: Higher resource usage

**Embedded SQLite** (single node only):
- Pros: Zero config, lowest resource
- Cons: No HA possible

**External etcd/MySQL/PostgreSQL:**
- Pros: Separate from k3s, existing DBA tools
- Cons: More components to maintain

**Gotcha:** Switching from SQLite to etcd requires cluster rebuild. Choose BEFORE production data.

## 3. ARM vs x86 Image Compatibility

**Problem:** Edge nodes (Raspberry Pi) are ARM64. Container images must be multi-arch.

**Check image architecture:**
```bash
docker manifest inspect nginx:latest | grep architecture
```

**Gotcha:** Many community images are amd64-only. Use `--platform linux/arm64` in Dockerfiles.

**For homelab apps:** Prefer apps with official ARM images:
- ✅ Traefik, Cilium, Longhorn, cert-manager, Prometheus, Grafana
- ❌ Some ML images, Plex (use linuxserver.io versions)

## 4. Longhorn Replica Placement

**Problem:** Longhorn replicates volumes. 3 replicas on 3 nodes = data safe, but what if only 2 nodes?

**Settings:**
```yaml
defaultSettings:
  replicaCount: 2  # minimum for redundancy
  defaultDataLocality: best-effort  # keep replica local if possible
```

**Gotcha:** Longhorn on ARM edge nodes → disable with node selector:
```bash
kubectl label node edge-rpi-01 node.longhorn.io/create-default-disk=false
```

## 5. GPU Memory and MPS

**Problem:** Consumer GPUs (RTX) don't support MIG (Multi-Instance GPU). Workaround: MPS.

**MPS tradeoffs:**
- ✅ All containers share GPU (no exclusive lock)
- ✅ Better utilization
- ❌ No memory isolation → one container can OOM the GPU
- ❌ Error isolation: one bad CUDA call crashes all MPS clients

**Alternative:** Time-slicing GPU sharing (NVIDIA config):
```yaml
config:
  name: time-slicing-config
  default: "8"  # 8 replicas per GPU
```

## 6. cert-manager DNS Challenge Timing

**Problem:** cert-manager creates DNS TXT record → waits for propagation → fails if DNS TTL too high.

**Solutions:**
- Cloudflare DNS: propagation is near-instant → use Cloudflare DNS challenge
- Increase `--dns01-recursive-nameservers-only` and wait time
- Use HTTP challenge instead (requires publicly accessible port 80)

**Gotcha:** If Let's Encrypt rate limit hit (5 certs/week per domain), use staging environment:
```yaml
issuer: letsencrypt-staging  # during testing
```

## 7. Cilium vs kube-proxy Replacement

**Problem:** Cilium in kube-proxy replacement mode conflicts with k3s built-in kube-proxy.

**Fix:** Disable k3s kube-proxy on install:
```bash
curl -sfL https://get.k3s.io | sh -s - server \
  --disable-kube-proxy \
  --flannel-backend=none
```

## 8. ArgoCD "App of Apps" Sync Order

**Problem:** Apps need to deploy in specific order (infrastructure before workloads).

**Solution:** Use sync waves:
```yaml
metadata:
  annotations:
    argocd.argoproj.io/sync-wave: "-5"  # infrastructure first
---
metadata:
  annotations:
    argocd.argoproj.io/sync-wave: "0"   # platform services
---
metadata:
  annotations:
    argocd.argoproj.io/sync-wave: "5"   # workloads last
```

## 9. Cloudflare Tunnel Reliability

**Problem:** Tunnel daemon crashes → all external services unreachable.

**Fix:** Run `cloudflared` as deployment (not single pod):
```yaml
replicas: 2
strategy:
  type: RollingUpdate
```

**Gotcha:** Multiple tunnels to same hostname → DNS round-robin confusion. Use one tunnel with multiple replicas, NOT multiple tunnels.

## 10. Edge Node Networking

**Problem:** Edge nodes may be on WiFi, behind NAT, or have intermittent connectivity.

**Solutions:**
- K3S uses websocket tunnel for agent-server comms → works through NAT
- Set `node.kubernetes.io/unreachable` toleration times higher for edge
- Consider k3s `--node-ip` to bind to correct interface

## 11. Renovate Bot Auto-Update Risk

**Problem:** Renovate auto-updates Helm charts and Docker images → can break running services.

**Mitigation:**
```json
{
  "packageRules": [
    {
      "matchUpdateTypes": ["major"],
      "enabled": false  // don't auto-upgrade major versions
    },
    {
      "matchDatasources": ["docker"],
      "matchPackagePatterns": ["^bitnami/"],
      "automerge": false  // require manual approval
    }
  ]
}
```

## 12. Node Failure Recovery

**Problem:** Control plane node dies → cluster partially unusable.

**Minimum for HA:** 3 control plane nodes (etcd quorum = 2).

**Gotcha:** With 2 CP nodes, etcd loses quorum if 1 dies → cluster read-only!

**Recovery from CP loss:**
```bash
# On surviving CP node:
k3s server --cluster-reset
# Rejoin other nodes with new token
```
