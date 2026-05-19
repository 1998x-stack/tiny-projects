#pragma once
#include "shapes/shape.h"
#include "collision/gjk.h"
#include "math/math_utils.h"
#include <vector>
#include <array>

struct EPAFace {
    std::array<vec3, 3> vertices;
    vec3 normal;
    float distance;
};

struct EPAResult {
    vec3 normal;
    float depth;
    bool success;
};

inline EPAResult epa(const CollisionShape& a, const CollisionShape& b, const Simplex& simplex) {
    std::vector<vec3> polytope;
    for (int i = 0; i < simplex.size; i++) {
        polytope.push_back(simplex.points[i]);
    }

    std::vector<EPAFace> faces;
    int face_indices[4][3] = {{0,1,2}, {0,3,1}, {0,2,3}, {1,3,2}};
    for (int f = 0; f < 4 && simplex.size >= 4; f++) {
        EPAFace face;
        face.vertices[0] = polytope[face_indices[f][0]];
        face.vertices[1] = polytope[face_indices[f][1]];
        face.vertices[2] = polytope[face_indices[f][2]];
        vec3 ab = face.vertices[1] - face.vertices[0];
        vec3 ac = face.vertices[2] - face.vertices[0];
        face.normal = normalize(cross(ab, ac));
        face.distance = dot(face.normal, face.vertices[0]);
        if (face.distance < 0) {
            face.normal = -face.normal;
            face.distance = -face.distance;
        }
        faces.push_back(face);
    }

    EPAResult result;
    result.success = false;
    result.depth = 0.0f;
    result.normal = vec3(0, 1, 0);

    int max_iter = 32;
    float epsilon = 0.001f;

    for (int iter = 0; iter < max_iter; iter++) {
        if (faces.empty()) break;

        int closest_idx = 0;
        float min_dist = faces[0].distance;
        for (int i = 1; i < (int)faces.size(); i++) {
            if (faces[i].distance < min_dist) {
                min_dist = faces[i].distance;
                closest_idx = i;
            }
        }

        EPAFace closest_face = faces[closest_idx];
        vec3 search_dir = closest_face.normal;
        vec3 support_pt = support_minkowski(a, b, search_dir);
        float support_dist = dot(support_pt, search_dir);

        if (support_dist - closest_face.distance < epsilon) {
            result.normal = closest_face.normal;
            result.depth = closest_face.distance;
            result.success = true;
            return result;
        }

        polytope.push_back(support_pt);
        std::vector<EPAFace> new_faces;
        for (const auto& face : faces) {
            if (dot(face.vertices[0] - support_pt, face.normal) < 0) {
                new_faces.push_back(face);
            }
        }
        if (new_faces.size() == faces.size()) break;
        faces = new_faces;
    }

    if (!faces.empty()) {
        int best = 0;
        float best_dist = faces[0].distance;
        for (int i = 1; i < (int)faces.size(); i++) {
            if (faces[i].distance < best_dist) {
                best_dist = faces[i].distance;
                best = i;
            }
        }
        result.normal = faces[best].normal;
        result.depth = faces[best].distance;
        result.success = true;
    }
    return result;
}
