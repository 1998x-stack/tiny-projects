#include "raylib.h"
#include "core/world.h"
#include "shapes/sphere.h"
#include "shapes/box.h"
#include "shapes/plane.h"
#include "render/debug_draw.h"
#include "math/math_utils.h"

int main() {
    const int screen_width = 1280;
    const int screen_height = 720;

    InitWindow(screen_width, screen_height, "3D Physics Engine");
    SetTargetFPS(60);

    Camera3D camera;
    DebugDraw::setup_camera(camera);

    World world;
    world.params().solver_iterations = 15;
    world.params().baumgarte = 0.15f;
    world.params().sleep_timeout = 0.5f;

    {
        auto ground = std::make_unique<RigidBody>();
        ground->set_mass(0.0f);
        ground->friction = 0.8f;
        ground->restitution = 0.3f;
        ground->shape = std::make_unique<PlaneShape>(vec3(0, 1, 0), 0.0f);
        world.add_body(std::move(ground));
    }

    for (int i = 0; i < 5; i++) {
        auto box = std::make_unique<RigidBody>();
        box->position = vec3(0, 1.0f + i * 1.2f, 0);
        box->set_mass(1.0f);
        box->friction = 0.5f;
        box->restitution = 0.1f;
        box->shape = std::make_unique<BoxShape>(
            box->position, vec3(1,0,0), vec3(0,1,0), vec3(0,0,1), 0.5f, 0.5f, 0.5f);
        box->inertia_tensor = inertia_tensor_box(0.5f, 0.5f, 0.5f, box->mass);
        box->inv_inertia_tensor = inverse(box->inertia_tensor);
        world.add_body(std::move(box));
    }

    for (int i = 0; i < 20; i++) {
        auto sphere = std::make_unique<RigidBody>();
        sphere->position = vec3((float)(i % 5) * 2.0f - 4.0f, 5.0f + (i / 5) * 2.0f, 0);
        sphere->set_mass(1.0f);
        sphere->friction = 0.4f;
        sphere->restitution = 0.5f;
        sphere->shape = std::make_unique<SphereShape>(sphere->position, 0.5f);
        sphere->inertia_tensor = inertia_tensor_sphere(0.5f, sphere->mass);
        sphere->inv_inertia_tensor = inverse(sphere->inertia_tensor);
        world.add_body(std::move(sphere));
    }

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        world.step(dt);

        UpdateCamera(&camera, CAMERA_ORBITAL);

        BeginDrawing();
        ClearBackground(BLACK);

        BeginMode3D(camera);
        DrawGrid(20, 1.0f);
        DebugDraw::draw_world(world);
        EndMode3D();

        DrawFPS(10, 10);
        DrawText("3D Physics Engine", 10, 30, 20, WHITE);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
