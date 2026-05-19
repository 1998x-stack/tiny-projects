#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include "collision/broadphase.h"
#include "shapes/sphere.h"
#include "shapes/box.h"

TEST_CASE("octree finds overlapping pairs", "[broadphase]") {
    std::vector<std::unique_ptr<RigidBody>> bodies;

    auto body1 = std::make_unique<RigidBody>();
    body1->position = vec3(0, 0, 0);
    body1->shape = std::make_unique<SphereShape>(vec3(0, 0, 0), 1.0f);
    bodies.push_back(std::move(body1));

    auto body2 = std::make_unique<RigidBody>();
    body2->position = vec3(1.5f, 0, 0);
    body2->shape = std::make_unique<SphereShape>(vec3(1.5f, 0, 0), 1.0f);
    bodies.push_back(std::move(body2));

    auto body3 = std::make_unique<RigidBody>();
    body3->position = vec3(10, 0, 0);
    body3->shape = std::make_unique<SphereShape>(vec3(10, 0, 0), 1.0f);
    bodies.push_back(std::move(body3));

    auto pairs = OctreeBroadphase::find_pairs(bodies);
    REQUIRE(pairs.size() == 1);
    REQUIRE_FALSE(pairs[0].body_a == pairs[0].body_b);
}

TEST_CASE("octree empty world", "[broadphase]") {
    std::vector<std::unique_ptr<RigidBody>> bodies;
    auto pairs = OctreeBroadphase::find_pairs(bodies);
    REQUIRE(pairs.empty());
}
