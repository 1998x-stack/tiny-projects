#pragma once
#include <memory>
#include "math/vec3.h"
#include "math/quat.h"
#include "math/mat3.h"

struct CollisionShape;

struct RigidBody {
    vec3 position;
    quat orientation;
    vec3 linear_velocity;
    vec3 angular_velocity;

    float mass = 1.0f;
    float inv_mass = 1.0f;
    mat3 inertia_tensor = mat3::identity();
    mat3 inv_inertia_tensor = mat3::identity();
    float restitution = 0.3f;
    float friction = 0.5f;

    vec3 force_accum;
    vec3 torque_accum;

    std::unique_ptr<CollisionShape> shape;

    bool is_sleeping = false;
    float sleep_timer = 0.0f;

    void add_force(const vec3& force);
    void add_force_at_point(const vec3& force, const vec3& world_point);
    void add_torque(const vec3& torque);
    bool is_dynamic() const { return mass > 0.0f; }
    void set_mass(float m);
    mat3 world_inertia() const;
    mat3 world_inv_inertia() const;
};
