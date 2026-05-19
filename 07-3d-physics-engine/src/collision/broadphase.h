#pragma once
#include <vector>
#include <array>
#include "core/rigid_body.h"
#include "math/vec3.h"

struct OctreeNode {
    vec3 center;
    vec3 half_size;
    std::vector<RigidBody*> bodies;
    std::array<OctreeNode*, 8> children{};
    bool is_leaf = true;

    OctreeNode(const vec3& c, const vec3& hs) : center(c), half_size(hs) {}
    ~OctreeNode() { for (auto* child : children) delete child; }
};

struct CollisionPair {
    RigidBody* body_a;
    RigidBody* body_b;
};

class OctreeBroadphase {
public:
    static constexpr int MAX_DEPTH = 5;
    static constexpr int MAX_BODIES_PER_LEAF = 4;

    static std::vector<CollisionPair> find_pairs(const std::vector<std::unique_ptr<RigidBody>>& bodies);

private:
    static void insert(OctreeNode* node, RigidBody* body, int depth);
    static void collect_pairs(OctreeNode* node, std::vector<CollisionPair>& pairs);
    static void compute_world_bounds(const std::vector<std::unique_ptr<RigidBody>>& bodies,
                                      vec3& out_center, vec3& out_half_size);
};
