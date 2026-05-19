#pragma once
#include "math/vec3.h"

enum class ShapeType { Sphere, Box, Plane };

struct CollisionShape {
    ShapeType type;
    CollisionShape(ShapeType t) : type(t) {}
    virtual ~CollisionShape() = default;
    virtual vec3 support(const vec3& direction) const = 0;
    virtual void compute_aabb(vec3& out_min, vec3& out_max) const = 0;
};
