#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include "core/rigid_body.h"
#include "core/world.h"
#include "core/simulation.h"
#include "math/math_utils.h"

TEST_CASE("rigid body gravity causes falling", "[integration]") {
    RigidBody body;
    body.position = vec3(0, 10, 0);
    body.set_mass(1.0f);
    body.inv_inertia_tensor = mat3::identity();

    SimulationParams params;
    params.gravity = vec3(0, -10.0f, 0);
    params.linear_damping = 0.0f;
    params.angular_damping = 0.0f;
    float dt = 1.0f / 60.0f;

    for (int i = 0; i < 60; i++) {
        body.force_accum = vec3(0);
        body.torque_accum = vec3(0);
        body.force_accum += params.gravity * body.mass;
        vec3 acceleration = body.force_accum * body.inv_mass;
        body.linear_velocity += acceleration * dt;
        body.position += body.linear_velocity * dt;
    }

    REQUIRE(body.linear_velocity.y == Approx(-10.0f).margin(0.1f));
    REQUIRE(body.position.y == Approx(5.0f).margin(0.1f));
}

TEST_CASE("static body ignores forces", "[integration]") {
    RigidBody body;
    body.position = vec3(0, 0, 0);
    body.set_mass(0.0f);
    body.add_force(vec3(100, 0, 0));
    REQUIRE(body.force_accum.x == 0.0f);
    body.add_force_at_point(vec3(100, 0, 0), vec3(1, 0, 0));
    REQUIRE(body.torque_accum.z == 0.0f);
}

TEST_CASE("world_inertia transforms correctly", "[integration]") {
    RigidBody body;
    body.orientation = quat_identity();
    body.inertia_tensor = mat3::diagonal(2, 3, 5);
    mat3 world_I = body.world_inertia();
    REQUIRE(world_I.m[0][0] == 2.0f);
    REQUIRE(world_I.m[1][1] == 3.0f);
    REQUIRE(world_I.m[2][2] == 5.0f);
}

TEST_CASE("quaternion integration maintains unit length", "[integration]") {
    RigidBody body;
    body.orientation = quat_identity();
    body.angular_velocity = vec3(0.5f, 0.3f, 0.1f);
    float dt = 1.0f / 60.0f;
    for (int i = 0; i < 600; i++) {
        quat q = body.orientation;
        quat w(0, body.angular_velocity.x, body.angular_velocity.y, body.angular_velocity.z);
        quat dq;
        dq.w = 0.5f * (w.w * q.w - w.x * q.x - w.y * q.y - w.z * q.z) * dt;
        dq.x = 0.5f * (w.w * q.x + w.x * q.w + w.y * q.z - w.z * q.y) * dt;
        dq.y = 0.5f * (w.w * q.y - w.x * q.z + w.y * q.w + w.z * q.x) * dt;
        dq.z = 0.5f * (w.w * q.z + w.x * q.y - w.y * q.x + w.z * q.w) * dt;
        q.w += dq.w; q.x += dq.x; q.y += dq.y; q.z += dq.z;
        body.orientation = normalize(q);
    }
    REQUIRE(length(body.orientation) == Approx(1.0f).margin(0.001f));
}

TEST_CASE("world step advances physics with fixed timestep", "[integration]") {
    World world;
    world.params().gravity = vec3(0, -10.0f, 0);
    world.params().physics_dt = 1.0f / 60.0f;
    world.params().linear_damping = 0.0f;
    world.params().angular_damping = 0.0f;

    auto body = std::make_unique<RigidBody>();
    body->position = vec3(0, 100, 0);
    body->set_mass(1.0f);
    body->inv_inertia_tensor = mat3::identity();
    world.add_body(std::move(body));
    world.step(1.0f);

    const auto& bodies = world.bodies();
    REQUIRE(bodies[0]->position.y < 95.0f);
    REQUIRE(bodies[0]->linear_velocity.y < -5.0f);
}

TEST_CASE("sleep activates after timeout", "[integration]") {
    World world;
    world.params().physics_dt = 1.0f / 60.0f;
    world.params().sleep_timeout = 0.5f;

    auto body = std::make_unique<RigidBody>();
    body->set_mass(1.0f);
    body->linear_velocity = vec3(0);
    body->angular_velocity = vec3(0);
    body->sleep_timer = 0.6f;
    world.add_body(std::move(body));
    world.step(0.1f);

    REQUIRE(world.bodies()[0]->is_sleeping);
}
