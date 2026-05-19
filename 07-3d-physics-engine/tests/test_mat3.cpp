#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include "math/mat3.h"
#include "math/vec3.h"
#include "math/math_utils.h"

TEST_CASE("mat3 identity", "[mat3]") {
    mat3 m = mat3::identity();
    vec3 v(1, 2, 3);
    vec3 r = m * v;
    REQUIRE(r == v);
}

TEST_CASE("mat3 from diagonal", "[mat3]") {
    mat3 m = mat3::diagonal(2.0f, 3.0f, 5.0f);
    vec3 v(1, 1, 1);
    vec3 r = m * v;
    REQUIRE(r.x == 2.0f);
    REQUIRE(r.y == 3.0f);
    REQUIRE(r.z == 5.0f);
}

TEST_CASE("mat3 transpose", "[mat3]") {
    mat3 m;
    m.m[0][0] = 1; m.m[0][1] = 2; m.m[0][2] = 3;
    m.m[1][0] = 4; m.m[1][1] = 5; m.m[1][2] = 6;
    m.m[2][0] = 7; m.m[2][1] = 8; m.m[2][2] = 9;
    mat3 t = transpose(m);
    REQUIRE(t.m[0][1] == 4.0f);
    REQUIRE(t.m[1][0] == 2.0f);
}

TEST_CASE("mat3 multiply vector", "[mat3]") {
    mat3 m = mat3::diagonal(2, 1, 1);
    vec3 v(3, 4, 5);
    vec3 r = m * v;
    REQUIRE(r.x == 6.0f);
    REQUIRE(r.y == 4.0f);
    REQUIRE(r.z == 5.0f);
}

TEST_CASE("mat3 scalar multiply", "[mat3]") {
    mat3 m = mat3::identity();
    mat3 r = m * 3.0f;
    REQUIRE(r.m[0][0] == 3.0f);
    REQUIRE(r.m[2][2] == 3.0f);
}

TEST_CASE("mat3 inverse of diagonal", "[mat3]") {
    mat3 m = mat3::diagonal(2, 4, 8);
    mat3 inv = inverse(m);
    mat3 result = m * inv;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            if (i == j)
                REQUIRE(result.m[i][j] == Approx(1.0f).margin(0.001f));
            else
                REQUIRE(std::abs(result.m[i][j]) < 0.001f);
        }
    }
}

TEST_CASE("inertia_tensor_sphere", "[math_utils]") {
    mat3 I = inertia_tensor_sphere(1.0f, 5.0f);
    float expected = (2.0f / 5.0f) * 5.0f * 1.0f * 1.0f;
    REQUIRE(I.m[0][0] == Approx(expected));
    REQUIRE(I.m[1][1] == Approx(expected));
    REQUIRE(I.m[2][2] == Approx(expected));
}

TEST_CASE("inertia_tensor_box", "[math_utils]") {
    mat3 I = inertia_tensor_box(2.0f, 3.0f, 4.0f, 6.0f);
    float Ixx = (1.0f / 12.0f) * 6.0f * (1.5f * 1.5f + 2.0f * 2.0f);
    REQUIRE(I.m[0][0] == Approx(Ixx));
}

TEST_CASE("quat_to_mat3 rotation", "[math_utils]") {
    quat q = quat_from_axis_angle(vec3(0, 0, 1), 1.57079633f);
    mat3 R = quat_to_mat3(q);
    vec3 v(1, 0, 0);
    vec3 r = R * v;
    REQUIRE(r.x == Approx(0.0f).margin(0.001f));
    REQUIRE(r.y == Approx(1.0f).margin(0.001f));
}

TEST_CASE("world_to_local transforms", "[math_utils]") {
    quat q(1, 0, 0, 0);
    vec3 world_pt(5, 0, 0);
    vec3 com(0, 0, 0);
    vec3 body_pt = world_to_local(world_pt, com, q);
    REQUIRE(body_pt == world_pt);
}
