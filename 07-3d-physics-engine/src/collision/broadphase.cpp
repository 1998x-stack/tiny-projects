#include "collision/broadphase.h"
#include "shapes/shape.h"
#include "math/math_utils.h"
#include <algorithm>

void OctreeBroadphase::compute_world_bounds(const std::vector<std::unique_ptr<RigidBody>>& bodies,
                                              vec3& out_center, vec3& out_half_size) {
    if (bodies.empty()) {
        out_center = vec3(0, 0, 0);
        out_half_size = vec3(10, 10, 10);
        return;
    }
    vec3 world_min(1e10f, 1e10f, 1e10f);
    vec3 world_max(-1e10f, -1e10f, -1e10f);
    for (const auto& body : bodies) {
        if (!body->shape) continue;
        vec3 aabb_min, aabb_max;
        body->shape->compute_aabb(aabb_min, aabb_max);
        world_min.x = minf(world_min.x, aabb_min.x);
        world_min.y = minf(world_min.y, aabb_min.y);
        world_min.z = minf(world_min.z, aabb_min.z);
        world_max.x = maxf(world_max.x, aabb_max.x);
        world_max.y = maxf(world_max.y, aabb_max.y);
        world_max.z = maxf(world_max.z, aabb_max.z);
    }
    out_center = (world_min + world_max) * 0.5f;
    out_half_size = (world_max - world_min) * 0.5f + vec3(1, 1, 1);
}

void OctreeBroadphase::insert(OctreeNode* node, RigidBody* body, int depth) {
    if (!body->shape) return;
    vec3 aabb_min, aabb_max;
    body->shape->compute_aabb(aabb_min, aabb_max);
    vec3 body_center = (aabb_min + aabb_max) * 0.5f;

    vec3 delta = body_center - node->center;
    if (std::abs(delta.x) > node->half_size.x ||
        std::abs(delta.y) > node->half_size.y ||
        std::abs(delta.z) > node->half_size.z) return;

    if (node->is_leaf) {
        node->bodies.push_back(body);
        if ((int)node->bodies.size() > MAX_BODIES_PER_LEAF && depth < MAX_DEPTH) {
            node->is_leaf = false;
            vec3 child_hs = node->half_size * 0.5f;
            for (int i = 0; i < 8; i++) {
                float cx = node->center.x + ((i & 1) ? child_hs.x : -child_hs.x);
                float cy = node->center.y + ((i & 2) ? child_hs.y : -child_hs.y);
                float cz = node->center.z + ((i & 4) ? child_hs.z : -child_hs.z);
                node->children[i] = new OctreeNode(vec3(cx, cy, cz), child_hs);
            }
            auto bodies_copy = node->bodies;
            node->bodies.clear();
            for (auto* b : bodies_copy) {
                for (auto* child : node->children) {
                    insert(child, b, depth + 1);
                }
            }
        }
    } else {
        for (auto* child : node->children) {
            insert(child, body, depth + 1);
        }
    }
}

void OctreeBroadphase::collect_pairs(OctreeNode* node, std::vector<CollisionPair>& pairs) {
    if (node->is_leaf) {
        int n = (int)node->bodies.size();
        for (int i = 0; i < n; i++) {
            for (int j = i + 1; j < n; j++) {
                if (!node->bodies[i]->shape || !node->bodies[j]->shape) continue;
                vec3 min_a, max_a, min_b, max_b;
                node->bodies[i]->shape->compute_aabb(min_a, max_a);
                node->bodies[j]->shape->compute_aabb(min_b, max_b);
                if (max_a.x >= min_b.x && max_b.x >= min_a.x &&
                    max_a.y >= min_b.y && max_b.y >= min_a.y &&
                    max_a.z >= min_b.z && max_b.z >= min_a.z) {
                    pairs.push_back({node->bodies[i], node->bodies[j]});
                }
            }
        }
    } else {
        for (auto* child : node->children) {
            collect_pairs(child, pairs);
        }
    }
}

std::vector<CollisionPair> OctreeBroadphase::find_pairs(
    const std::vector<std::unique_ptr<RigidBody>>& bodies) {
    vec3 center, half_size;
    compute_world_bounds(bodies, center, half_size);
    OctreeNode root(center, half_size);
    for (auto& body : bodies) {
        insert(&root, body.get(), 0);
    }
    std::vector<CollisionPair> pairs;
    collect_pairs(&root, pairs);
    return pairs;
}
