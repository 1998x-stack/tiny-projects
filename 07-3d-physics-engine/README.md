# 3D Rigid Body Physics Engine

A from-scratch 3D rigid-body physics engine written in C++17 with zero external dependencies in the simulation core.

> **Why this exists:** Build a full physics pipeline — integration, broad/narrow phase collision detection, constraint solving, and spatial partitioning — to understand how game physics works under the hood.

![](https://img.shields.io/badge/C%2B%2B-17-blue) ![](https://img.shields.io/badge/build-CMake-green) ![](https://img.shields.io/badge/dependencies-none-red) ![](https://img.shields.io/badge/render-raylib-orange)

---

## Table of Contents

- [Quick Start](#quick-start)
- [Architecture](#architecture)
- [API Reference](#api-reference)
  - [World](#world)
  - [RigidBody](#rigidbody)
  - [Collision Shapes](#collision-shapes)
  - [SimulationParams](#simulationparams)
  - [Math Types](#math-types)
- [Examples](#examples)
- [Build & Run](#build--run)
- [Configuration Guide](#configuration-guide)
- [Testing](#testing)
- [Debug Visualization](#debug-visualization)
- [Troubleshooting](#troubleshooting)
- [Gotchas](#gotchas)
- [Project Structure](#project-structure)
- [References](#references)

---

## Quick Start

### Docker (recommended)

```bash
cd 07-3d-physics-engine
docker build -t physics-engine .
docker run --rm -it -v $(pwd):/workspace physics-engine bash

# Inside container:
mkdir build && cd build && cmake .. && make -j$(nproc)
./physics_sim
```

### Native (macOS/Linux)

```bash
# Prerequisites: CMake 3.16+, C++17 compiler, raylib 5.x
brew install cmake raylib   # macOS
# or: sudo apt install cmake libglfw3-dev libraylib-dev  # Ubuntu

mkdir build && cd build && cmake .. && make -j$(nproc)
./physics_sim
```

---

## Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                Simulation Step (fixed 1/240s)                 │
│                                                               │
│  1. Apply Forces      2. Integrate          3. Broadphase     │
│  ┌──────────┐        ┌───────────┐        ┌──────────────┐  │
│  │ gravity  │───────►│ semi-     │───────►│ octree       │  │
│  │ damping  │        │ implicit  │        │ candidate    │  │
│  │ user f.  │        │ Euler     │        │ pairs        │  │
│  └──────────┘        └───────────┘        └──────┬───────┘  │
│                                                   │          │
│  4. Narrow Phase                                  ▼          │
│  ┌─────────────────────────┐            ┌──────────────┐    │
│  │ GJK (overlap test)      │◄───────────│ AABB filter  │    │
│  │ EPA (penetration depth) │            └──────────────┘    │
│  └───────────┬─────────────┘                                 │
│              │                                                │
│  5. Contacts ▼           6. Constraint Solver                │
│  ┌──────────────┐       ┌─────────────────────┐             │
│  │ contact      │──────►│ sequential impulse  │             │
│  │ manifold     │       │ + Baumgarte bias    │             │
│  │ (1-4 pts)    │       │ + Coulomb friction  │             │
│  └──────────────┘       └──────────┬──────────┘             │
│                                     │                        │
│  7. Sleep / Wake                   ▼                        │
│  ┌──────────────┐       ┌─────────────────────┐             │
│  │ deactivate   │◄──────│ post-solve velocity │             │
│  │ stationary   │       │ damping (anti-jitter)│             │
│  └──────────────┘       └─────────────────────┘             │
└──────────────────────────────────────────────────────────────┘
```

**Key design decisions:**
- **Fixed 240 Hz timestep** — accumulator pattern absorbs frame rate variance, physics is deterministic
- **Body-space angular velocity** — standard convention (matches Box2D, Bullet, PhysX); torque/impulse transforms required
- **Semi-implicit Euler** — first-order symplectic integrator, energy-stable with damping
- **Cold-start solver** — no contact persistence/warm-starting; compensated with 15 solver iterations
- **Simplified EPA** — no edge tracking; sufficient for sphere-sphere and box-box contacts
- **Shape owned by body** — `std::unique_ptr<CollisionShape>` eliminates dangling pointer risk

---

## API Reference

### World

The top-level container: owns all bodies, manages the simulation loop, holds global parameters.

```cpp
#include "core/world.h"

World world;

// Access mutable simulation parameters
world.params().gravity = vec3(0, -20.0f, 0);
world.params().solver_iterations = 15;

// Add bodies (transfers ownership)
world.add_body(std::move(my_body));

// Inspect bodies (read-only)
for (const auto& body : world.bodies()) {
    printf("Position: (%.2f, %.2f, %.2f)\n",
           body->position.x, body->position.y, body->position.z);
}

// Advance simulation by frame_time seconds
// Internally runs N substeps at fixed physics_dt
world.step(frame_time);
```

**Important:** `step()` handles the fixed timestep internally. Never pass variable `dt` to individual physics functions — pass real frame time to `step()` and it divides into substeps.

---

### RigidBody

```cpp
#include "core/rigid_body.h"

auto body = std::make_unique<RigidBody>();

// --- State (read/write) ---
body->position = vec3(0, 10, 0);          // world-space center of mass
body->orientation = quat_identity();       // unit quaternion
body->linear_velocity = vec3(0, 0, 0);
body->angular_velocity = vec3(0, 0, 0);   // body-space

// --- Physical properties ---
body->set_mass(5.0f);                     // dynamic body (> 0)
body->set_mass(0.0f);                     // static body (immovable)
body->restitution = 0.3f;                  // bounciness [0, 1]
body->friction = 0.5f;                     // Coulomb coefficient

// --- Inertia ---
body->shape = std::make_unique<SphereShape>(vec3(0, 0, 0), 1.0f);
body->inertia_tensor = inertia_tensor_sphere(1.0f, body->mass);
body->inv_inertia_tensor = inverse(body->inertia_tensor);

// --- Forces (applied BEFORE world.step(), reset each substep) ---
body->add_force(vec3(0, 100, 0));          // Newtons at COM
body->add_force_at_point(vec3(10, 0, 0), vec3(0, 1, 0));  // torque from offset
body->add_torque(vec3(0, 0, 50));          // direct torque

// --- Queries ---
body->is_dynamic();                        // true if mass > 0
body->is_sleeping;                         // deactivated (stationary long enough)
body->world_inertia();                     // body-frame → world-space transform
```

**Static bodies:** Set `mass = 0` (or `set_mass(0.0f)`). These never move, ignoring all forces and impulses. Use for ground planes, walls, and fixtures.

**Sleep:** Bodies that remain nearly-stationary for `sleep_timeout` seconds are deactivated. They wake automatically on collision or external force.

---

### Collision Shapes

All shapes inherit from `CollisionShape` and provide `support()` (GJK/EPA queries) and `compute_aabb()` (broadphase bounding boxes).

#### SphereShape

```cpp
#include "shapes/sphere.h"

auto sphere = std::make_unique<SphereShape>(
    vec3(0, 5, 0),   // center (world space)
    1.0f               // radius
);
body->shape = std::move(sphere);
```

#### BoxShape (OBB — Oriented Bounding Box)

```cpp
#include "shapes/box.h"

// Axis-aligned convenience constructor
auto box = std::make_unique<BoxShape>(
    vec3(0, 5, 0),          // center
    vec3(1, 0, 0),           // right axis
    vec3(0, 1, 0),           // up axis
    vec3(0, 0, 1),           // forward axis
    0.5f, 0.5f, 0.5f        // half-extents (width/2, height/2, depth/2)
);
body->shape = std::move(box);

// Or use the static AABB helper:
auto box2 = BoxShape::aabb(vec3(0, 5, 0), 0.5f, 0.5f, 0.5f);
```

**Inertia for boxes:**
```cpp
body->inertia_tensor = inertia_tensor_box(0.5f, 0.5f, 0.5f, body->mass);
body->inv_inertia_tensor = inverse(body->inertia_tensor);
```

#### PlaneShape

```cpp
#include "shapes/plane.h"

auto plane = std::make_unique<PlaneShape>(
    vec3(0, 1, 0),   // unit normal (points UP = objects above plane)
    0.0f               // distance from origin (0 = at origin)
);
ground_body->shape = std::move(plane);
```

> **Note:** Planes are infinite and should only be used on static bodies. A plane at `normal=(0,1,0) offset=0` creates a horizontal ground at y=0 with normal pointing up. The `offset` shifts the plane along the normal: `offset=5` creates ground at y=5.

---

### SimulationParams

All tunable constants live here. Access via `world.params()`:

```cpp
struct SimulationParams {
    // Gravity
    vec3 gravity = vec3(0.0f, -9.81f, 0.0f);

    // Damping (velocity-proportional, simple model)
    float linear_damping = 0.01f;
    float angular_damping = 0.01f;

    // Timestep
    float physics_dt = 1.0f / 240.0f;   // 240 Hz physics rate
    int max_substeps = 8;                 // catch-up cap

    // Solver (sequential impulse)
    int solver_iterations = 15;           // higher = more stable stacks
    float baumgarte = 0.15f;              // position correction rate (0.1-0.3)
    float penetration_slop = 0.005f;      // ignore tiny overlaps (< 5mm)

    // Sleep
    float sleep_linear_threshold = 0.01f;
    float sleep_angular_threshold = 0.01f;
    float sleep_timeout = 1.0f;           // seconds stationary before sleep

    // Collision
    float contact_break_distance = 0.02f; // max distance for persistent contacts
};
```

**Tuning tips:**
| Problem | Adjustment |
|---------|------------|
| Stacked boxes jitter | ↓ `baumgarte` (0.1), ↑ `solver_iterations` (20) |
| Objects sink into ground | ↑ `baumgarte` (0.25) |
| Objects too bouncy | ↓ `restitution` per body |
| Objects slide too much | ↑ `friction` per body |
| Sleep too aggressive | ↑ `sleep_timeout` (2.0) |
| Objects fall through floor | ↑ `max_substeps` (16), check `physics_dt` |
| NaN/explosion | Objects clamped at |position| > 1000; velocities reset if NaN |

---

### Math Types

The engine includes a hand-rolled math library. All types are header-only `struct` with inline operators.

#### vec3

```cpp
vec3 a(1, 2, 3);
vec3 b(4, 5, 6);
vec3 c = a + b;           // (5, 7, 9)
vec3 d = a - b;           // (-3, -3, -3)
vec3 e = a * 2.0f;        // (2, 4, 6)
vec3 f = a / 2.0f;        // (0.5, 1, 1.5)
float g = dot(a, b);      // 32
vec3 h = cross(a, b);     // (-3, 6, -3)
float len = length(a);    // sqrt(14) ≈ 3.742
vec3 norm = normalize(a); // unit vector in direction of a
```

#### quat

```cpp
quat q = quat_identity();                          // (1, 0, 0, 0)
quat r = quat_from_axis_angle(vec3(0, 0, 1), 1.57f); // 90° around Z
quat s = q * r;                                    // compose rotations
vec3 v(1, 0, 0);
vec3 rotated = rotate(r, v);                       // rotate vector
quat inv = conjugate(r);                           // inverse rotation
quat n = normalize(s);                             // ensure unit length
```

#### mat3

```cpp
mat3 I = mat3::identity();
mat3 D = mat3::diagonal(2, 3, 5);    // diag(2, 3, 5)
vec3 result = D * vec3(1, 1, 1);     // (2, 3, 5)
mat3 inv = inverse(D);                // diag(0.5, 0.333, 0.2)
mat3 T = transpose(D);                // same for diagonal
```

#### math_utils

```cpp
// Inertia tensors (body-frame)
mat3 Is = inertia_tensor_sphere(radius, mass);   // I = (2/5)mr²
mat3 Ib = inertia_tensor_box(hx, hy, hz, mass);  // Ixx,Iyy,Izz

// Quaternion ↔ matrix
mat3 R = quat_to_mat3(quat_from_axis_angle(vec3(0,1,0), 0.5f)); // rotation matrix

// Coordinate transforms
vec3 world_pt = local_to_world(local_pt, position, orientation);
vec3 local_pt = world_to_local(world_pt, position, orientation);
vec3 world_dir = local_to_world_direction(vec3(0, 1, 0), orientation);

// Utilities
float c = clampf(value, 0.0f, 1.0f);
float m = maxf(a, b);
float n = minf(a, b);
```

---

## Examples

See the `examples/` directory for self-contained demo scenes. Each example is a standalone C++ file.

| # | Example | What it demonstrates |
|---|---------|---------------------|
| 1 | [Bouncing Spheres](examples/01-bouncing-spheres.cpp) | Gravity, restitution, sphere-sphere collision, sphere-plane |
| 2 | [Box Stack](examples/02-box-stack.cpp) | Sequential impulse stability, friction, sleep |
| 3 | [Ramp Roll](examples/03-ramp-roll.cpp) | Oriented box as ramp, rolling sphere, angular velocity |
| 4 | [Gyroscope](examples/04-gyroscope.cpp) | Tennis racket theorem, free rotation, energy conservation |
| 5 | [Stress Test](examples/05-stress-test.cpp) | 200 bodies, broadphase performance, sleep efficiency |

To build and run an example:

```bash
cd build && cmake .. && make example_01_bounce && ./example_01_bounce
```

---

## Build & Run

### Directory Layout

```
07-3d-physics-engine/
├── src/
│   ├── math/          # vec3, quat, mat3, math_utils (header-only)
│   ├── core/          # RigidBody, World, SimulationParams, sleep
│   ├── collision/     # GJK, EPA, octree broadphase, contact structs
│   ├── shapes/        # Sphere, Box (OBB), Plane
│   ├── constraints/   # Sequential impulse solver
│   ├── render/        # Raylib debug draw
│   └── app/           # main.cpp (default demo)
├── examples/          # Standalone demo scenes
├── tests/             # Catch2 test suite (9 test files)
├── docs/              # ADRs, design docs, implementation plan
├── CMakeLists.txt     # Top-level build (src/)
├── tests/CMakeLists.txt
└── Dockerfile         # Reproducible dev container
```

### Build Configuration

```bash
# Debug build
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Release build (optimized)
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-O3 -march=native"

# Without raylib (headless — tests only, no rendering)
cmake .. -DRAYLIB_DISABLE=ON
```

### CMake Targets

```
physics_core       Static library (simulation core, no dependencies)
physics_render     Render layer (links raylib)
physics_sim        Default demo executable (links both)
example_01_bounce  Example 1
example_02_stack   Example 2
...                etc.
test_vec3          Unit test binary
...                etc.
```

---

## Configuration Guide

### Scene Setup Pattern

Every example and demo follows the same pattern:

```cpp
#include "core/world.h"
#include "shapes/sphere.h"
#include "shapes/box.h"
#include "shapes/plane.h"
#include "render/debug_draw.h"
#include "math/math_utils.h"

int main() {
    // 1. Create world
    World world;

    // 2. Tune parameters (optional)
    world.params().solver_iterations = 15;
    world.params().baumgarte = 0.15f;

    // 3. Add static bodies (ground, walls)
    auto ground = std::make_unique<RigidBody>();
    ground->set_mass(0.0f);
    ground->friction = 0.8f;
    ground->shape = std::make_unique<PlaneShape>(vec3(0, 1, 0), 0.0f);
    world.add_body(std::move(ground));

    // 4. Add dynamic bodies
    for (int i = 0; i < 5; i++) {
        auto body = std::make_unique<RigidBody>();
        body->position = vec3(0, 2.0f + i * 2.0f, 0);
        body->set_mass(1.0f);
        body->friction = 0.5f;
        body->restitution = 0.3f;

        // Assign shape
        body->shape = std::make_unique<SphereShape>(body->position, 0.5f);

        // Compute inertia
        body->inertia_tensor = inertia_tensor_sphere(0.5f, body->mass);
        body->inv_inertia_tensor = inverse(body->inertia_tensor);

        world.add_body(std::move(body));
    }

    // 5. Simulation loop
    while (!should_close) {
        float dt = get_frame_time_seconds();
        world.step(dt);

        // Render
        for (const auto& body : world.bodies()) {
            draw_body(body);
        }
    }
}
```

### Force Application Timing

Forces are applied at the **start of each substep** and **reset** after integration. Apply forces **before** `world.step()`:

```cpp
// ✅ CORRECT: Apply before step
auto& body = world.bodies()[0];
body->add_force(vec3(100, 0, 0));
world.step(dt);

// ❌ WRONG: Force applied after step won't take effect until next frame
world.step(dt);
body->add_force(vec3(100, 0, 0));
```

For continuous forces (e.g., player input), apply before **each** `world.step()` call:

```cpp
while (running) {
    // Apply per-frame forces
    for (auto& body : world.bodies()) {
        if (is_moving_left()) body->add_force(vec3(-50, 0, 0));
    }
    world.step(get_frame_time());
}
```

---

## Testing

9 test files covering the full pipeline:

```bash
# Build and run all tests
cd build && cmake .. && make -j$(nproc)
ctest --output-on-failure

# Run specific test
./tests/test_vec3
./tests/test_gjk
./tests/test_integration
```

**Test summary:**

| Test | What it covers |
|------|---------------|
| `test_vec3` | All operators, dot, cross, length, normalize |
| `test_quat` | Multiplication, rotate vector, conjugate, normalize |
| `test_mat3` | Identity, diagonal, multiply, inverse, inertia tensors |
| `test_integration` | Gravity fall, static body, world inertia, quaternion drift, sleep, **box stack stability** |
| `test_gjk` | Sphere-sphere (overlap/separate), box-box, sphere-box |
| `test_epa` | Penetration depth accuracy for sphere-sphere |
| `test_solver` | Solver separates overlapping bodies |
| `test_broadphase` | Octree finds correct pairs, empty world |
| `test_sleep` | Sleep timeout, moving bodies stay awake, static never sleeps |

---

## Debug Visualization

The `DebugDraw` class (in `src/render/debug_draw.h`) renders physics state:

| Visual | What it means |
|--------|--------------|
| 🔴 Red sphere / 🔵 Blue box wireframe | Active dynamic body |
| ⬛ Dark gray sphere / box | Sleeping body (deactivated) |
| 🟡 Yellow line | Linear velocity vector (direction × magnitude) |
| 🟢 Green plane | Static ground/wall |
| 🔳 Gray grid | World-space reference (1m spacing) |

**Camera controls** (when using `CAMERA_ORBITAL`):
- Right-click + drag: Orbit
- Scroll: Zoom
- WASD: Pan (if enabled)

**Adding debug drawing to your own code:**
```cpp
#include "render/debug_draw.h"

Camera3D camera;
DebugDraw::setup_camera(camera);

// In render loop:
BeginMode3D(camera);
DebugDraw::draw_world(world);
EndMode3D();
```

---

## Troubleshooting

### Build Fails

| Symptom | Fix |
|---------|-----|
| `raylib.h not found` | Install raylib: `brew install raylib` (macOS) or `sudo apt install libraylib-dev` (Ubuntu) |
| `cmake: command not found` | Install CMake: `brew install cmake` or `sudo apt install cmake` |
| `C++17 required` | Upgrade compiler: GCC 8+, Clang 7+, or Apple Clang 11+ |
| No OpenGL | Install dev packages: `apt install libgl1-mesa-dev libglu1-mesa-dev freeglut3-dev` |

### Runtime Issues

| Symptom | Likely cause | Fix |
|---------|-------------|-----|
| Objects fly to infinity | Missing inertia tensor | Call `body->inertia_tensor = inertia_tensor_sphere(...)` then `body->inv_inertia_tensor = inverse(...)` |
| Objects pass through ground | Tunneling | ↑ `max_substeps` to 16, or make ground thicker |
| Box stack collapses immediately | Wrong inertia | Ensure `inertia_tensor_box(half_extent_x, half_extent_y, half_extent_z, mass)` |
| Nothing moves | All bodies static | Check `set_mass()` > 0 for dynamic bodies |
| Rotation looks wrong | Wrong angular velocity space | Body-space ω used; `add_force_at_point()` handles world→body transform automatically |
| Simulation freezes after few seconds | NaN propagation | Guard clamps prevent full collapse; check `physics_dt` is set before `step()` |

### Performance

| Issue | Fix |
|-------|-----|
| Low FPS with 100+ bodies | ↓ `solver_iterations` (10), enable sleep (already on), check broadphase is working |
| High CPU in sleep state | Sleep deactivates stationary bodies — make sure `sleep_timeout` isn't too high |
| Broadphase slower than brute force | For < 50 objects, octree overhead exceeds brute force AABB check |

---

## Gotchas

This engine is a **toy** — it deliberately simplifies several things. Know these before depending on it:

1. **Single contact point per pair** — boxes stack with fewer rotational constraints than production engines. Box stacks may lean slightly before settling.

2. **Cold-start solver** — no warm-starting contact persistence. Uses 15 solver iterations to compensate. Stacks converge more slowly than engines with warm-starting.

3. **Simplified EPA** — no edge tracking. Works correctly for sphere-sphere and axis-aligned box-box. Edge-edge contacts may produce approximate penetration depth.

4. **No CCD** (continuous collision detection) — fast-moving objects can tunnel through thin walls. Use smaller `physics_dt` or more substeps to mitigate.

5. **First-order integration** — semi-implicit Euler drifts energy over long simulations. Fine for interactive use, not suitable for orbital mechanics.

6. **No joints or constraints** beyond contacts — no hinges, springs, motors, or ragdolls.

7. **Single shape per body** — no compound shapes. Each body has exactly one collision shape.

See [`gotchas.md`](gotchas.md) for detailed explanations of all 12 categories of pitfalls.

---

## Project Structure

```
src/
├── math/
│   ├── vec3.h           (51 lines)  3D vector + operators
│   ├── quat.h           (68 lines)   Quaternion + rotate, conjugate
│   ├── mat3.h           (98 lines)   3×3 matrix (column-major) + inverse
│   └── math_utils.h     (58 lines)   Inertia formulas, transforms, clamp
├── core/
│   ├── rigid_body.h/cpp (37 + 43)   State + force accumulation + inertia
│   ├── world.h/cpp      (31 + 118)  Fixed-timestep simulation loop
│   ├── simulation.h     (17 lines)  All tunable parameters
│   └── sleep.h          (24 lines)  Sleep detection + wake
├── collision/
│   ├── contact.h        (23 lines)  ContactPoint, ContactManifold structs
│   ├── gjk.h            (131 lines) GJK with line/triangle/tetrahedron simplex
│   ├── epa.h            (83 lines)  Simplified EPA penetration
│   ├── broadphase.h/cpp (36 + 98)   Octree spatial partitioning
├── shapes/
│   ├── shape.h          (12 lines)  CollisionShape base (ShapeType enum)
│   ├── sphere.h         (21 lines)  Sphere support + AABB
│   ├── box.h            (51 lines)  OBB support + AABB (8 corners → extents)
│   └── plane.h          (31 lines)  Infinite plane (static only)
├── constraints/
│   ├── solver.h/cpp     (24 + 86)   Sequential impulse + Baumgarte + post-damping
├── render/
│   └── debug_draw.h     (85 lines)  Raylib wireframe + velocity vectors
└── app/
    └── main.cpp         (77 lines)  Default demo (5 boxes + 20 spheres)
```

---

## References

### Reference implementations (studied during design)

| Project | Language | Key Feature |
|---------|----------|-------------|
| [raw-physics](https://github.com/felipeek/raw-physics) | C | XPBD rigid body, GJK+EPA, Sutherland-Hodgman manifold |
| [Riemann](https://github.com/atlantis13579/Riemann) | C++ | Collision detection + physics, zero deps, BVH + SAP |
| [basic-physics](https://github.com/george7378/basic-physics) | C# | Minimal rigid body, penalty method, inertia tensor |
| [physics3D](https://github.com/NathanMacLeod/physics3D) | C++ | Rotors/quaternions, octree broadphase, gyroscopic motion |
| [slrbs](https://github.com/sheldona/slrbs) | C++ | Educational rigid body simulator (Eigen + Polyscope) |

### Key papers & resources
- **GJK**: [A Fast and Robust GJK Implementation for Collision Detection of Convex Objects](https://graphics.stanford.edu/courses/cs448b-00-winter/papers/gjk.pdf) — van den Bergen
- **EPA**: [Proximity Queries and Penetration Depth Computation on 3D Game Objects](https://graphics.stanford.edu/courses/cs348b-03-spring/papers/van_den_bergen.pdf) — van den Bergen
- **Sequential Impulse**: [Iterative Dynamics with Temporal Coherence](http://box2d.org/files/GDC2005/ErinCatto_GDC2005_IterativeDynamics.pdf) — Erin Catto (Box2D)
- **Tennis Racket Theorem**: [Dzhanibekov effect](https://en.wikipedia.org/wiki/Tennis_racket_theorem) — intermediate axis instability
- **Loose Octree**: [Game Programming Gems 2](https://www.gamedevs.org/uploads/octree-book.pdf) — Thatcher Ulrich

---

## License

This is a toy project for educational purposes. Do whatever you want with it.
