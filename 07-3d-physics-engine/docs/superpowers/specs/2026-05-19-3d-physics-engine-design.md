# 3D Physics Engine — Specification

> **Type**: Educational toy engine for learning 3D rigid body simulation.
> **Language**: C++17 | **Build**: CMake | **Rendering**: raylib (separate from core)
> **Dependencies**: Standard library only for physics core. Eigen3 optional fallback. raylib for visualization.
>
> Based on: raw-physics, Riemann, basic-physics, physics3D, slrbs

---

## 1. Tech Stack

| Component | Choice | Rationale |
|-----------|--------|-----------|
| Language | C++17 | `std::optional`, structured bindings, widely supported |
| Math library | Hand-rolled `vec3`, `quat`, `mat3` | Zero-dependency core; full control over layout |
| Linear algebra fallback | Eigen3 (optional) | Pre-installed in Docker; use only if hand-rolled blocked |
| Build system | CMake 3.16+ | Standard, works everywhere |
| Visualization | raylib 5.x | Simple C API, no build system headaches |
| Container | Docker (Ubuntu 22.04) | Reproducible dev environment |

**Coding standards:**
- C++17, no exceptions in physics hot path (return error codes / optional)
- Header-only for math types; `.cpp` for simulation logic
- `snake_case` for functions/variables, `PascalCase` for types

---

## 2. Project Structure

```
src/
├── math/
│   ├── vec3.h           # 3D vector (float x, y, z)
│   ├── quat.h           # Quaternion (float w, x, y, z)
│   ├── mat3.h           # 3×3 matrix (column-major, 9 floats)
│   └── math_utils.h     # dot, cross, normalize, inertia formulas
├── core/
│   ├── rigid_body.h     # RigidBody struct + state management
│   ├── world.h          # World: owns bodies, forces, simulation params
│   ├── simulation.h     # Step function: integrate → collide → solve
│   └── sleep.h          # Sleep detection and wake logic
├── collision/
│   ├── aabb.h           # Axis-aligned bounding box
│   ├── gjk.h            # GJK distance / overlap test
│   ├── epa.h            # EPA penetration depth + normal
│   ├── contact.h        # Contact point, manifold struct
│   ├── manifold.h       # Sutherland-Hodgman clipping, contact reduction
│   └── broadphase.h     # Octree spatial partitioning
├── shapes/
│   ├── shape.h          # Base CollisionShape + support function interface
│   ├── sphere.h         # Sphere: center + radius
│   ├── box.h            # OBB: center + 3 axes + half-extents
│   ├── capsule.h        # Capsule: segment + radius
│   ├── convex_hull.h    # Convex hull: vertex list
│   └── plane.h          # Infinite plane: normal + offset (static only)
├── constraints/
│   ├── solver.h         # Sequential impulse constraint solver
│   └── contact_constraint.h  # Normal + friction constraint
├── render/              # Separate from physics core
│   └── debug_draw.h     # Wireframes, contacts, normals, AABBs
├── app/
│   └── main.cpp         # Entry point: setup scene, run loop
└── CMakeLists.txt
```

**Dependency rules:**
- `math/` → no internal deps
- `shapes/` → depends on `math/`
- `collision/` → depends on `math/`, `shapes/`
- `core/` → depends on `math/`, `collision/`, `shapes/`, `constraints/`
- `constraints/` → depends on `math/`, `core/` (for RigidBody)
- `render/` → depends on `core/`, `math/`, `shapes/`; links raylib
- `app/` → depends on everything

---

## 3. Architecture Overview

