#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include "collision/epa.h"
#include "collision/gjk.h"
#include "shapes/sphere.h"
#include "shapes/box.h"

TEST_CASE("EPA sphere-sphere penetration detail", "[epa]") {
    SphereShape a(vec3(0, 0, 0), 1.0f);
    SphereShape b(vec3(1.2f, 0, 0), 1.0f);
    GJKResult gjk_result = gjk(a, b);
    REQUIRE(gjk_result.overlap);
    EPAResult epa_result = epa(a, b, gjk_result.simplex);
    REQUIRE(epa_result.success);
    REQUIRE(epa_result.depth > 0.3f);
}
