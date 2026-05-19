#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include "collision/gjk.h"
#include "collision/epa.h"
#include "shapes/sphere.h"
#include "shapes/box.h"

TEST_CASE("sphere support function", "[shapes]") {
    SphereShape s(vec3(0, 0, 0), 1.0f);
    vec3 pt = s.support(vec3(1, 0, 0));
    REQUIRE(pt.x == Approx(1.0f));
    pt = s.support(vec3(0, 1, 0));
    REQUIRE(pt.y == Approx(1.0f));
}

TEST_CASE("sphere AABB", "[shapes]") {
    SphereShape s(vec3(2, 3, 4), 5.0f);
    vec3 min_pt, max_pt;
    s.compute_aabb(min_pt, max_pt);
    REQUIRE(min_pt.x == Approx(-3.0f));
    REQUIRE(max_pt.x == Approx(7.0f));
}

TEST_CASE("GJK sphere-sphere overlap", "[gjk]") {
    SphereShape a(vec3(0, 0, 0), 1.0f);
    SphereShape b(vec3(1.5f, 0, 0), 1.0f);
    REQUIRE(gjk(a, b).overlap);
}

TEST_CASE("GJK sphere-sphere no overlap", "[gjk]") {
    SphereShape a(vec3(0, 0, 0), 1.0f);
    SphereShape b(vec3(3.0f, 0, 0), 1.0f);
    REQUIRE_FALSE(gjk(a, b).overlap);
}

TEST_CASE("GJK box-box overlap", "[gjk]") {
    BoxShape a = BoxShape::aabb(vec3(0, 0, 0), 1, 1, 1);
    BoxShape b = BoxShape::aabb(vec3(0.5f, 0, 0), 1, 1, 1);
    REQUIRE(gjk(a, b).overlap);
}

TEST_CASE("GJK box-box no overlap", "[gjk]") {
    BoxShape a = BoxShape::aabb(vec3(0, 0, 0), 1, 1, 1);
    BoxShape b = BoxShape::aabb(vec3(2.0f, 0, 0), 1, 1, 1);
    REQUIRE_FALSE(gjk(a, b).overlap);
}

TEST_CASE("GJK sphere-box overlap", "[gjk]") {
    SphereShape a(vec3(0, 0, 0), 1.0f);
    BoxShape b = BoxShape::aabb(vec3(1.5f, 0, 0), 1, 1, 1);
    REQUIRE(gjk(a, b).overlap);
}

TEST_CASE("EPA sphere-sphere penetration", "[epa]") {
    SphereShape a(vec3(0, 0, 0), 1.0f);
    SphereShape b(vec3(1.5f, 0, 0), 1.0f);
    GJKResult gjk_result = gjk(a, b);
    REQUIRE(gjk_result.overlap);
    EPAResult epa_result = epa(a, b, gjk_result.simplex);
    REQUIRE(epa_result.success);
    REQUIRE(epa_result.depth == Approx(0.5f).margin(0.1f));
    REQUIRE(epa_result.normal.x > 0.0f);
}

TEST_CASE("EPA box-box penetration", "[epa]") {
    BoxShape a = BoxShape::aabb(vec3(0, 0, 0), 1, 1, 1);
    BoxShape b = BoxShape::aabb(vec3(0.3f, 0, 0), 1, 1, 1);
    GJKResult gjk_result = gjk(a, b);
    REQUIRE(gjk_result.overlap);
    EPAResult epa_result = epa(a, b, gjk_result.simplex);
    REQUIRE(epa_result.success);
    REQUIRE(epa_result.depth > 0.0f);
}
