#include "constraints/solver.h"
#include "math/math_utils.h"
#include <cmath>

vec3 ConstraintSolver::compute_relative_velocity(const RigidBody& a, const RigidBody& b,
                                                   const vec3& point_a, const vec3& point_b) {
    vec3 r_a = point_a - a.position;
    vec3 r_b = point_b - b.position;
    vec3 vel_a = a.linear_velocity + cross(a.angular_velocity, r_a);
    vec3 vel_b = b.linear_velocity + cross(b.angular_velocity, r_b);
    return vel_a - vel_b;
}

void ConstraintSolver::apply_impulse(RigidBody& a, RigidBody& b,
                                      const vec3& impulse, const vec3& point_a, const vec3& point_b) {
    if (a.is_dynamic()) {
        a.linear_velocity += impulse * a.inv_mass;
        vec3 r_a = point_a - a.position;
        vec3 torque_impulse = cross(r_a, impulse);
        vec3 torque_body = rotate(conjugate(a.orientation), torque_impulse);
        a.angular_velocity += a.inv_inertia_tensor * torque_body;
    }
    if (b.is_dynamic()) {
        b.linear_velocity -= impulse * b.inv_mass;
        vec3 r_b = point_b - b.position;
        vec3 torque_impulse = cross(r_b, impulse);
        vec3 torque_body = rotate(conjugate(b.orientation), torque_impulse);
        b.angular_velocity -= b.inv_inertia_tensor * torque_body;
    }
}

void ConstraintSolver::solve_contact(ContactManifold& manifold,
                                      const SimulationParams& params, float dt) {
    RigidBody& a = *manifold.body_a;
    RigidBody& b = *manifold.body_b;
    vec3 normal = manifold.normal;

    for (auto& contact : manifold.points) {
        for (int iter = 0; iter < params.solver_iterations; iter++) {
            vec3 v_rel = compute_relative_velocity(a, b, contact.point_a, contact.point_b);
            float v_n = dot(v_rel, normal);

            float inv_mass_sum = 0.0f;
            if (a.is_dynamic()) inv_mass_sum += a.inv_mass;
            if (b.is_dynamic()) inv_mass_sum += b.inv_mass;

            float bias = (params.baumgarte / dt) * maxf(contact.penetration - params.penetration_slop, 0.0f);
            float effective_mass = 1.0f / (inv_mass_sum + 0.0001f);
            float delta_normal = -(v_n + bias) * effective_mass;

            if (std::isnan(delta_normal) || delta_normal > 1000.0f) continue;

            float new_impulse = maxf(contact.normal_impulse + delta_normal, 0.0f);
            delta_normal = new_impulse - contact.normal_impulse;
            contact.normal_impulse = new_impulse;

            apply_impulse(a, b, normal * delta_normal, contact.point_a, contact.point_b);

            vec3 v_rel_new = compute_relative_velocity(a, b, contact.point_a, contact.point_b);
            vec3 tangent_vel = v_rel_new - normal * dot(v_rel_new, normal);
            float tangent_speed = length(tangent_vel);

            if (tangent_speed > 0.0001f) {
                vec3 tangent_dir = tangent_vel / tangent_speed;
                float max_friction = manifold.friction * contact.normal_impulse;
                float v_t = dot(v_rel_new, tangent_dir);
                float delta_tangent = -v_t * effective_mass;
                delta_tangent = clampf(delta_tangent, -max_friction, max_friction);
                apply_impulse(a, b, tangent_dir * delta_tangent, contact.point_a, contact.point_b);
            }
        }
    }
}

void ConstraintSolver::solve(const SolverInput& input) {
    for (auto& manifold : input.manifolds) {
        solve_contact(manifold, input.params, input.dt);
    }

    for (auto& m : input.manifolds) {
        for (auto& contact : m.points) {
            vec3 v_rel = compute_relative_velocity(*m.body_a, *m.body_b, contact.point_a, contact.point_b);
            if (length(v_rel) < 0.01f) {
                vec3 damping_impulse = v_rel * -0.5f;
                apply_impulse(*m.body_a, *m.body_b, damping_impulse, contact.point_a, contact.point_b);
            }
        }
    }
}
