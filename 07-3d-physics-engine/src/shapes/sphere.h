#pragma once
#include "shapes/shape.h"

struct SphereShape : public CollisionShape {
    vec3 center;
    float radius;

    SphereShape(const vec3& c, float r)
        : CollisionShape(ShapeType::Sphere), center(c), radius(r) {}

    vec3 support(const vec3& direction) const override {
        float len = length(direction);
        if (len < 1e-6f) return center;
        return center + (direction / len) * radius;
    }

    void compute_aabb(vec3& out_min, vec3& out_max) const override {
        out_min = center - vec3(radius, radius, radius);
        out_max = center + vec3(radius, radius, radius);
    }
};
