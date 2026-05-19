#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include "math/quat.h"
#include "math/vec3.h"

TEST_CASE("quat default is identity", "[quat]") {
    quat q;
    REQUIRE(q.w == 1.0f);
    REQUIRE(q.x == 0.0f);
    REQUIRE(q.y == 0.0f);
    REQUIRE(q.z == 0.0f);
}

TEST_CASE("quat multiplication", "[quat]") {
    quat a(0.707f, 0.707f, 0, 0);
    quat b(0.707f, 0, 0.707f, 0);
    quat c = a * b;
    REQUIRE(length(c) == Approx(1.0f));
}

TEST_CASE("quat rotate vector", "[quat]") {
    quat q(0.70710678f, 0, 0, 0.70710678f);
    vec3 v(1, 0, 0);
    vec3 r = rotate(q, v);
    REQUIRE(r.x == Approx(0.0f).margin(0.001f));
    REQUIRE(r.y == Approx(1.0f).margin(0.001f));
    REQUIRE(r.z == Approx(0.0f).margin(0.001f));
}

TEST_CASE("quat normalize preserves rotation", "[quat]") {
    quat q(0.5f, 0.5f, 0.5f, 0.5f);
    quat n = normalize(q);
    REQUIRE(length(n) == Approx(1.0f));
}

TEST_CASE("quat conjugate", "[quat]") {
    quat q(0.707f, 0.707f, 0, 0);
    quat c = conjugate(q);
    quat result = q * c;
    REQUIRE(result.w == Approx(1.0f));
    REQUIRE(std::abs(result.x) < 0.001f);
}
