#pragma once
#include <cmath>
#include "vec3.h"

struct quat {
    float w, x, y, z;

    quat() : w(1), x(0), y(0), z(0) {}
    quat(float w, float x, float y, float z) : w(w), x(x), y(y), z(z) {}

    quat operator*(const quat& other) const {
        return quat(
            w * other.w - x * other.x - y * other.y - z * other.z,
            w * other.x + x * other.w + y * other.z - z * other.y,
            w * other.y - x * other.z + y * other.w + z * other.x,
            w * other.z + x * other.y - y * other.x + z * other.w
        );
    }

    quat& operator*=(const quat& other) {
        *this = *this * other;
        return *this;
    }

    bool operator==(const quat& other) const {
        return w == other.w && x == other.x && y == other.y && z == other.z;
    }
};

inline float dot(const quat& a, const quat& b) {
    return a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;
}

inline float length_sq(const quat& q) {
    return dot(q, q);
}

inline float length(const quat& q) {
    return std::sqrt(length_sq(q));
}

inline quat normalize(const quat& q) {
    float len = length(q);
    if (len > 0.0f) return quat(q.w / len, q.x / len, q.y / len, q.z / len);
    return quat();
}

inline quat conjugate(const quat& q) {
    return quat(q.w, -q.x, -q.y, -q.z);
}

inline vec3 rotate(const quat& q, const vec3& v) {
    // v' = q * v_pure * q_conjugate
    quat qv(0, v.x, v.y, v.z);
    quat q_conj = conjugate(q);
    quat result = q * qv * q_conj;
    return vec3(result.x, result.y, result.z);
}

inline quat quat_from_axis_angle(const vec3& axis, float angle) {
    float half_angle = angle * 0.5f;
    float s = std::sin(half_angle);
    return quat(std::cos(half_angle), axis.x * s, axis.y * s, axis.z * s);
}

inline quat quat_identity() {
    return quat();
}
