#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include "math/vec3.h"

TEST_CASE("vec3 default constructor is zero", "[vec3]") {
    vec3 v;
    REQUIRE(v.x == 0.0f);
    REQUIRE(v.y == 0.0f);
    REQUIRE(v.z == 0.0f);
}

TEST_CASE("vec3 value constructor", "[vec3]") {
    vec3 v(1.0f, 2.0f, 3.0f);
    REQUIRE(v.x == 1.0f);
    REQUIRE(v.y == 2.0f);
    REQUIRE(v.z == 3.0f);
}

TEST_CASE("vec3 addition", "[vec3]") {
    vec3 a(1, 2, 3);
    vec3 b(4, 5, 6);
    vec3 c = a + b;
    REQUIRE(c.x == 5.0f);
    REQUIRE(c.y == 7.0f);
    REQUIRE(c.z == 9.0f);
}

TEST_CASE("vec3 subtraction", "[vec3]") {
    vec3 a(5, 7, 9);
    vec3 b(1, 2, 3);
    vec3 c = a - b;
    REQUIRE(c.x == 4.0f);
    REQUIRE(c.y == 5.0f);
    REQUIRE(c.z == 6.0f);
}

TEST_CASE("vec3 scalar multiplication", "[vec3]") {
    vec3 v(1, 2, 3);
    vec3 r = v * 2.0f;
    REQUIRE(r.x == 2.0f);
    REQUIRE(r.y == 4.0f);
    REQUIRE(r.z == 6.0f);
}

TEST_CASE("vec3 scalar division", "[vec3]") {
    vec3 v(2, 4, 6);
    vec3 r = v / 2.0f;
    REQUIRE(r.x == 1.0f);
    REQUIRE(r.y == 2.0f);
    REQUIRE(r.z == 3.0f);
}

TEST_CASE("vec3 unary negation", "[vec3]") {
    vec3 v(1, -2, 3);
    vec3 r = -v;
    REQUIRE(r.x == -1.0f);
    REQUIRE(r.y == 2.0f);
    REQUIRE(r.z == -3.0f);
}

TEST_CASE("vec3 compound assignment", "[vec3]") {
    vec3 v(1, 2, 3);
    v += vec3(4, 5, 6);
    REQUIRE(v.x == 5.0f);
    v -= vec3(1, 1, 1);
    REQUIRE(v.y == 6.0f);
    v *= 2.0f;
    REQUIRE(v.z == 12.0f);
    v /= 2.0f;
    REQUIRE(v.x == 5.0f);
}

TEST_CASE("vec3 length and normalize", "[vec3]") {
    vec3 v(3, 4, 0);
    REQUIRE(length(v) == 5.0f);
    vec3 n = normalize(v);
    REQUIRE(length(n) == Approx(1.0f));
    REQUIRE(n.x == Approx(0.6f));
    REQUIRE(n.y == Approx(0.8f));
}

TEST_CASE("vec3 dot product", "[vec3]") {
    vec3 a(1, 2, 3);
    vec3 b(4, -5, 6);
    REQUIRE(dot(a, b) == 12.0f);
}

TEST_CASE("vec3 cross product", "[vec3]") {
    vec3 a(1, 0, 0);
    vec3 b(0, 1, 0);
    vec3 c = cross(a, b);
    REQUIRE(c.x == 0.0f);
    REQUIRE(c.y == 0.0f);
    REQUIRE(c.z == 1.0f);
}

TEST_CASE("vec3 equality", "[vec3]") {
    REQUIRE(vec3(1, 2, 3) == vec3(1, 2, 3));
    REQUIRE_FALSE(vec3(1, 2, 3) == vec3(1, 2, 4));
}
