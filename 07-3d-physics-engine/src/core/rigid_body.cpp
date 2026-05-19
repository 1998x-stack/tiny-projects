#include "core/rigid_body.h"
#include "math/math_utils.h"

void RigidBody::add_force(const vec3& force) {
    if (!is_dynamic()) return;
    force_accum += force;
}

void RigidBody::add_force_at_point(const vec3& force, const vec3& world_point) {
    if (!is_dynamic()) return;
    force_accum += force;
    vec3 r = world_point - position;
    torque_accum += cross(r, force);
}

void RigidBody::add_torque(const vec3& torque) {
    if (!is_dynamic()) return;
    torque_accum += torque;
}

void RigidBody::set_mass(float m) {
    mass = m;
    if (m > 0.0f) {
        inv_mass = 1.0f / m;
    } else {
        inv_mass = 0.0f;
        inertia_tensor = mat3::identity();
        inv_inertia_tensor = mat3::identity();
    }
}

mat3 RigidBody::world_inertia() const {
    mat3 R = quat_to_mat3(orientation);
    mat3 R_T = transpose(R);
    return R * inertia_tensor * R_T;
}

mat3 RigidBody::world_inv_inertia() const {
    mat3 R = quat_to_mat3(orientation);
    mat3 R_T = transpose(R);
    return R * inv_inertia_tensor * R_T;
}
