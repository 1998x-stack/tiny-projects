# 3D Physics Engine — Gotchas

## 1. Tunneling (Objects Passing Through Each Other)

**Problem:** Fast-moving objects can pass entirely through thin walls in a single time step.

```
Frame N:   [●]  ──────────────►  |wall|
Frame N+1:                        |wall|  [●]    ← passed through!
```

**Solutions:**
- **CCD (Continuous Collision Detection):** Sweep shapes along velocity vector and test at first contact time
- **Smaller timesteps:** Subdivide physics step (e.g., 4 substeps of 4ms instead of 1 step of 16ms)
- **Thicker collision geometry:** Inflate thin walls

**Toy engine:** Use substeps (4-8 per frame), accept that very thin objects may still tunnel.

## 2. Quaternion Normalization Drift

**Problem:** Repeated quaternion integration causes `|q| ≠ 1`. Non-unit quaternion → scaling of rotated vectors.

**Fix:** Normalize after every integration step:
```cpp
body.orientation = normalize(body.orientation);
```

**Gotcha:** After MANY steps, even normalized quaternions accumulate error. Can re-orthogonalize periodically.

## 3. GJK Degenerate Cases

**Problem:** GJK can fail or loop infinitely in edge cases:
- Touching shapes (distance ≈ 0)
- Parallel faces (no unique closest point)
- Shapes exactly interpenetrating

**Mitigations:**
- Max iterations: 32 (should converge quickly for convex shapes)
- Epsilon for zero-distance check
- When GJK returns simplex containing origin, switch to EPA for penetration info
- If EPA fails, fall back to face clipping

## 4. EPA Failure on Near-Coplanar Faces

**Problem:** EPA expands the polytope by finding the closest face. If multiple faces are equally close (coplanar case), the algorithm can oscillate or pick the wrong face.

**Mitigations:**
- Face selection with tie-breaking (e.g., largest area)
- Max iterations with early termination
- Degenerate face removal (merge coplanar faces)

## 5. Constraint Solver Oscillation (Jitter)

**Problem:** Stacked objects vibrate because position correction overcorrects.

**Causes:**
- Baumgarte bias too high → overshoot
- Not enough solver iterations → constraints not globally resolved
- Low mass ratio (e.g., light object under heavy object)

**Fixes:**
- `baumgarte = 0.1` (conservative, slow correction)
- 8-10 solver iterations
- Warm-starting (carry over impulses from previous frame)
- Post-solve velocity clamping for nearly-resting contacts

## 6. Numerical Instability in Semi-Implicit Euler

**Problem:** Semi-implicit Euler is symplectic but still first-order. Energy can drift over time.

**When it matters:** Long simulations (minutes), pendulums, orbits.

**Toy engine acceptance:** Semi-implicit Euler is good enough for game-like simulations with damping.

**Alternative:** RK4 (4th order Runge-Kutta) for better energy conservation, but more expensive.

## 7. Inertia Tensor Computation

**Problem:** Computing the 3×3 inertia tensor from a triangle mesh requires integrating over the volume.

**For basic shapes (analytical formulas):**

```
Solid sphere (radius r, mass m):
  I = (2/5) * m * r² * identity

Solid box (half-extents hx, hy, hz, mass m):
  Ixx = (1/12) * m * (hy² + hz²)
  Iyy = (1/12) * m * (hx² + hz²)
  Izz = (1/12) * m * (hx² + hy²)

Solid capsule (radius r, height h, mass m):
  // hemisphere + cylinder decomposition
  // See Game Programming Gems or similar reference
```

**Gotcha:** Inertia tensor must be in BODY frame, not world frame. Transform with rotation matrix: `I_world = R * I_body * R^T`.

## 8. Friction Model Instability

**Coulomb friction model:**
```
friction_impulse ≤ μ * normal_impulse  (friction cone)
```

**Gotcha:** Friction impulse can ADD energy to the system (violating conservation). Clamp tangential velocity change.

**Box2D approach:** Clamp tangential impulse to friction cone, then apply:
```cpp
float max_friction = contact.friction * normal_impulse;
tangent_impulse = clamp(tangent_impulse, -max_friction, max_friction);
```

## 9. Contact Manifold Stability

**Problem:** Contact points appearing/disappearing between frames → visual popping.

**Persistence:** Track contact points across frames. Match by ID or proximity. Warm-start solver with previous frame's impulses.

**Contact reduction:** For box-box contact, up to 8 contact points from Sutherland-Hodgman. Reduce to 4 most significant (largest penetration depth).

## 10. Gyroscopic Torque — Energy Gain

**Problem:** Explicit integration of angular velocity can add energy.

The gyroscopic term is `τ_gyro = ω × (I · ω)`. Explicit integration:
```
ω += I_inv * (τ - ω × (I · ω)) * dt
```

This can cause angular velocity to diverge (spin faster and faster).

**Fix (from Box2D/Erin Catto):** Implicit integration of gyroscopic term:
```cpp
// Implicit gyroscopic integration
quat dq = quat(0, ω) * orientation;
dq = 0.5 * dq * dt;
orientation += dq;
// Don't explicitly integrate gyroscopic torque — 
// orientation quaternion handles it implicitly
```

## 11. Octree Broadphase — Static vs Dynamic

**Problem:** Rebuilding octree every frame is expensive.

**Optimization:** Insert objects into octree each frame (O(n log n)). Only update moved objects.

**Loose octree:** Nodes are 2× larger than strict bounds. Reduces object migration between nodes.

**For < 100 objects:** Brute force n² is actually faster than spatial partitioning overhead.

## 12. Debug Visualization Checklist

When your simulation explodes, render these debug visuals:
- [ ] Wireframe collision shapes (are they where you think?)
- [ ] Contact points (red dots) and normals (green arrows)
- [ ] Linear/angular velocity vectors (blue arrows)
- [ ] Center of mass markers
- [ ] Octree bounds (yellow wireframe)
- [ ] Sleeping object indicators (greyed out)
