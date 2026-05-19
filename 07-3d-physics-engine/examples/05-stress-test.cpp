#include "raylib.h"
#include "core/world.h"
#include "shapes/sphere.h"
#include "shapes/box.h"
#include "shapes/plane.h"
#include "render/debug_draw.h"
#include "math/math_utils.h"
#include <cstdlib>

int main() {
    const int W = 1280, H = 720;
    InitWindow(W, H, "Example 5: Stress Test (200 Bodies)");
    SetTargetFPS(60);

    Camera3D camera;
    DebugDraw::setup_camera(camera);
    camera.position = {15.0f, 15.0f, 15.0f};

    World world;
    world.params().solver_iterations = 15;
    world.params().baumgarte = 0.15f;
    world.params().sleep_timeout = 0.3f;
    world.params().physics_dt = 1.0f / 120.0f;
    world.params().max_substeps = 4;

    auto ground = std::make_unique<RigidBody>();
    ground->set_mass(0.0f);
    ground->friction = 0.8f;
    ground->restitution = 0.2f;
    ground->shape = std::make_unique<PlaneShape>(vec3(0, 1, 0), 0.0f);
    world.add_body(std::move(ground));

    for (int x = 0; x < 6; x++) {
        auto wall = std::make_unique<RigidBody>();
        wall->set_mass(0.0f);
        wall->friction = 0.3f;
        wall->restitution = 0.1f;
        float wx = (x < 3) ? -8.0f : 8.0f;
        float wz = (x % 3) * 5.0f - 5.0f;
        wall->position = vec3(wx, 1.0f, wz);
        wall->shape = std::make_unique<BoxShape>(
            wall->position, vec3(1,0,0), vec3(0,1,0), vec3(0,0,1),
            0.2f, 2.0f, 2.5f);
        world.add_body(std::move(wall));
    }

    for (int i = 0; i < 200; i++) {
        float x = (i % 20) * 0.7f - 6.5f;
        float y = 2.0f + (i / 20) * 0.5f;
        float z = (i % 10) * 0.8f - 3.5f;

        if (i % 3 == 0) {
            auto box = std::make_unique<RigidBody>();
            box->position = vec3(x, y, z);
            box->set_mass(1.0f);
            box->friction = 0.5f;
            box->restitution = 0.1f;
            box->shape = std::make_unique<BoxShape>(
                box->position, vec3(1,0,0), vec3(0,1,0), vec3(0,0,1),
                0.25f, 0.25f, 0.25f);
            box->inertia_tensor = inertia_tensor_box(0.25f, 0.25f, 0.25f, box->mass);
            box->inv_inertia_tensor = inverse(box->inertia_tensor);
            world.add_body(std::move(box));
        } else {
            auto sphere = std::make_unique<RigidBody>();
            sphere->position = vec3(x, y + 1.0f, z);
            sphere->set_mass(0.5f + (i % 3) * 0.5f);
            sphere->friction = 0.3f;
            sphere->restitution = 0.4f;
            sphere->shape = std::make_unique<SphereShape>(sphere->position, 0.25f);
            sphere->inertia_tensor = inertia_tensor_sphere(0.25f, sphere->mass);
            sphere->inv_inertia_tensor = inverse(sphere->inertia_tensor);
            world.add_body(std::move(sphere));
        }
    }

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        world.step(dt);

        BeginDrawing();
        ClearBackground(BLACK);
        BeginMode3D(camera);
        DrawGrid(20, 1.0f);
        DebugDraw::draw_world(world);
        EndMode3D();

        int awake = 0, sleeping = 0, total = 0;
        for (const auto& b : world.bodies()) {
            if (!b->is_dynamic()) continue;
            total++;
            b->is_sleeping ? sleeping++ : awake++;
        }

        DrawFPS(10, 10);
        DrawText(TextFormat("%d dynamic bodies | Awake: %d | Sleeping: %d",
                            total, awake, sleeping), 10, 30, 20, WHITE);
        DrawText("Watch the broadphase + sleep reduce simulation cost", 10, 50, 18, GRAY);
        EndDrawing();
    }
    CloseWindow();
    return 0;
}
