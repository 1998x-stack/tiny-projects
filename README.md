# Tiny Projects — 从零构建的8个玩具项目

> "We choose to build these things not because they are easy, but because we want to understand how they work."

基于社区已有实现的参考，从零构建8个中高级计算机工程项目。每个项目包含详细的 `spec.md`（基于他人实现总结的规格说明）、`gotchas.md`（常见陷阱与解决方案）、Docker 运行环境。

---

## 项目列表

| # | 项目 | 描述 | 难度 | 参考实现 |
|---|------|------|------|----------|
| 1 | [Toy Linux Kernel](./01-toy-linux-kernel/) | 多任务、内存管理、引导加载、文件系统 | ⭐⭐⭐⭐⭐ | xv6 (MIT) |
| 2 | [Toy Browser Engine](./02-toy-browser-engine/) | HTML/CSS解析、JS渲染、隔离、后台任务 | ⭐⭐⭐⭐⭐ | Robinson, Mini Browser |
| 3 | [Homelab K8S Cluster](./03-homelab-k8s/) | GPU节点 + 边缘节点 + GitOps持久化维护 | ⭐⭐⭐⭐ | k3s-ansible, homelab |
| 4 | [Ben Eater 8-bit CPU](./04-ben-eater-cpu/) | 面包板8位CPU (Ben Eater教程) | ⭐⭐⭐⭐ | Ben Eater, jcpu |
| 5 | [Toy PyTorch](./05-toy-pytorch/) | 纯Python自动微分引擎 + 神经网络 | ⭐⭐⭐ | micrograd, tinygrad |
| 6 | [Toy Database](./06-toy-database/) | B+ Tree, 并发读写, 索引 | ⭐⭐⭐⭐ | toydb, baobab |
| 7 | [3D Physics Engine](./07-3d-physics-engine/) | 刚体动力学, 碰撞检测, 约束求解 | ⭐⭐⭐⭐ | raw-physics, Riemann |
| 8 | [Tiny Claude Code](./08-tiny-claude-code/) | Harness engineer优化的微型AI编程代理 | ⭐⭐⭐ | CoreCoder, claude-code-from-scratch |

---

## 项目结构

每个项目包含：

```
project/
├── spec.md          # 规格说明（基于社区实现的总结）
├── gotchas.md       # 常见陷阱与解决方案
├── Dockerfile       # Docker 运行环境
├── assets/          # GitHub Pages 静态资源
│   └── index.html   # 项目展示页面
└── src/             # 源代码（待实现）
```

---

## 快速开始

### 前置要求

- Docker (未安装请参考 [Docker 安装指南](https://docs.docker.com/get-docker/))
- Git & GitHub 账号
- 基础知识：C/Rust/Python/K8S 等（各项目不同）

### 通用 Docker 运行方式

```bash
# 进入项目目录
cd 01-toy-linux-kernel

# 构建 Docker 镜像
docker build -t toy-kernel .

# 运行
docker run --rm -it toy-kernel
```

---

## 参考来源

每个项目的 `spec.md` 详细列出了参考的社区实现和教程。主要来源包括：

- **xv6** (MIT) — 教学用 Unix 内核
- **Robinson** (Matt Brubeck) — 玩具浏览器渲染引擎
- **k3s-ansible** — 家庭 K8S 集群最佳实践
- **Ben Eater** — 8位面包板 CPU 教程
- **micrograd** (Andrej Karpathy) — 微型自动微分引擎
- **toydb** (Erik Grinaker) — 分布式 SQL 数据库
- **raw-physics** — XPBD 刚体物理模拟
- **CoreCoder** — 微型 AI 编程代理蓝图

---

## 许可

MIT License — See individual project directories for details.
