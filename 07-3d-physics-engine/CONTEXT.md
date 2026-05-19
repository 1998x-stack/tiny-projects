# 3D Physics Engine

A toy 3D rigid-body physics engine for learning the fundamentals of real-time simulation: integration, collision detection, constraint solving, and spatial partitioning.

## Glossary

| Term | Definition |
|------|------------|
| **RigidBody** | A simulated object with mass, velocity, orientation, and a collision shape. Static bodies have zero mass. |
| **CollisionShape** | Geometric description of a body's boundary. Owned by the RigidBody. Supports GJK/EPA queries via `support()` and `compute_aabb()`. |
| **World** | Top-level container: owns all bodies, runs the simulation loop, holds global parameters. |
| **SimulationParams** | Centralized tunable constants (gravity, damping, solver iterations, sleep thresholds, etc.). |
| **Integration** | Numerically advancing position and velocity over time. Semi-implicit Euler used here. |
| **Broadphase** | Fast, approximate pair-finding to avoid O(n²) narrow-phase checks. Octree spatial partitioning. |
| **Narrow Phase** | Precise collision test between two shapes. GJK (overlap) + EPA (penetration depth/normal). |
| **GJK** (Gilbert–Johnson–Keerthi) | Algorithm that tests whether two convex shapes overlap using Minkowski difference and a simplex search. |
| **EPA** (Expanding Polytope Algorithm) | Extends GJK's overlap simplex to compute penetration depth and contact normal. |
| **Contact Manifold** | Collection of contact points between two overlapping bodies. Reduced to ≤ 4 significant points. |
| **Contact Point** | A single touch-point with penetration depth, normal, and accumulated impulses (for warm-starting). |
| **Sequential Impulse** | Iterative constraint solver: applies one contact at a time, converges through repetition. |
| **Baumgarte Stabilization** | Position correction term added to velocity constraints to resolve penetration drift over time. |
| **Sleep** | Deactivation of nearly-stationary bodies to save computation. Bodies wake on collision or external force. |
| **Fixed Timestep** | The physics step always runs with an identical `dt` value (1/240s), using an accumulator to match wall-clock time. |

## Relationships

- A **World** contains many **RigidBodies**
- A **RigidBody** owns exactly one **CollisionShape**
- **Broadphase** produces candidate pairs → **GJK** filters overlapping pairs → **EPA** computes penetration → **Sequential Impulse** solver resolves contacts
- **SimulationParams** feeds **World**, solver, and sleep system

## Example Dialogue

> **Dev:** "When does GJK run vs when does EPA run?"
> **Domain expert:** "Broadphase finds candidate pairs, then GJK tests each pair for overlap. Only overlapping pairs go to EPA to get penetration depth. EPA never runs on separated pairs."

## Flagged Ambiguities

- "Shape" was used to mean both CollisionShape type and specific instances → resolved: CollisionShape is the base class; concrete shapes are SphereShape, BoxShape, etc.
- Angular velocity storage frame (body vs world) → resolved: `angular_velocity` is stored in body-space (standard convention). Torque and impulses must be transformed to body-space before multiplying by `inv_inertia_tensor`.
