#pragma once
#include "shapes/shape.h"
#include "math/math_utils.h"

struct PlaneShape : public CollisionShape {
    vec3 normal;
    float offset;

    PlaneShape(const vec3& n, float o)
        : CollisionShape(ShapeType::Plane), normal(normalize(n)), offset(o) {}

    vec3 support(const vec3& direction) const override {
        float d = dot(direction, normal);
        if (d > 0) {
            vec3 tangent = direction - normal * d;
            if (length_sq(tangent) < 1e-6f) {
                vec3 arbitrary;
                if (std::abs(normal.x) < 0.9f) arbitrary = vec3(1, 0, 0);
                else arbitrary = vec3(0, 1, 0);
                tangent = cross(normal, arbitrary);
            }
            return normal * offset + normalize(tangent) * 1000.0f;
        }
        return normal * offset;
    }

    void compute_aabb(vec3& out_min, vec3& out_max) const override {
        out_min = vec3(-500, -500, -500);
        out_max = vec3(500, 500, 500);
    }
};
