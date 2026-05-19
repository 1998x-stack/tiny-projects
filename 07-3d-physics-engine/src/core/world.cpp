#include "core/world.h"
#include "math/math_utils.h"
#include "shapes/shape.h"
#include "collision/broadphase.h"
#include "collision/gjk.h"
#include "collision/epa.h"
#include "constraints/solver.h"
#include "core/sleep.h"
#include <algorithm>
#include <cmath>

World::World() {}

void World::add_body(std::unique_ptr<RigidBody> body) {
    m_bodies.push_back(std::move(body));
}

void World::step(float frame_time) {
    float dt = m_params.physics_dt;
    m_accumulator += frame_time;
    m_accumulator = std::min(m_accumulator, m_params.max_substeps * dt);

    while (m_accumulator >= dt) {
        apply_forces();
        integrate(dt);
        detect_collisions(dt);
        solve_constraints(dt);
        update_sleep(dt);
        m_accumulator -= dt;
    }
}

void World::apply_forces() {
    for (auto& body : m_bodies) {
        if (!body->is_dynamic()) continue;
        body->force_accum = vec3(0);
        body->torque_accum = vec3(0);
        body->force_accum += m_params.gravity * body->mass;
        body->force_accum -= body->linear_velocity * m_params.linear_damping;
        body->torque_accum -= body->angular_velocity * m_params.angular_damping;
    }
}

void World::integrate(float dt) {
    for (auto& body : m_bodies) {
        if (!body->is_dynamic() || body->is_sleeping) continue;

        vec3 acceleration = body->force_accum * body->inv_mass;
        body->linear_velocity += acceleration * dt;
        body->position += body->linear_velocity * dt;

        vec3 torque_body = rotate(conjugate(body->orientation), body->torque_accum);
        vec3 gyro = cross(body->angular_velocity, body->inertia_tensor * body->angular_velocity);
        vec3 angular_accel = body->inv_inertia_tensor * (torque_body - gyro);
        body->angular_velocity += angular_accel * dt;

        quat q = body->orientation;
        quat w(0, body->angular_velocity.x, body->angular_velocity.y, body->angular_velocity.z);
        quat dq;
        dq.w = 0.5f * (w.w * q.w - w.x * q.x - w.y * q.y - w.z * q.z) * dt;
        dq.x = 0.5f * (w.w * q.x + w.x * q.w + w.y * q.z - w.z * q.y) * dt;
        dq.y = 0.5f * (w.w * q.y - w.x * q.z + w.y * q.w + w.z * q.x) * dt;
        dq.z = 0.5f * (w.w * q.z + w.x * q.y - w.y * q.x + w.z * q.w) * dt;
        q.w += dq.w; q.x += dq.x; q.y += dq.y; q.z += dq.z;
        body->orientation = normalize(q);

        if (std::isnan(body->linear_velocity.x) || std::isnan(body->angular_velocity.x)) {
            body->linear_velocity = vec3(0);
            body->angular_velocity = vec3(0);
        }
        if (length(body->position) > 1000.0f) {
            body->position = vec3(0, 100, 0);
            body->linear_velocity = vec3(0);
        }
    }
}

void World::detect_collisions(float dt) {
    (void)dt;
    m_contacts.clear();

    auto pairs = OctreeBroadphase::find_pairs(m_bodies);

    for (const auto& pair : pairs) {
        if (pair.body_a->is_sleeping && pair.body_b->is_sleeping) continue;
        if (!pair.body_a->shape || !pair.body_b->shape) continue;
        if (!pair.body_a->is_dynamic() && !pair.body_b->is_dynamic()) continue;

        GJKResult gjk_result = gjk(*pair.body_a->shape, *pair.body_b->shape);
        if (!gjk_result.overlap) continue;

        EPAResult epa_result = epa(*pair.body_a->shape, *pair.body_b->shape, gjk_result.simplex);
        if (!epa_result.success || epa_result.depth <= 0.001f) continue;

        ContactManifold manifold;
        manifold.body_a = pair.body_a;
        manifold.body_b = pair.body_b;
        manifold.normal = epa_result.normal;
        manifold.friction = std::sqrt(pair.body_a->friction * pair.body_b->friction);
        manifold.restitution = maxf(pair.body_a->restitution, pair.body_b->restitution);

        ContactPoint cp;
        cp.normal = epa_result.normal;
        cp.penetration = epa_result.depth;
        cp.point_a = pair.body_a->position - epa_result.normal * (epa_result.depth * 0.5f);
        cp.point_b = pair.body_b->position + epa_result.normal * (epa_result.depth * 0.5f);
        manifold.points.push_back(cp);

        m_contacts.push_back(manifold);

        wake_body(*pair.body_a);
        wake_body(*pair.body_b);
    }
}

void World::solve_constraints(float dt) {
    SolverInput input;
    input.manifolds = m_contacts;
    input.dt = dt;
    input.params = m_params;
    ConstraintSolver::solve(input);
}

void World::update_sleep(float dt) {
    for (auto& body : m_bodies) {
        update_sleep_body(*body, dt, m_params);
    }
}
