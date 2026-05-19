#pragma once
#include "shapes/shape.h"
#include "math/math_utils.h"

struct BoxShape : public CollisionShape {
    vec3 center;
    vec3 axes[3];
    vec3 half_extents;

    BoxShape(const vec3& c, const vec3& hx, const vec3& hy, const vec3& hz,
             float hw, float hh, float hd)
        : CollisionShape(ShapeType::Box), center(c), half_extents(hw, hh, hd) {
        axes[0] = hx; axes[1] = hy; axes[2] = hz;
    }

    static BoxShape aabb(const vec3& c, float hw, float hh, float hd) {
        return BoxShape(c, vec3(1,0,0), vec3(0,1,0), vec3(0,0,1), hw, hh, hd);
    }

    vec3 support(const vec3& direction) const override {
        vec3 result = center;
        for (int i = 0; i < 3; i++) {
            float d = dot(direction, axes[i]);
            if (d > 0) result += axes[i] * half_extents[i];
            else       result -= axes[i] * half_extents[i];
        }
        return result;
    }

    void compute_aabb(vec3& out_min, vec3& out_max) const override {
        vec3 corners[8];
        int idx = 0;
        for (int i = 0; i < 8; i++) {
            corners[idx] = center;
            corners[idx] += ((i & 1) ? axes[0] : -axes[0]) * half_extents[0];
            corners[idx] += ((i & 2) ? axes[1] : -axes[1]) * half_extents[1];
            corners[idx] += ((i & 4) ? axes[2] : -axes[2]) * half_extents[2];
            idx++;
        }
        out_min = corners[0];
        out_max = corners[0];
        for (int i = 1; i < 8; i++) {
            out_min.x = minf(out_min.x, corners[i].x);
            out_min.y = minf(out_min.y, corners[i].y);
            out_min.z = minf(out_min.z, corners[i].z);
            out_max.x = maxf(out_max.x, corners[i].x);
            out_max.y = maxf(out_max.y, corners[i].y);
            out_max.z = maxf(out_max.z, corners[i].z);
        }
    }
};