```
┌──────────────────────────────────────────────────────────────┐
│                     Simulation Step (fixed dt)                 │
│                                                                │
│  1. Apply Forces     2. Integrate        3. Broadphase        │
│  (gravity, damping,  (semi-implicit      (octree →            │
│   user forces)        Euler)              candidate pairs)     │
│         │                  │                    │              │
│         ▼                  ▼                    ▼              │
│  ┌──────────┐      ┌──────────┐        ┌──────────────┐      │
│  │ Forces   │─────►│Integrate │───────►│Collision     │      │
│  │          │      │(pos+vel) │        │Detection     │      │
│  └──────────┘      └──────────┘        │(GJK+EPA)     │      │
│                                          └──────┬───────┘      │
│                                                 │              │
│                                         4. Build Contacts     │
│                                         (manifold, clipping)   │
│                                                 │              │
│                                                 ▼              │
│                                        ┌───────────────┐      │
│                                        │5. Constraint  │      │
│                                        │   Solver      │      │
│                                        │(sequential    │      │
│                                        │ impulse,      │      │
│                                        │ Baumgarte)    │      │
│                                        └───────┬───────┘      │
│                                                │              │
│                                        6. Sleep / Wake        │
│                                                │              │
│                                                ▼              │
│                                        ┌───────────────┐      │
│                                        │7. Update       │      │
│                                        │   Velocities   │      │
│                                        └───────────────┘      │
└──────────────────────────────────────────────────────────────┘
```

---

## 4. Fixed Timestep

**Deterministic physics requires a fixed timestep.**

```
target_frametime = 1/60 ≈ 16.67ms
physics_dt        = 1/240 ≈ 4.17ms  (4 substeps per frame)
max_substeps      = 8               (catch-up cap)
```

```cpp
void run_frame(float frame_time) {
    accumulator += frame_time;
    accumulator = std::min(accumulator, max_substeps * physics_dt);

    while (accumulator >= physics_dt) {
        step(physics_dt);  // always called with same dt
        accumulator -= physics_dt;
    }

    render(accumulator / physics_dt);  // interpolation alpha
}
```

**Why 240 Hz:** 4 substeps per frame gives good stacking stability and reduces tunneling for typical object speeds. Lower than 120 Hz → jitter in stacks. Higher than 480 Hz → diminishing returns for toy engine.

**Determinism rule:** Every `step(dt)` call uses identical `dt`. No variable timestep ever passed to physics.

---

## 5. Simulation Parameters (Centralized)

```cpp
struct SimulationParams {
    // Gravity
    vec3 gravity = vec3(0.0f, -9.81f, 0.0f);

    // Damping
    float linear_damping = 0.01f;   // per-unit velocity damping
    float angular_damping = 0.01f;

    // Timestep
    float physics_dt = 1.0f / 240.0f;
    int max_substeps = 8;

    // Constraint solver
    int solver_iterations = 10;
    float baumgarte = 0.2f;         // position correction rate
    float penetration_slop = 0.005f; // ignore tiny overlaps

    // Sleep
    float sleep_linear_threshold = 0.01f;
    float sleep_angular_threshold = 0.01f;
    float sleep_timeout = 1.0f;      // seconds below threshold before sleep

    // Collision
    float contact_break_distance = 0.02f;  // max distance to keep persistent contact
};
```

All constants live here, tunable per scene. No magic numbers in simulation code.

---

## 6. API Design

User-facing API (from `app/main.cpp`):

```cpp
#include "core/world.h"
#include "shapes/sphere.h"
#include "shapes/box.h"
#include "shapes/plane.h"

int main() {
    // 1. Create world with default params
    World world;

    // 2. Create bodies
    RigidBody body;
    body.position = vec3(0, 5, 0);
    body.orientation = quat::identity();
    body.mass = 1.0f;
    body.restitution = 0.3f;
    body.friction = 0.5f;
    body.shape = std::make_unique<SphereShape>(1.0f);
    body.compute_inertia();  // fills inertia_tensor from shape + mass
    world.add_body(std::move(body));

    // Static bodies: mass = 0 → inv_mass = 0 (never moves)
    RigidBody ground;
    ground.mass = 0.0f;
    ground.shape = std::make_unique<PlaneShape>(vec3(0, 1, 0), 0.0f);
    world.add_body(std::move(ground));

    // 3. Optional: modify params
    world.params().gravity = vec3(0, -20.0f, 0);

    // 4. Run loop
    while (!window_should_close()) {
        float frame_time = get_frame_time();
        world.step(frame_time);  // handles fixed timestep internally

        // Render
        for (const auto& body : world.bodies()) {
            draw_shape(body);
        }
    }
}
```

