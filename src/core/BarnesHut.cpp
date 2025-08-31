#include "BarnesHut.h"
#include <algorithm>
#include <cmath>

static AABB computeBounds(const std::vector<Particle>& particles) {
    if (particles.empty()) return {};
    glm::vec3 minp = particles[0].position;
    glm::vec3 maxp = particles[0].position;
    for (const auto& p : particles) {
        minp = glm::min(minp, p.position);
        maxp = glm::max(maxp, p.position);
    }
    AABB b;
    b.center = (minp + maxp) * 0.5f;
    b.halfSize = (maxp - b.center) + glm::vec3(1e-3f);
    return b;
}

void BarnesHut::build(const std::vector<Particle>& particles) {
    std::vector<int> idx(particles.size());
    for (int i = 0; i < (int)particles.size(); ++i) idx[i] = i;
    AABB bounds = computeBounds(particles);
    root = buildRecursive(particles, bounds, idx, 0);
    accumulateMass(root.get(), particles);
}

std::unique_ptr<OctreeNode> BarnesHut::buildRecursive(const std::vector<Particle>& particles, const AABB& bounds, const std::vector<int>& indices, int depth) {
    auto node = std::make_unique<OctreeNode>();
    node->box = bounds;

    if ((int)indices.size() <= params.maxLeafSize || depth > 32) {
        node->indices = indices;
        return node;
    }

    glm::vec3 c = bounds.center;
    glm::vec3 hs = bounds.halfSize * 0.5f;

    // Prepare child AABBs
    AABB childBoxes[8];
    for (int i = 0; i < 8; ++i) {
        glm::vec3 offset = glm::vec3((i & 1) ? hs.x : -hs.x,
                                     (i & 2) ? hs.y : -hs.y,
                                     (i & 4) ? hs.z : -hs.z) * 0.5f;
        childBoxes[i].center = c + offset * 2.0f;
        childBoxes[i].halfSize = hs;
    }

    std::vector<int> childIndices[8];
    for (int i = 0; i < 8; ++i) childIndices[i].reserve(indices.size() / 8 + 1);

    for (int idx : indices) {
        const glm::vec3& p = particles[idx].position;
        for (int i = 0; i < 8; ++i) {
            if (childBoxes[i].contains(p)) {
                childIndices[i].push_back(idx);
                break;
            }
        }
    }

    bool allEmpty = true;
    // Collect work items to enable parallel build of independent children
    struct Task { int idx; };
    std::vector<Task> tasks; tasks.reserve(8);
    for (int i = 0; i < 8; ++i) if (!childIndices[i].empty()) { tasks.push_back({i}); allEmpty = false; }

    #pragma omp parallel for schedule(static) if(tasks.size() > 2)
    for (int ti = 0; ti < (int)tasks.size(); ++ti) {
        int i = tasks[ti].idx;
        auto child = buildRecursive(particles, childBoxes[i], childIndices[i], depth + 1);
        // publish
        node->children[i] = std::move(child);
    }

    if (allEmpty) {
        node->indices = indices;
    }
    return node;
}

void BarnesHut::accumulateMass(OctreeNode* node, const std::vector<Particle>& particles) {
    if (!node) return;
    if (node->isLeaf()) {
        node->mass = 0.0f;
        node->com = glm::vec3(0.0f);
        for (int idx : node->indices) {
            node->mass += particles[idx].mass;
            node->com += particles[idx].mass * particles[idx].position;
        }
        if (node->mass > 0.0f) node->com /= node->mass;
        else node->com = node->box.center;
        return;
    }
    node->mass = 0.0f;
    node->com = glm::vec3(0.0f);
    for (const auto& c : node->children) {
        if (!c) continue;
        accumulateMass(c.get(), particles);
        node->mass += c->mass;
        node->com += c->mass * c->com;
    }
    if (node->mass > 0.0f) node->com /= node->mass;
    else node->com = node->box.center;
}

glm::vec3 BarnesHut::computeForce(int i, const std::vector<Particle>& particles) const {
    const Particle& pi = particles[i];
    glm::vec3 force(0.0f);

    std::vector<const OctreeNode*> stack;
    stack.reserve(64);
    stack.push_back(root.get());

    while (!stack.empty()) {
        const OctreeNode* node = stack.back();
        stack.pop_back();
        if (!node || node->mass <= 0.0f) continue;

        if (node->isLeaf()) {
            for (int idx : node->indices) {
                if (idx == i) continue;
                const Particle& pj = particles[idx];
                glm::vec3 r = pj.position - pi.position;
                float dist2 = glm::dot(r, r) + params.softening * params.softening;
                float invDist = 1.0f / sqrtf(dist2);
                float invDist3 = invDist * invDist * invDist;
                force += params.G * pj.mass * invDist3 * r;
            }
        } else {
            glm::vec3 r = node->com - pi.position;
            float dist = glm::length(r) + 1e-6f;
            float s = 2.0f * std::max(std::max(node->box.halfSize.x, node->box.halfSize.y), node->box.halfSize.z); // be conservative if box not cubic
            if ((s / dist) < params.theta) {
                float dist2 = dist * dist + params.softening * params.softening;
                float invDist = 1.0f / sqrtf(dist2);
                float invDist3 = invDist * invDist * invDist;
                force += params.G * node->mass * invDist3 * r;
            } else {
                for (const auto& c : node->children) if (c) stack.push_back(c.get());
            }
        }
    }

    return force;
}
