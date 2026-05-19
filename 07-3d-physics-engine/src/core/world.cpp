#include "core/world.h"
#include "math/math_utils.h"
#include "shapes/shape.h"
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
}

void World::solve_constraints(float dt) {
    (void)dt;
}

void World::update_sleep(float dt) {
    for (auto& body : m_bodies) {
        if (!body->is_dynamic()) continue;
        if (length(body->linear_velocity) < m_params.sleep_linear_threshold &&
            length(body->angular_velocity) < m_params.sleep_angular_threshold) {
            body->sleep_timer += dt;
            if (body->sleep_timer > m_params.sleep_timeout && !body->is_sleeping) {
                body->is_sleeping = true;
                body->linear_velocity = vec3(0);
                body->angular_velocity = vec3(0);
            }
        } else {
            body->sleep_timer = 0.0f;
            body->is_sleeping = false;
        }
    }
}