**Key design decisions:**
- `World` owns all bodies (`std::vector<std::unique_ptr<RigidBody>>`)
- Static bodies: `mass == 0` → `inv_mass = 0`, never moved by solver
- Shapes are polymorphic (`CollisionShape` base) but caching-friendly (no virtual calls in hot path; dispatch via enum tag)
- Forces applied by user BEFORE `world.step()` via `body.add_force(vec3)`

---

## 7. Rigid Body Representation

**State vector (per body):**

```
position:       vec3    (x, y, z)          — center of mass (world space)
orientation:    quat    (w, x, y, z)       — rotation (unit quaternion)
linear_velocity:  vec3  (vx, vy, vz)       — world space
angular_velocity: vec3  (ωx, ωy, ωz)       — body space
```

**Constant properties (per body):**

```
mass:            float              — total mass (kg); 0 = static
inv_mass:        float              — 1/mass; 0 for static
inertia_tensor:  mat3               — 3×3 in body frame
inv_inertia:     mat3               — inverse inertia in body frame
restitution:     float   [0, 1]     — coefficient of restitution (bounciness)
friction:        float   [0, 1]     — Coulomb friction coefficient
```

```cpp
enum class BodyType { Dynamic, Static };

struct RigidBody {
    // State
    vec3 position;
    quat orientation;
    vec3 linear_velocity;
    vec3 angular_velocity;    // body-space (ω)

    // Constants
    float mass;
    float inv_mass;           // 0 for static
    mat3 inertia_tensor;      // body-frame diagonal
    mat3 inv_inertia_tensor;  // body-frame
    float restitution;
    float friction;

    // Accumulators (reset each substep)
    vec3 force_accum;
    vec3 torque_accum;

    // Collision shape (owned, polymorphic)
    CollisionShape* shape;

    // Sleep state
    bool is_sleeping = false;
    float sleep_timer = 0.0f;

    // --- Methods ---
    void add_force(const vec3& force);
    void add_force_at_point(const vec3& force, const vec3& world_point);
    void add_torque(const vec3& torque);
    void compute_inertia();  // from shape + mass
};
```

---

## 8. Integration

**Semi-implicit Euler (symplectic) — first-order, energy-stable with damping.**

```
function integrate(body, dt):
    // Linear
    acceleration = body.force_accum * body.inv_mass
    body.linear_velocity += acceleration * dt
    body.position += body.linear_velocity * dt

    // Angular (body-space angular velocity ω)
    angular_accel = body.inv_inertia_tensor * (body.torque_accum - cross(ω, I·ω))
    body.angular_velocity += angular_accel * dt

    // Orientation update
    q = body.orientation
    w = quat(0, body.angular_velocity)
    dq = 0.5 * w * q * dt
    body.orientation = normalize(q + dq)

    // Reset accumulators
    body.force_accum = vec3(0)
    body.torque_accum = vec3(0)
```

**Forces applied each substep:**

```cpp
void apply_gravity(RigidBody& body, const vec3& gravity) {
    body.force_accum += gravity * body.mass;
}

void apply_damping(RigidBody& body, float linear_damp, float angular_damp) {
    // F = -k_d * v  (velocity-proportional damping, not physically accurate but simple)
    body.force_accum -= body.linear_velocity * linear_damp;
    body.torque_accum -= body.angular_velocity * angular_damp;
}
```

**Gyroscopic term:** `τ_gyro = ω × (I · ω)`. Integrated explicitly (OK for small dt). If energy gain observed → switch to implicit method from Box2D/Erin Catto.

---

## 9. Collision Detection — Broad Phase

**Octree spatial partitioning** (default; SAP alternative noted below).

```
function build_octree(bodies):
    root = OctreeNode(world_bounds)
    for body in bodies:
        insert(root, body, body.compute_aabb())
    return root

function find_pairs(node, pairs):
    if node.body_count < 2: return
    if node.is_leaf:
        for i in range(node.bodies):
            for j in range(i+1, node.bodies):
                pairs.add((node.bodies[i], node.bodies[j]))
    else:
        for child in node.children:
            find_pairs(child, pairs)
```

