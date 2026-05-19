#pragma once
#include "math/vec3.h"

struct SimulationParams {
    vec3 gravity = vec3(0.0f, -9.81f, 0.0f);
    float linear_damping = 0.01f;
    float angular_damping = 0.01f;
    float physics_dt = 1.0f / 240.0f;
    int max_substeps = 8;
    int solver_iterations = 15;
    float baumgarte = 0.15f;
    float penetration_slop = 0.005f;
    float sleep_linear_threshold = 0.01f;
    float sleep_angular_threshold = 0.01f;
    float sleep_timeout = 1.0f;
    float contact_break_distance = 0.02f;
};
