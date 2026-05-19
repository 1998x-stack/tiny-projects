#pragma once
#include "shapes/shape.h"
#include "math/math_utils.h"
#include <array>

struct Simplex {
    std::array<vec3, 4> points;
    int size = 0;
};

inline vec3 support_minkowski(const CollisionShape& a, const CollisionShape& b, const vec3& d) {
    return a.support(d) - b.support(-d);
}

inline bool simplex_line(Simplex& s, vec3& direction) {
    vec3 a = s.points[1];
    vec3 b = s.points[0];
    vec3 ab = b - a;
    vec3 ao = -a;
    if (dot(ab, ao) > 0) {
        direction = cross(cross(ab, ao), ab);
    } else {
        s.points[0] = a;
        s.size = 1;
        direction = ao;
    }
    return false;
}

inline bool simplex_triangle(Simplex& s, vec3& direction) {
    vec3 a = s.points[2];
    vec3 b = s.points[1];
    vec3 c = s.points[0];
    vec3 ab = b - a;
    vec3 ac = c - a;
    vec3 ao = -a;
    vec3 abc = cross(ab, ac);

    if (dot(cross(abc, ac), ao) > 0) {
        if (dot(ac, ao) > 0) {
            s.points[0] = c; s.points[1] = a; s.size = 2;
            direction = cross(cross(ac, ao), ac);
        } else {
            return simplex_line(s, direction);
        }
        return false;
    }
    if (dot(cross(ab, abc), ao) > 0) {
        return simplex_line(s, direction);
    }
    if (dot(abc, ao) > 0) {
        s.points[0] = b; s.points[1] = c; s.points[2] = a; s.size = 3;
        direction = abc;
    } else {
        s.points[0] = c; s.points[1] = b; s.points[2] = a; s.size = 3;
        direction = -abc;
    }
    return false;
}

inline bool simplex_tetrahedron(Simplex& s, vec3& direction) {
    vec3 a = s.points[3];
    vec3 b = s.points[2];
    vec3 c = s.points[1];
    vec3 d = s.points[0];
    vec3 ao = -a;
    vec3 ab = b - a;
    vec3 ac = c - a;
    vec3 ad = d - a;

    vec3 abc_n = cross(ab, ac);
    if (dot(abc_n, ao) > 0) {
        s.points[0] = c; s.points[1] = b; s.points[2] = a; s.size = 3;
        direction = abc_n;
        return false;
    }
    vec3 acd_n = cross(ac, ad);
    if (dot(acd_n, ao) > 0) {
        s.points[0] = d; s.points[1] = c; s.points[2] = a; s.size = 3;
        direction = acd_n;
        return false;
    }
    vec3 adb_n = cross(ad, ab);
    if (dot(adb_n, ao) > 0) {
        s.points[0] = b; s.points[1] = d; s.points[2] = a; s.size = 3;
        direction = adb_n;
        return false;
    }
    return true;
}

inline bool update_simplex(Simplex& s, vec3& direction) {
    switch (s.size) {
        case 2: return simplex_line(s, direction);
        case 3: return simplex_triangle(s, direction);
        case 4: return simplex_tetrahedron(s, direction);
        default: return false;
    }
}

struct GJKResult {
    bool overlap;
    Simplex simplex;
};

inline GJKResult gjk(const CollisionShape& a, const CollisionShape& b) {
    Simplex simplex;
    simplex.size = 0;

    vec3 a_min, a_max, b_min, b_max;
    a.compute_aabb(a_min, a_max);
    b.compute_aabb(b_min, b_max);
    vec3 dir = (a_min + a_max) * 0.5f - (b_min + b_max) * 0.5f;
    if (length_sq(dir) < 1e-6f) dir = vec3(1, 0, 0);

    vec3 support_pt = support_minkowski(a, b, dir);
    simplex.points[0] = support_pt;
    simplex.size = 1;
    dir = -support_pt;

    for (int iter = 0; iter < 32; iter++) {
        support_pt = support_minkowski(a, b, dir);
        if (dot(support_pt, dir) < 0) {
            return {false, simplex};
        }
        simplex.points[simplex.size] = support_pt;
        simplex.size++;
        if (update_simplex(simplex, dir)) {
            return {true, simplex};
        }
    }
    return {false, simplex};
}