**Octree parameters:**

```
max_depth = 5           // deepest subdivision level
max_bodies_per_leaf = 4 // switch to brute force at leaf
loose_factor = 2.0      // loose octree: nodes 2× larger (reduces migration)
```

**Sweep and Prune (SAP) — simpler alternative:**
- Sort bodies by min AABB X coordinate
- Maintain overlapping intervals
- Only test pairs with overlapping AABBs in X, then Y, then Z
- Faster than octree for < ~200 objects with coherent motion

**Performance note:** For < 100 objects, brute force O(n²) may beat spatial partitioning overhead. Benchmark before optimizing.

---

## 10. Collision Detection — Narrow Phase

### 10a. GJK (Gilbert–Johnson–Keerthi)

Distance/overlap between two convex shapes using Minkowski difference.

```
function GJK(shape_a, shape_b):
    simplex = []                           // up to 4 points in 3D
    direction = shape_a.center - shape_b.center
    while iterations < 32:
        point = support(shape_a, shape_b, direction)
        if dot(point, direction) < 0:
            return NO_COLLISION
        simplex.append(point)
        if update_simplex(simplex, direction):
            return OVERLAP, simplex       // origin inside Minkowski diff
    return NO_COLLISION                   // degenerate: max iterations reached
```

**Simplex cases (3D):**
- 1 point → search toward origin
- 2 points (line) → closest point on line segment to origin
- 3 points (triangle) → closest point on triangle to origin, Voronoi regions
- 4 points (tetrahedron) → origin inside? If yes: overlap.

### 10b. EPA (Expanding Polytope Algorithm)

Given GJK's overlap simplex, find penetration depth and normal.

```
function EPA(shape_a, shape_b, simplex):
    polytope = convex_hull(simplex)        // start from GJK simplex
    while iterations < 32:
        face = find_closest_face(polytope) // face closest to origin, tiebreak by area
        support_pt = support(shape_a, shape_b, face.normal)
        d = dot(support_pt, face.normal)
        if |d - face.distance| < 0.001:
            return face.normal, face.distance  // penetration normal + depth
        polytope.expand(support_pt)
    return face.normal, face.distance      // best guess on timeout
```

### 10c. Support Functions

| Shape | Support function `f(direction)` |
|-------|--------------------------------|
| Sphere | `center + radius * normalize(direction)` |
| Box (OBB) | `center + Σ sign(dot(axis_i, dir)) * axis_i * half_extent_i` |
| Capsule | `closest_point_on_segment + radius * normalize(dir)` |
| Convex Hull | `vertex with max dot(dir, v)` (brute force, OK for < 100 verts) |
| Plane | Not used with GJK; handled separately as static contact |

---

## 11. Collision Response

### 11a. Contact Manifold

For each overlapping pair, build a contact manifold:

```
function build_manifold(shape_a, shape_b, overlap):
    // 1. Identify feature pairs (face-face, edge-edge, vertex-face)
    // 2. Sutherland-Hodgman clipping for box-box
    // 3. Reduce to ≤ 4 contact points (keep deepest penetration)
    // 4. Compute contact basis:
    //    normal = overlap.direction (from A to B)
    //    tangent1, tangent2 = orthonormal basis perpendicular to normal
```

**Contact persistence:** Match contacts across frames by feature ID. Warm-start solver with previous frame's accumulated impulses.

### 11b. Sequential Impulse Solver

