#include "raylib.h"
#include "core/world.h"
#include "shapes/sphere.h"
#include "shapes/box.h"
#include "shapes/plane.h"
#include "render/debug_draw.h"
#include "math/math_utils.h"

int main() {
    const int W = 1280, H = 720;
    InitWindow(W, H, "Example 3: Ramp Roll");
    SetTargetFPS(60);

    Camera3D camera;
    DebugDraw::setup_camera(camera);
    camera.target = {2.0f, 2.0f, -3.0f};

    World world;
    world.params().solver_iterations = 15;
    world.params().baumgarte = 0.15f;
    world.params().gravity = vec3(0, -9.81f, 0);

    auto ground = std::make_unique<RigidBody>();
    ground->set_mass(0.0f);
    ground->friction = 0.8f;
    ground->restitution = 0.1f;
    ground->shape = std::make_unique<PlaneShape>(vec3(0, 1, 0), 0.0f);
    world.add_body(std::move(ground));

    float angle_deg = 25.0f;
    float angle_rad = angle_deg * (3.14159265f / 180.0f);
    vec3 ramp_right = vec3(std::cos(angle_rad), std::sin(angle_rad), 0);
    vec3 ramp_up = vec3(-std::sin(angle_rad), std::cos(angle_rad), 0);
    vec3 ramp_fwd = vec3(0, 0, 1);
    vec3 ramp_center = vec3(2.0f, std::sin(angle_rad) * 2.0f, -3.0f);

    auto ramp = std::make_unique<RigidBody>();
    ramp->set_mass(0.0f);
    ramp->friction = 0.6f;
    ramp->restitution = 0.1f;
    ramp->shape = std::make_unique<BoxShape>(
        ramp_center, ramp_right, ramp_up, ramp_fwd, 4.0f, 0.15f, 2.0f);
    world.add_body(std::move(ramp));

    auto side_wall = std::make_unique<RigidBody>();
    side_wall->set_mass(0.0f);
    side_wall->friction = 0.2f;
    side_wall->shape = std::make_unique<BoxShape>(
        vec3(-2.0f, 1.5f, -3.0f), vec3(1,0,0), vec3(0,1,0), vec3(0,0,1),
        0.2f, 2.0f, 2.0f);
    world.add_body(std::move(side_wall));

    auto ball = std::make_unique<RigidBody>();
    ball->position = vec3(-1.0f, 4.0f, -3.0f);
    ball->set_mass(2.0f);
    ball->friction = 0.4f;
    ball->restitution = 0.3f;
    ball->shape = std::make_unique<SphereShape>(ball->position, 0.6f);
    ball->inertia_tensor = inertia_tensor_sphere(0.6f, ball->mass);
    ball->inv_inertia_tensor = inverse(ball->inertia_tensor);
    world.add_body(std::move(ball));

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        world.step(dt);
        UpdateCamera(&camera, CAMERA_ORBITAL);

        BeginDrawing();
        ClearBackground(BLACK);
        BeginMode3D(camera);
        DrawGrid(20, 1.0f);
        DebugDraw::draw_world(world);

        const auto& bodies = world.bodies();
        if (bodies.size() > 4) {
            const auto& sphere = bodies.back();
            DrawText(TextFormat("Angular vel: (%.2f, %.2f, %.2f)",
                                sphere->angular_velocity.x,
                                sphere->angular_velocity.y,
                                sphere->angular_velocity.z),
                     10, 10, 16, YELLOW);
        }
        EndMode3D();

        DrawFPS(10, 10);
        DrawText(TextFormat("Ramp angle: %.0f deg | Sphere rolling down incline",
                            angle_deg), 10, 30, 20, WHITE);
        EndDrawing();
    }
    CloseWindow();
    return 0;
}
