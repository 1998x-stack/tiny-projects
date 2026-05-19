#pragma once
#include "core/rigid_body.h"
#include "core/simulation.h"
#include "math/math_utils.h"

inline void update_sleep_body(RigidBody& body, float dt, const SimulationParams& params) {
    if (!body.is_dynamic()) return;
    if (length(body.linear_velocity) < params.sleep_linear_threshold &&
        length(body.angular_velocity) < params.sleep_angular_threshold) {
        body.sleep_timer += dt;
        if (body.sleep_timer > params.sleep_timeout && !body.is_sleeping) {
            body.is_sleeping = true;
            body.linear_velocity = vec3(0);
            body.angular_velocity = vec3(0);
        }
    } else {
        body.sleep_timer = 0.0f;
        body.is_sleeping = false;
    }
}

inline void wake_body(RigidBody& body) {
    if (body.is_sleeping) {
        body.is_sleeping = false;
        body.sleep_timer = 0.0f;
    }
}