```
function solve_contacts(contacts, dt):
    // Warm-start: apply accumulated impulses from previous frame
    for contact in contacts:
        apply_impulse(contact.body_a, contact.body_b,
                       contact.normal_impulse, contact.tangent_impulse,
                       contact.point, contact.normal, contact.tangent1, contact.tangent2)

    for iteration in range(solver_iterations):
        for contact in contacts:
            // Compute relative velocity at contact point
            v_rel = compute_relative_velocity(body_a, body_b, contact.point)

            // Normal impulse (resolve penetration + velocity constraint)
            v_n = dot(v_rel, contact.normal)
            effective_mass = compute_effective_mass(body_a, body_b, contact.point, contact.normal)

            // Baumgarte position correction (bias)
            bias = (baumgarte / dt) * max(contact.penetration - slop, 0.0f)
            delta_normal = -(v_n + bias) * effective_mass

            // Clamp accumulated impulse (non-negative for normal)
            new_impulse = max(contact.normal_impulse + delta_normal, 0.0f)
            delta_normal = new_impulse - contact.normal_impulse
            contact.normal_impulse = new_impulse

            apply_impulse(body_a, body_b, delta_normal, contact.normal, contact.point)

            // Friction (Coulomb model, tangent1 + tangent2)
            for tangent in [contact.tangent1, contact.tangent2]:
                v_t = dot(v_rel, tangent)
                delta_tangent = -v_t * effective_mass
                max_friction = contact.friction * contact.normal_impulse
                new_tangent = clamp(contact.tangent_impulse[i] + delta_tangent,
                                    -max_friction, max_friction)
                delta_tangent = new_tangent - contact.tangent_impulse[i]
                contact.tangent_impulse[i] = new_tangent

                apply_impulse(body_a, body_b, delta_tangent, tangent, contact.point)
```

**Impulse application:**

```cpp
void apply_impulse(RigidBody& a, RigidBody& b,
                   float impulse, const vec3& normal, const vec3& world_point) {
    a.linear_velocity -= (impulse * a.inv_mass) * normal;
    b.linear_velocity += (impulse * b.inv_mass) * normal;

    vec3 r_a = world_point - a.position;
    vec3 r_b = world_point - b.position;

    a.angular_velocity -= a.inv_inertia_tensor * cross(r_a, impulse * normal);
    b.angular_velocity += b.inv_inertia_tensor * cross(r_b, impulse * normal);
}
```

### 11c. Friction

Coulomb friction model with friction cone clamping:
- **Static friction**: no relative tangential velocity → apply up to `μ_s * N`
- **Kinetic friction**: sliding → apply `μ_k * N` opposing sliding direction
- **Toy simplification**: single `friction` coefficient (= μ_s = μ_k)

---

## 12. Supported Collision Shapes

| Shape | Description | Comment |
|-------|-------------|---------|
| Sphere | center + radius | Fastest; sphere-sphere is O(1) check |
| Box (OBB) | center + 3 orthonormal axes + 3 half-extents | General rotated box |
| Capsule | line segment + radius | Good for character controllers |
| Convex Hull | list of vertices (≤ 100) | Arbitrary convex shape |
| Plane | normal + offset | Static ground only (infinite) |

**Composite shapes** (out of scope for toy engine): A body has exactly one shape. No compound/concave shapes.

---

## 13. Sleeping (Deactivation)

Objects at rest are deactivated to save computation.

```cpp
void update_sleep(RigidBody& body, float dt, const SimulationParams& params) {
    if (length(body.linear_velocity) < params.sleep_linear_threshold &&
        length(body.angular_velocity) < params.sleep_angular_threshold) {
        body.sleep_timer += dt;
        if (body.sleep_timer > params.sleep_timeout) {
            body.is_sleeping = true;
            body.linear_velocity = vec3(0);
            body.angular_velocity = vec3(0);
        }
    } else {
        body.sleep_timer = 0.0f;
        body.is_sleeping = false;
    }
}
```

