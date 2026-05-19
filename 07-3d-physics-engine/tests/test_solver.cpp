#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include "constraints/solver.h"
#include "core/simulation.h"

TEST_CASE("solver separates overlapping bodies", "[solver]") {
    RigidBody a, b;
    a.position = vec3(0, 0, 0);
    a.set_mass(1.0f);
    a.inv_inertia_tensor = mat3::identity();
    a.restitution = 0.0f;

    b.position = vec3(0, 5, 0);
    b.set_mass(1.0f);
    b.inv_inertia_tensor = mat3::identity();
    b.linear_velocity = vec3(0, -2, 0);
    b.restitution = 0.0f;

    ContactManifold m;
    m.body_a = &a;
    m.body_b = &b;
    m.normal = vec3(0, 1, 0);
    m.friction = 0.5f;
    m.restitution = 0.0f;

    ContactPoint cp;
    cp.point_a = a.position;
    cp.point_b = b.position;
    cp.penetration = 0.5f;
    cp.normal = m.normal;
    m.points.push_back(cp);

    SolverInput input;
    input.manifolds.push_back(m);
    input.dt = 1.0f / 60.0f;
    SimulationParams params;
    params.baumgarte = 0.2f;
    params.solver_iterations = 15;
    input.params = params;

    ConstraintSolver::solve(input);
    REQUIRE(b.linear_velocity.y > -2.0f);
}
