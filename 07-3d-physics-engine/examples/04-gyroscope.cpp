#include "raylib.h"
#include "core/world.h"
#include "shapes/box.h"
#include "render/debug_draw.h"
#include "math/math_utils.h"

int main() {
    const int W = 1280, H = 720;
    InitWindow(W, H, "Example 4: Gyroscope (Tennis Racket Theorem)");
    SetTargetFPS(60);

    Camera3D camera;
    DebugDraw::setup_camera(camera);
    camera.target = {0.0f, 0.0f, 0.0f};

    World world;
    world.params().gravity = vec3(0, 0, 0);
    world.params().linear_damping = 0.0f;
    world.params().angular_damping = 0.0001f;
    world.params().solver_iterations = 15;

    auto body = std::make_unique<RigidBody>();
    body->position = vec3(0, 0, 0);
    body->set_mass(1.0f);
    body->restitution = 0.0f;
    body->friction = 0.0f;

    body->shape = std::make_unique<BoxShape>(
        vec3(0, 0, 0), vec3(1,0,0), vec3(0,1,0), vec3(0,0,1),
        2.0f, 0.2f, 0.2f);
    body->inertia_tensor = inertia_tensor_box(2.0f, 0.2f, 0.2f, body->mass);
    body->inv_inertia_tensor = inverse(body->inertia_tensor);

    body->angular_velocity = vec3(0.01f, 5.0f, 0.01f);
    world.add_body(std::move(body));

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        world.step(dt);
        UpdateCamera(&camera, CAMERA_ORBITAL);

        BeginDrawing();
        ClearBackground(BLACK);
        BeginMode3D(camera);
        DebugDraw::draw_world(world);
        EndMode3D();

        DrawFPS(10, 10);
        DrawText("Tennis Racket Theorem (Dzhanibekov effect)", 10, 30, 20, WHITE);
        DrawText("Spin around intermediate axis (Y) -> flips every ~2s", 10, 50, 18, GRAY);
        DrawText("R = rotation axis (stable), G = intermediate axis (unstable), B = third axis (stable)", 10, 70, 16, GRAY);

        const auto& b = world.bodies().back();
        DrawText(TextFormat("ω = (%.3f, %.3f, %.3f) | |ω| = %.3f",
                            b->angular_velocity.x, b->angular_velocity.y,
                            b->angular_velocity.z, length(b->angular_velocity)),
                 10, 90, 16, YELLOW);
        EndDrawing();
    }
    CloseWindow();
    return 0;
}
