#pragma once
#include <vector>
#include "core/simulation.h"
#include "core/rigid_body.h"
#include "collision/contact.h"

struct SolverInput {
    std::vector<ContactManifold> manifolds;
    float dt;
    const SimulationParams& params;
};

class ConstraintSolver {
public:
    static void solve(const SolverInput& input);

private:
    static void solve_contact(ContactManifold& manifold, const SimulationParams& params, float dt);
    static vec3 compute_relative_velocity(const RigidBody& a, const RigidBody& b,
                                           const vec3& point_a, const vec3& point_b);
    static void apply_impulse(RigidBody& a, RigidBody& b,
                               const vec3& impulse, const vec3& point_a, const vec3& point_b);
};