**Wake rules:**
- Sleeping body hit by moving body → wake
- Sleeping body receives external force → wake
- Static bodies never sleep (they don't move anyway)

---

## 14. Edge Cases & Recovery

| Scenario | Handling |
|----------|----------|
| **NaN position/velocity** | Skip body for one frame, clamp to last valid state |
| **Explosion** (bodies fly to infinity) | World bounds clip: if `|position| > 1000`, freeze body |
| **GJK non-convergence** | Max 32 iterations → return `NO_COLLISION` (missed contact, not crash) |
| **EPA non-convergence** | Max 32 iterations → use best face found (approximate contact) |
| **Zero mass dynamic body** | Assert in debug; clamp `inv_mass` to 0 in release |
| **Quaternion drift** | Normalize after every integration step |
| **Negative penetration** | Clamp to 0 (separated objects should not be corrected) |
| **NaN in solver** | Skip contact (won't converge, but won't crash) |

---

## 15. Development Roadmap

### Phase 1: Math Library
- **Deliverables**: `vec3`, `quat`, `mat3` with operators + utility functions
- **Verify**: Unit tests for all operations; quaternion multiplication rotates correctly
- **Files**: `src/math/*.h`

### Phase 2: Rigid Body + Integration
- **Deliverables**: `RigidBody` struct, force accumulation, semi-implicit Euler integration
- **Verify**: Single sphere falls under gravity; energy doesn't grow without external forces
- **Files**: `src/core/rigid_body.h`, `src/core/simulation.h`

### Phase 3: Collision Shapes + AABB
- **Deliverables**: Shape base class, Sphere, Box, Plane; AABB computation
- **Verify**: AABB bounds correctly in world space after rotation
- **Files**: `src/shapes/*.h`, `src/collision/aabb.h`

### Phase 4: Narrow Phase (GJK + EPA)
- **Deliverables**: GJK overlap test, EPA penetration, sphere-sphere, sphere-box, box-box
- **Verify**: Known collision scenarios return correct normal + depth
- **Files**: `src/collision/gjk.h`, `src/collision/epa.h`

### Phase 5: Constraint Solver
- **Deliverables**: Contact manifold, sequential impulse with Baumgarte, friction
- **Verify**: Box stack of 5 boxes remains stable for 10+ seconds; sphere bounces on plane
- **Files**: `src/collision/contact.h`, `src/collision/manifold.h`, `src/constraints/*.h`

### Phase 6: Broadphase + Sleep
- **Deliverables**: Octree, sleep detection/wake, integration with narrow phase
- **Verify**: 100+ objects at 60 fps; sleeping bodies consume zero collision time
- **Files**: `src/collision/broadphase.h`, `src/core/sleep.h`

### Phase 7: Visualization
- **Deliverables**: Raylib debug draw (wireframes, contacts, normals, AABBs, sleep state)
- **Verify**: Visual confirmation of all success criteria below
- **Files**: `src/render/debug_draw.h`, `src/app/main.cpp`

### Phase 8: Polish & Demos
- **Deliverables**: Interactive demo scenes, gyroscopic motion, final benchmarks
- **Files**: `src/app/main.cpp` (demo scenes)

---

## 16. Success Criteria

1. **Gravity**: Sphere falls under gravity (-9.81 m/s²) and reaches correct velocity after 1s
2. **Bounce**: Sphere dropped from 5m bounces on ground plane (restitution visible)
3. **Stack**: 5 boxes stacked vertically remain stable (> 10s without jitter or explosion)
4. **Incline**: Sphere rolls down 30° inclined plane with correct angular velocity
5. **Mid-air collision**: Two boxes thrown at each other collide and separate correctly
6. **Sleep**: Objects at rest deactivate within ~1 second; wake on collision
7. **Broadphase**: Octree reduces collision pairs by > 50% vs brute force with 100+ objects
8. **Gyroscopic motion**: T-handle (tennis racket theorem) visibly flips in free rotation
9. **Performance**: 100 dynamic boxes simulate at > 120 Hz physics rate (substep time < 4ms)
10. **No crashes**: Napkin math stress test (random impulses, high speeds) doesn't NaN or segfault

---

## 17. Gotchas Reference

See `gotchas.md` for detailed explanations of:
1. Tunneling (CCD, substep mitigation)
2. Quaternion normalization drift
3. GJK degenerate cases (touching, parallel faces)
4. EPA near-coplanar face oscillation
5. Constraint solver oscillation (Baumgarte bias)
6. Semi-implicit Euler energy drift
7. Inertia tensor computation (analytical formulas)
8. Friction model energy gain
9. Contact manifold pop-through
10. Gyroscopic torque energy gain
11. Octree rebuild cost vs n²
12. Debug visualization checklist
