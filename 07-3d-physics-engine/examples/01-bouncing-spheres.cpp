#include "raylib.h"
#include "core/world.h"
#include "shapes/sphere.h"
#include "shapes/plane.h"
#include "render/debug_draw.h"
#include "math/math_utils.h"

int main() {
    const int W = 1280, H = 720;
    InitWindow(W, H, "Example 1: Bouncing Spheres");
    SetTargetFPS(60);

    Camera3D camera;
    DebugDraw::setup_camera(camera);

    World world;
    world.params().restitution = 0.7f;
    world.params().solver_iterations = 15;

    auto ground = std::make_unique<RigidBody>();
    ground->set_mass(0.0f);
    ground->friction = 0.5f;
    ground->restitution = 0.8f;
    ground->shape = std::make_unique<PlaneShape>(vec3(0, 1, 0), 0.0f);
    world.add_body(std::move(ground));

    int sphere_count = 0;
    const int MAX_SPHERES = 15;
    float spawn_timer = 0.0f;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        if (spawn_timer > 0.8f && sphere_count < MAX_SPHERES) {
            spawn_timer = 0.0f;
            auto s = std::make_unique<RigidBody>();
            s->position = vec3((sphere_count % 5) * 2.0f - 4.0f,
                               8.0f + (sphere_count / 5) * 3.0f, 0);
            s->set_mass(1.0f);
            s->restitution = 0.7f;
            s->friction = 0.3f;
            s->shape = std::make_unique<SphereShape>(s->position, 0.6f);
            s->inertia_tensor = inertia_tensor_sphere(0.6f, s->mass);
            s->inv_inertia_tensor = inverse(s->inertia_tensor);
            world.add_body(std::move(s));
            sphere_count++;
        }
        spawn_timer += dt;

        world.step(dt);
        UpdateCamera(&camera, CAMERA_ORBITAL);

        BeginDrawing();
        ClearBackground(BLACK);
        BeginMode3D(camera);
        DrawGrid(20, 1.0f);
        DebugDraw::draw_world(world);
        EndMode3D();
        DrawFPS(10, 10);
        DrawText("Spheres: bouncing with 0.7 restitution", 10, 30, 20, WHITE);
        EndDrawing();
    }
    CloseWindow();
    return 0;
}
