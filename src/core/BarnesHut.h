#pragma once
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "Particle.h"

// Axis-aligned bounding box
struct AABB {
    glm::vec3 center{0.0f};
    glm::vec3 halfSize{1.0f};
    bool contains(const glm::vec3& p) const {
        glm::vec3 d = glm::abs(p - center);
        return (d.x <= halfSize.x && d.y <= halfSize.y && d.z <= halfSize.z);
    }
};

class OctreeNode {
public:
    AABB box;
    glm::vec3 com{0.0f}; // center of mass
    float mass{0.0f};
    std::vector<int> indices; // particle indices for leaf
    std::unique_ptr<OctreeNode> children[8];

    bool isLeaf() const {
        for (const auto& c : children) if (c) return false; return true;
    }
};

struct BarnesHutParams {
    float theta = 0.7f; // opening angle
    float softening = 0.01f; // gravitational softening
    float G = 1.0f; // gravitational constant (scaled)
    int maxLeafSize = 8;
};

class BarnesHut {
public:
    BarnesHut(BarnesHutParams params = {}): params(params) {}
    void build(const std::vector<Particle>& particles);
    glm::vec3 computeForce(int i, const std::vector<Particle>& particles) const;

private:
    std::unique_ptr<OctreeNode> root;
    BarnesHutParams params;

    std::unique_ptr<OctreeNode> buildRecursive(const std::vector<Particle>& particles, const AABB& bounds, const std::vector<int>& indices, int depth);
    void accumulateMass(OctreeNode* node, const std::vector<Particle>& particles);
};
