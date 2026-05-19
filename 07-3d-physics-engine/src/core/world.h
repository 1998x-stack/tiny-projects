#pragma once
#include <vector>
#include <memory>
#include "core/simulation.h"
#include "core/rigid_body.h"
#include "collision/contact.h"

class World {
public:
    World();

    SimulationParams& params() { return m_params; }
    const SimulationParams& params() const { return m_params; }

    void add_body(std::unique_ptr<RigidBody> body);
    const std::vector<std::unique_ptr<RigidBody>>& bodies() const { return m_bodies; }

    void step(float frame_time);

private:
    void apply_forces();
    void integrate(float dt);
    void detect_collisions(float dt);
    void solve_constraints(float dt);
    void update_sleep(float dt);

    SimulationParams m_params;
    std::vector<std::unique_ptr<RigidBody>> m_bodies;
    std::vector<ContactManifold> m_contacts;
    float m_accumulator = 0.0f;
};
