#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include "core/sleep.h"

TEST_CASE("body goes to sleep after timeout", "[sleep]") {
    RigidBody body;
    body.set_mass(1.0f);
    body.linear_velocity = vec3(0);
    body.angular_velocity = vec3(0);
    SimulationParams params;
    params.sleep_timeout = 1.0f;
    update_sleep_body(body, 1.1f, params);
    REQUIRE(body.is_sleeping);
    REQUIRE(body.linear_velocity.x == 0.0f);
}

TEST_CASE("moving body stays awake", "[sleep]") {
    RigidBody body;
    body.set_mass(1.0f);
    body.linear_velocity = vec3(5, 0, 0);
    SimulationParams params;
    update_sleep_body(body, 10.0f, params);
    REQUIRE_FALSE(body.is_sleeping);
}

TEST_CASE("static body never sleeps", "[sleep]") {
    RigidBody body;
    body.set_mass(0.0f);
    SimulationParams params;
    update_sleep_body(body, 10.0f, params);
    REQUIRE_FALSE(body.is_sleeping);
}
