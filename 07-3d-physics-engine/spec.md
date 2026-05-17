# 3D Physics Engine — Specification

> Based on: raw-physics, Riemann, basic-physics, physics3D, slrbs

## References

| Project | Stars | Language | Key Features |
|---------|-------|----------|--------------|
| [raw-physics](https://github.com/felipeek/raw-physics) | 196 | C | XPBD rigid body, GJK+EPA, Sutherland-Hodgman manifold |
| [Riemann](https://github.com/atlantis13579/Riemann) | — | C++ | Collision detection + physics, zero dependencies, BVH + SAP |
| [basic-physics](https://github.com/george7378/basic-physics) | — | C# | Minimal rigid body, penalty method, inertia tensor |
| [physics3D](https://github.com/NathanMacLeod/physics3D) | — | C++ | Rotors/quaternions, octree broadphase, gyroscopic motion |
| [slrbs](https://github.com/sheldona/slrbs) | — | C++ | Educational rigid body simulator (Eigen + Polyscope) |

## Architecture Overview

```
┌─────────────────────────────────────────────────────┐
│                   Simulation Loop                     │
│                                                       │
│  ┌─────────┐   ┌──────────┐   ┌───────────────┐     │
│  │ Forces  │──►│Integrate │──►│Collision      │     │
│  │(gravity,│   │(velocity,│   │Detection      │     │
│  │ damping)│   │position) │   │(broad+narrow) │     │
│  └─────────┘   └──────────┘   └───────┬───────┘     │
│                                        │             │
│                                        ▼             │
│                              ┌───────────────┐      │
│                              │Constraint     │      │
│                              │Solver         │      │
│                              │(impulses,     │      │
│                              │position fix)  │      │
│                              └───────────────┘      │
│                                        │             │
│                                        ▼             │
│                              ┌───────────────┐      │
│                              │Update         │      │
│                              │Velocities     │      │
│                              └───────────────┘      │
└─────────────────────────────────────────────────────┘
```

## Feature Specification

### 1. Rigid Body Representation

**State vector (per body):**
```
position:       vec3    (x, y, z)          — center of mass
orientation:    quat    (w, x, y, z)       — rotation
linear_velocity:  vec3  (vx, vy, vz)
angular_velocity: vec3  (ωx, ωy, ωz)
```

**Constant properties (per body):**
```
mass:           float              — total mass (kg)
inertia_tensor: mat3              — 3×3 inertia (diagonal for symmetric)
inertia_inv:    mat3              — inverse inertia (for torque→angular accel)
restitution:    float   [0, 1]    — bounciness
friction:       float   [0, 1]    — Coulomb friction coefficient
```

```cpp
struct RigidBody {
    // State
    vec3 position;
    quat orientation;
    vec3 linear_velocity;
    vec3 angular_velocity;

    // Constants
    float mass;
    float inv_mass;       // 0 for static objects
    mat3 inertia_tensor;
    mat3 inv_inertia_tensor;
    float restitution;
    float friction;

    // Accumulators (reset each frame)
    vec3 force_accum;
    vec3 torque_accum;

    // Collision shape
    CollisionShape* shape;
};
```

### 2. Integration

**Semi-implicit Euler (symplectic) — recommended for stability:**

```
function integrate(body, dt):
    // Linear
    acceleration = body.force_accum * body.inv_mass
    body.linear_velocity += acceleration * dt
    body.position += body.linear_velocity * dt

    // Angular
    angular_accel = body.inv_inertia_tensor * body.torque_accum
    body.angular_velocity += angular_accel * dt

    // Orientation update (quaternion derivative)
    q = body.orientation
    w = quat(0, body.angular_velocity)  // pure quaternion
    dq = 0.5 * w * q * dt
    body.orientation = normalize(q + dq)

    // Reset accumulators
    body.force_accum = vec3(0)
    body.torque_accum = vec3(0)
```

**Forces applied each frame:**
```cpp
void apply_gravity(RigidBody& body) {
    body.force_accum += vec3(0, -9.81, 0) * body.mass;
}

void apply_damping(RigidBody& body, float linear_damp, float angular_damp) {
    body.force_accum -= body.linear_velocity * linear_damp;
    body.torque_accum -= body.angular_velocity * angular_damp;
}
```

### 3. Collision Detection — Broad Phase

**Octree spatial partitioning:**
```
function build_octree(bodies):
    root = OctreeNode(world_bounds)
    for body in bodies:
        insert(root, body)
    return root

function find_pairs(node, pairs):
    if node.body_count < 2: return
    if node.is_leaf:
        // All bodies in this node should be tested against each other
        for i in range(node.bodies):
            for j in range(i+1, node.bodies):
                pairs.add((node.bodies[i], node.bodies[j]))
    else:
        for child in node.children:
            find_pairs(child, pairs)
```

**Simpler alternative: Sweep and Prune (SAP)**
- Sort bodies by min X coordinate
- Maintain overlapping intervals
- Only test pairs with overlapping AABBs in X

### 4. Collision Detection — Narrow Phase

**GJK Algorithm (Gilbert–Johnson–Keerthi):**

Distance between two convex shapes using Minkowski difference.

```
function GJK(shape_a, shape_b):
    simplex = []  // up to 4 points in 3D
    direction = shape_a.center - shape_b.center  // initial search direction
    while True:
        point = support(shape_a, shape_b, direction)  // furthest point in direction
        if dot(point, direction) < 0:
            return NO_COLLISION
        simplex.append(point)
        if update_simplex(simplex, direction):  // simplex contains origin?
            return COLLISION, simplex
```

**EPA Algorithm (Expanding Polytope Algorithm):**

Given GJK's simplex containing origin, EPA expands it to find the closest face → penetration depth + normal.

```
function EPA(shape_a, shape_b, simplex):
    polytope = convex_hull_from_simplex(simplex)
    while True:
        closest_face = find_closest_face_to_origin(polytope)
        point = support(shape_a, shape_b, closest_face.normal)
        if dot(point, closest_face.normal) - closest_face.distance < epsilon:
            return closest_face.normal, closest_face.distance  // penetration normal + depth
        polytope.add_face(point)
```

### 5. Collision Response

**XPBD (Extended Position Based Dynamics) approach:**

```
function solve_collisions(contacts, dt):
    for iteration in range(num_iterations):
        for contact in contacts:
            // Compute penetration
            penetration = contact.depth

            // Compute inverse masses at contact point
            w1 = compute_generalized_inv_mass(body_a, contact.point, contact.normal)
            w2 = compute_generalized_inv_mass(body_b, contact.point, contact.normal)
            w = w1 + w2

            // Position correction (Baumgarte stabilization)
            bias = (bias_factor / dt) * max(penetration - slop, 0)
            delta_lambda = -(penetration + bias) / (w + compliance/dt²)

            // Apply position correction
            apply_position_update(body_a, delta_lambda * contact.normal, contact.point)
            apply_position_update(body_b, -delta_lambda * contact.normal, contact.point)

            // Friction
            // ... tangential impulse based on Coulomb model ...
```

**Contact manifold generation:**
- Find contact points on each shape's surface
- Sutherland-Hodgman clipping for box-box contacts
- Face-face, edge-edge, vertex-face cases
- Reduce to max 4 contact points

### 6. Supported Collision Shapes

| Shape | Description | Support Function |
|-------|-------------|------------------|
| Sphere | center + radius | center + radius * normalize(direction) |
| Box (OBB) | center + 3 axes + 3 half-extents | center + sum(sign(dot(axis, dir)) * axis * half_extent) |
| Capsule | segment + radius | closest_point_on_segment + radius * normalize(dir) |
| Convex Hull | list of vertices | vertex with max dot(dir, v) |
| Plane | normal + offset | (infinite, only for static ground) |

### 7. Constraint Solver Details

**Sequential Impulse (iterative):**
- Apply one constraint at a time
- Multiple iterations converge to global solution
- Warm-starting: carry over impulses from previous frame

**Baumgarte stabilization for position drift:**
```cpp
float bias = (baumgarte / dt) * max(penetration - slop, 0.0f);
```
- `baumgarte`: 0.1–0.3 (higher = faster correction but jitter)
- `slop`: small tolerance (e.g., 0.005m) to avoid micro-corrections

### 8. Sleeping (Deactivation)

**Problem:** Objects at rest still consume simulation time each frame.

**Sleep detection:**
```cpp
if (length(body.linear_velocity) < SLEEP_THRESHOLD &&
    length(body.angular_velocity) < SLEEP_THRESHOLD) {
    body.sleep_timer += dt;
    if (body.sleep_timer > SLEEP_TIMEOUT) {
        body.is_sleeping = true;
        body.linear_velocity = vec3(0);
        body.angular_velocity = vec3(0);
    }
}
```

**Wake on collision:** When a moving object contacts a sleeping object → wake it.

## Development Roadmap

### Phase 1: Math Library (Week 1)
- vec3, quat, mat3 classes
- Cross product, dot product, quaternion multiply, normalize
- Inertia tensor computation for basic shapes

### Phase 2: Rigid Body + Integration (Week 2)
- RigidBody struct
- Force application (gravity, damping)
- Semi-implicit Euler integration
- Simple test: one falling sphere

### Phase 3: Collision Detection (Week 3-4)
- AABB computation
- GJK algorithm for any convex shape
- EPA for penetration depth
- Sphere-sphere, sphere-box, box-box tests

### Phase 4: Collision Response (Week 5-6)
- Contact manifold generation
- Sequential impulse constraint solver
- Friction (Coulomb model)
- Box stacking test

### Phase 5: Broadphase (Week 7)
- Octree or BVH spatial partitioning
- Integrate with narrow phase
- Performance benchmarks

### Phase 6: Visualization (Week 8)
- OpenGL or raylib rendering
- Debug draw (wireframes, contact points, normals)
- Interactive demo scene

## Success Criteria

1. Sphere falls under gravity and bounces on ground plane
2. Box stack of 5 boxes remains stable (no jitter or explosion)
3. Sphere rolls down an inclined plane
4. Two boxes collide in mid-air and respond correctly
5. Objects at rest go to sleep (velocity < threshold)
6. Octree broadphase reduces collision pairs significantly
7. Gyroscopic motion visible (tennis racket theorem)
8. Simulation runs at 60fps with 100+ objects
