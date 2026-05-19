#pragma once
#include <cmath>
#include "vec3.h"

struct mat3 {
    float m[3][3];  // column-major: m[column][row]

    mat3() {
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++)
                m[i][j] = 0.0f;
    }

    static mat3 identity() {
        mat3 result;
        result.m[0][0] = 1.0f;
        result.m[1][1] = 1.0f;
        result.m[2][2] = 1.0f;
        return result;
    }

    static mat3 diagonal(float a, float b, float c) {
        mat3 result;
        result.m[0][0] = a;
        result.m[1][1] = b;
        result.m[2][2] = c;
        return result;
    }

    vec3 operator*(const vec3& v) const {
        return vec3(
            m[0][0] * v.x + m[1][0] * v.y + m[2][0] * v.z,
            m[0][1] * v.x + m[1][1] * v.y + m[2][1] * v.z,
            m[0][2] * v.x + m[1][2] * v.y + m[2][2] * v.z
        );
    }

    mat3 operator*(const mat3& other) const {
        mat3 result;
        for (int col = 0; col < 3; col++) {
            for (int row = 0; row < 3; row++) {
                result.m[col][row] = 0.0f;
                for (int k = 0; k < 3; k++) {
                    result.m[col][row] += m[k][row] * other.m[col][k];
                }
            }
        }
        return result;
    }

    mat3 operator*(float s) const {
        mat3 result;
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++)
                result.m[i][j] = m[i][j] * s;
        return result;
    }

    mat3 operator+(const mat3& other) const {
        mat3 result;
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++)
                result.m[i][j] = m[i][j] + other.m[i][j];
        return result;
    }
};

inline mat3 transpose(const mat3& a) {
    mat3 result;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            result.m[i][j] = a.m[j][i];
    return result;
}

inline float determinant(const mat3& a) {
    return a.m[0][0] * (a.m[1][1] * a.m[2][2] - a.m[2][1] * a.m[1][2])
         - a.m[1][0] * (a.m[0][1] * a.m[2][2] - a.m[2][1] * a.m[0][2])
         + a.m[2][0] * (a.m[0][1] * a.m[1][2] - a.m[1][1] * a.m[0][2]);
}

inline mat3 inverse(const mat3& a) {
    float det = determinant(a);
    if (std::abs(det) < 1e-10f) return mat3::identity();

    float inv_det = 1.0f / det;
    mat3 result;
    result.m[0][0] = (a.m[1][1] * a.m[2][2] - a.m[2][1] * a.m[1][2]) * inv_det;
    result.m[0][1] = (a.m[2][1] * a.m[0][2] - a.m[0][1] * a.m[2][2]) * inv_det;
    result.m[0][2] = (a.m[0][1] * a.m[1][2] - a.m[1][1] * a.m[0][2]) * inv_det;
    result.m[1][0] = (a.m[2][0] * a.m[1][2] - a.m[1][0] * a.m[2][2]) * inv_det;
    result.m[1][1] = (a.m[0][0] * a.m[2][2] - a.m[2][0] * a.m[0][2]) * inv_det;
    result.m[1][2] = (a.m[1][0] * a.m[0][2] - a.m[0][0] * a.m[1][2]) * inv_det;
    result.m[2][0] = (a.m[1][0] * a.m[2][1] - a.m[2][0] * a.m[1][1]) * inv_det;
    result.m[2][1] = (a.m[2][0] * a.m[0][1] - a.m[0][0] * a.m[2][1]) * inv_det;
    result.m[2][2] = (a.m[0][0] * a.m[1][1] - a.m[1][0] * a.m[0][1]) * inv_det;
    return result;
}
