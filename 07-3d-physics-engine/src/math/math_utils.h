#pragma once
#include "vec3.h"
#include "quat.h"
#include "mat3.h"

inline mat3 inertia_tensor_sphere(float radius, float mass) {
    float I = (2.0f / 5.0f) * mass * radius * radius;
    return mat3::diagonal(I, I, I);
}

inline mat3 inertia_tensor_box(float hx, float hy, float hz, float mass) {
    float Ixx = (1.0f / 12.0f) * mass * (hy * hy + hz * hz);
    float Iyy = (1.0f / 12.0f) * mass * (hx * hx + hz * hz);
    float Izz = (1.0f / 12.0f) * mass * (hx * hx + hy * hy);
    return mat3::diagonal(Ixx, Iyy, Izz);
}

inline mat3 quat_to_mat3(const quat& q) {
    float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
    float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
    float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;

    mat3 R;
    R.m[0][0] = 1.0f - 2.0f * (yy + zz);
    R.m[0][1] = 2.0f * (xy + wz);
    R.m[0][2] = 2.0f * (xz - wy);

    R.m[1][0] = 2.0f * (xy - wz);
    R.m[1][1] = 1.0f - 2.0f * (xx + zz);
    R.m[1][2] = 2.0f * (yz + wx);

    R.m[2][0] = 2.0f * (xz + wy);
    R.m[2][1] = 2.0f * (yz - wx);
    R.m[2][2] = 1.0f - 2.0f * (xx + yy);

    return R;
}

inline vec3 local_to_world(const vec3& local_pt, const vec3& position, const quat& orientation) {
    return position + rotate(orientation, local_pt);
}

inline vec3 world_to_local(const vec3& world_pt, const vec3& position, const quat& orientation) {
    return rotate(conjugate(orientation), world_pt - position);
}

inline vec3 local_to_world_direction(const vec3& local_dir, const quat& orientation) {
    return rotate(orientation, local_dir);
}

inline float clampf(float value, float min_val, float max_val) {
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
}

inline float maxf(float a, float b) { return a > b ? a : b; }
inline float minf(float a, float b) { return a < b ? a : b; }
