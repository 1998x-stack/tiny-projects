#pragma once
#include "raylib.h"
#include "core/world.h"
#include "shapes/sphere.h"
#include "shapes/box.h"
#include "shapes/plane.h"
#include "math/math_utils.h"

class DebugDraw {
public:
    static void draw_world(const World& world) {
        for (const auto& body : world.bodies()) {
            if (!body->shape) continue;

            switch (body->shape->type) {
                case ShapeType::Sphere: {
                    auto* sphere = static_cast<const SphereShape*>(body->shape.get());
                    DrawSphere(
                        {body->position.x, body->position.y, body->position.z},
                        sphere->radius,
                        body->is_sleeping ? DARKGRAY : RED);
                    break;
                }
                case ShapeType::Box: {
                    auto* box = static_cast<const BoxShape*>(body->shape.get());
                    draw_box_wireframe(box->center, box->axes, box->half_extents,
                                       body->is_sleeping ? DARKGRAY : BLUE);
                    break;
                }
                case ShapeType::Plane: {
                    auto* plane = static_cast<const PlaneShape*>(body->shape.get());
                    DrawPlane({plane->normal.x * plane->offset,
                                plane->normal.y * plane->offset,
                                plane->normal.z * plane->offset},
                              {20, 20}, GREEN);
                    break;
                }
            }

            if (!body->is_sleeping && body->is_dynamic()) {
                DrawLine3D(
                    {body->position.x, body->position.y, body->position.z},
                    {body->position.x + body->linear_velocity.x,
                     body->position.y + body->linear_velocity.y,
                     body->position.z + body->linear_velocity.z},
                    YELLOW);
            }
        }
    }

    static void setup_camera(Camera3D& camera) {
        camera.position = {10.0f, 10.0f, 10.0f};
        camera.target = {0.0f, 2.0f, 0.0f};
        camera.up = {0.0f, 1.0f, 0.0f};
        camera.fovy = 45.0f;
        camera.projection = CAMERA_PERSPECTIVE;
    }

private:
    static void draw_box_wireframe(const vec3& center, const vec3 axes[3],
                                    const vec3& half_extents, Color color) {
        vec3 corners[8];
        int idx = 0;
        for (int i = 0; i < 8; i++) {
            corners[idx] = center;
            corners[idx] += ((i & 1) ? axes[0] : -axes[0]) * half_extents.x;
            corners[idx] += ((i & 2) ? axes[1] : -axes[1]) * half_extents.y;
            corners[idx] += ((i & 4) ? axes[2] : -axes[2]) * half_extents.z;
            idx++;
        }

        int edges[12][2] = {
            {0,1},{0,2},{0,4},{1,3},{1,5},
            {2,3},{2,6},{3,7},{4,5},{4,6},
            {5,7},{6,7}
        };

        for (auto& edge : edges) {
            DrawLine3D(
                {corners[edge[0]].x, corners[edge[0]].y, corners[edge[0]].z},
                {corners[edge[1]].x, corners[edge[1]].y, corners[edge[1]].z},
                color);
        }
    }
};
