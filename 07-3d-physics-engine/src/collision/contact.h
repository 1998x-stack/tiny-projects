#pragma once
#include <vector>
#include "math/vec3.h"

struct RigidBody;

struct ContactPoint {
    vec3 point_a;
    vec3 point_b;
    vec3 normal;
    float penetration = 0.0f;
    float normal_impulse = 0.0f;
    float tangent_impulse_1 = 0.0f;
    float tangent_impulse_2 = 0.0f;
};

struct ContactManifold {
    RigidBody* body_a = nullptr;
    RigidBody* body_b = nullptr;
    vec3 normal;
    std::vector<ContactPoint> points;
    float friction = 0.5f;
    float restitution = 0.3f;
};
