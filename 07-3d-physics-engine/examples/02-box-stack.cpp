#include "raylib.h"
#include "core/world.h"
#include "shapes/box.h"
#include "shapes/plane.h"
#include "render/debug_draw.h"
#include "math/math_utils.h"

int main() {
    const int W = 1280, H = 720;
    InitWindow(W, H, "Example 2: Box Stack");
    SetTargetFPS(60);

    Camera3D camera;
    DebugDraw::setup_camera(camera);

    World world;
    world.params().solver_iterations = 15;
    world.params().baumgarte = 0.15f;
    world.params().sleep_timeout = 0.5f;

    auto ground = std::make_unique<RigidBody>();
    ground->set_mass(0.0f);
    ground->friction = 0.8f;
    ground->restitution = 0.1f;
    ground->shape = std::make_unique<PlaneShape>(vec3(0, 1, 0), 0.0f);
    world.add_body(std::move(ground));

    for (int i = 0; i < 6; i++) {
        auto box = std::make_unique<RigidBody>();
        box->position = vec3(0, 0.6f + i * 1.1f, 0);
        box->set_mass(1.0f);
        box->friction = 0.6f;
        box->restitution = 0.05f;
        box->shape = std::make_unique<BoxShape>(
            box->position, vec3(1,0,0), vec3(0,1,0), vec3(0,0,1),
            0.5f, 0.5f, 0.5f);
        box->inertia_tensor = inertia_tensor_box(0.5f, 0.5f, 0.5f, box->mass);
        box->inv_inertia_tensor = inverse(box->inertia_tensor);
        world.add_body(std::move(box));
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

        int awake = 0, sleeping = 0;
        for (const auto& b : world.bodies()) {
            if (!b->is_dynamic()) continue;
            b->is_sleeping ? sleeping++ : awake++;
        }
        DrawFPS(10, 10);
        DrawText(TextFormat("6-box stack | Awake: %d | Sleeping: %d", awake, sleeping),
                 10, 30, 20, WHITE);
        DrawText("Watch boxes settle and go to sleep", 10, 50, 18, GRAY);
        EndDrawing();
    }
    CloseWindow();
    return 0;
}
