#include "SimulationEngine.h"
#include <glm/gtc/constants.hpp>
#include <glm/gtx/norm.hpp>
#include <algorithm>
#include <unordered_map>

SimulationEngine::SimulationEngine() : rng(std::random_device{}()) {}

void SimulationEngine::reset(const SimulationSettings& s) {
    particles.clear();
    particles.shrink_to_fit();
    BarnesHutParams p; p.G = s.gravityG; p.softening = s.softening; p.theta = s.theta;
    bh = BarnesHut(p);
    lastBhParams = p; frameCounter = 0; lastParticleCount = 0;

    switch (s.module) {
        case SimulationModule::Galaxy: initGalaxy(s.particleCount); break;
        case SimulationModule::BlackHole: initBlackHole(s.particleCount); break;
        case SimulationModule::Supernova: initSupernova(s.particleCount); break;
        case SimulationModule::Interactions: initInteractions(s.particleCount); break;
    }
}

void SimulationEngine::update(const SimulationSettings& s) {
    BarnesHutParams p; p.G = s.gravityG; p.softening = s.softening; p.theta = s.theta;
    bool paramsChanged = (p.G != lastBhParams.G) || (p.softening != lastBhParams.softening) || (p.theta != lastBhParams.theta);
    bool countChanged = (particles.size() != lastParticleCount);
    if (paramsChanged) { bh = BarnesHut(p); lastBhParams = p; }
    if (paramsChanged || countChanged || (s.rebuildEveryN <= 1) || (frameCounter % s.rebuildEveryN == 0)) {
        bh.build(particles);
        lastParticleCount = particles.size();
    }

    // zero forces
    for (auto& pt : particles) pt.force = glm::vec3(0.0f);

    // compute forces (parallel)
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < (int)particles.size(); ++i) {
        particles[i].force = particles[i].mass * bh.computeForce(i, particles);
    }

    // interactive tool
    if (s.toolEngaged && s.tool != InteractionTool::None && s.toolRadius > 0.0f) {
        applyInteractiveTool(s);
    }

    integrate(s);
    if (s.collisions) handleCollisions(s.restitution);
    if (s.module == SimulationModule::BlackHole) applyBlackHoleEventHorizon();
    ++frameCounter;
}

void SimulationEngine::applyInteractiveTool(const SimulationSettings& s) {
    const glm::vec3 center = s.toolWorld;
    const float radius = s.toolRadius;
    const float r2 = radius * radius;
    const float k = s.toolStrength;

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < (int)particles.size(); ++i) {
        Particle& p = particles[i];
        glm::vec3 d = center - p.position;
        float dist2 = glm::dot(d, d) + 1e-6f;
        if (dist2 > r2) continue;
        float dist = sqrtf(dist2);
        glm::vec3 n = d / dist;
        glm::vec3 f(0.0f);
        switch (s.tool) {
            case InteractionTool::Attract:
                f = n * (k * p.mass / dist2);
                break;
            case InteractionTool::Repel:
                f = -n * (fabsf(k) * p.mass / dist2);
                break;
            case InteractionTool::Drag: {
                float springK = fabsf(k);
                glm::vec3 spring = springK * (center - p.position);
                glm::vec3 damping = -0.5f * springK * p.velocity;
                f = spring + damping;
            } break;
            default: break;
        }
        float t = 1.0f - (dist / radius);
        t = glm::clamp(t, 0.0f, 1.0f);
        f *= t * t;
        p.force += f;
    }
}

void SimulationEngine::integrate(const SimulationSettings& s) {
    float dt = s.timeStep;
    float damp = s.damping;
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < (int)particles.size(); ++i) {
        auto& pt = particles[i];
        glm::vec3 accel = (pt.mass > 0.0f) ? (pt.force / pt.mass) : glm::vec3(0.0f);
        pt.velocity += accel * dt;
        pt.velocity *= (1.0f - damp);
        pt.position += pt.velocity * dt;
    }
}

void SimulationEngine::handleCollisions(float restitution) {
    struct CellKey { int x,y,z; };
    struct KeyHash {
        size_t operator()(const CellKey& k) const noexcept {
            // mix 3 ints into a size_t (64-bit friendly hash)
            size_t h = (uint32_t)k.x * 73856093u ^ (uint32_t)k.y * 19349663u ^ (uint32_t)k.z * 83492791u;
            return h;
        }
    };
    struct KeyEq { bool operator()(const CellKey& a, const CellKey& b) const noexcept { return a.x==b.x && a.y==b.y && a.z==b.z; } };

    if (particles.empty()) return;
    // Choose cell size ~ 2x typical radius
    float avgR = 0.0f; int sampleN = (int)std::min<size_t>(particles.size(), 256);
    for (int i = 0; i < sampleN; ++i) avgR += particles[i].radius;
    avgR = (sampleN > 0) ? (avgR / sampleN) : 1.0f;
    const float cellSize = std::max(0.5f, avgR * 2.5f);
    const float invCell = 1.0f / cellSize;

    std::unordered_map<CellKey, std::vector<int>, KeyHash, KeyEq> grid;
    grid.reserve(particles.size()*2);

    auto cellOf = [&](const glm::vec3& p){
        return CellKey{ (int)floorf(p.x * invCell), (int)floorf(p.y * invCell), (int)floorf(p.z * invCell) };
    };

    // Build grid
    for (int i = 0; i < (int)particles.size(); ++i) {
        CellKey key = cellOf(particles[i].position);
        grid[key].push_back(i);
    }

    // Neighbor offsets (27 cells)
    int offs[27][3]; int t=0; for(int dz=-1; dz<=1; ++dz) for(int dy=-1; dy<=1; ++dy) for(int dx=-1; dx<=1; ++dx){ offs[t][0]=dx; offs[t][1]=dy; offs[t][2]=dz; ++t; }

    // Resolve collisions
    for (const auto& kv : grid) {
        const CellKey& c = kv.first;
        for (int nbi = 0; nbi < 27; ++nbi) {
            CellKey nb{ c.x + offs[nbi][0], c.y + offs[nbi][1], c.z + offs[nbi][2] };
            auto it = grid.find(nb);
            if (it == grid.end()) continue;
            const auto& a = kv.second;
            const auto& b = it->second;
            for (int ii = 0; ii < (int)a.size(); ++ii) {
                int i = a[ii];
                int jjStart = (&a == &b) ? (ii+1) : 0;
                for (int jj = jjStart; jj < (int)b.size(); ++jj) {
                    int j = b[jj];
                    glm::vec3 r = particles[j].position - particles[i].position;
                    float minDist = particles[i].radius + particles[j].radius;
                    float dist2 = glm::dot(r,r);
                    if (dist2 < minDist * minDist) {
                        float dist = sqrtf(std::max(dist2, 1e-12f));
                        glm::vec3 n = (dist > 0.0f) ? (r / dist) : glm::vec3(1,0,0);
                        float mi = particles[i].mass, mj = particles[j].mass;
                        glm::vec3 vi = particles[i].velocity;
                        glm::vec3 vj = particles[j].velocity;
                        float vi_n = glm::dot(vi, n);
                        float vj_n = glm::dot(vj, n);
                        float pi = (2.0f * (vi_n - vj_n)) / (mi + mj);
                        particles[i].velocity = vi - pi * mj * n * restitution;
                        particles[j].velocity = vj + pi * mi * n * restitution;
                        float overlap = minDist - dist;
                        particles[i].position -= n * (overlap * (mj / (mi + mj)));
                        particles[j].position += n * (overlap * (mi / (mi + mj)));
                    }
                }
            }
        }
    }
}

void SimulationEngine::initGalaxy(int n) {
    particles.resize(n);
    std::uniform_real_distribution<float> uni(0.0f, 1.0f);
    float R = 500.0f;
    for (int i = 0; i < n; ++i) {
        float r = R * sqrtf(uni(rng));
        float theta = 2.0f * glm::pi<float>() * uni(rng);
        float z = (uni(rng) - 0.5f) * 10.0f;
        glm::vec3 pos(r * cosf(theta), z, r * sinf(theta));
        glm::vec3 vel = glm::vec3(-sinf(theta), 0.0f, cosf(theta)) * sqrtf(1.0f / (r + 1.0f)) * 50.0f;
        particles[i].position = pos;
        particles[i].velocity = vel;
        particles[i].mass = 1.0f;
        particles[i].radius = 0.5f;
        particles[i].color = glm::vec4(0.7f + 0.3f * uni(rng), 0.7f, 1.0f, 1.0f);
    }
    // central massive body
    particles[0].mass = 100000.0f;
    particles[0].radius = 5.0f;
    particles[0].position = glm::vec3(0);
    particles[0].velocity = glm::vec3(0);
    particles[0].color = glm::vec4(5.0f, 4.0f, 2.0f, 1.0f);
}

void SimulationEngine::initBlackHole(int n) {
    particles.resize(n);
    std::uniform_real_distribution<float> uni(0.0f, 1.0f);
    float R = 400.0f;
    for (int i = 0; i < n; ++i) {
        float r = R * sqrtf(uni(rng));
        float theta = 2.0f * glm::pi<float>() * uni(rng);
        float z = (uni(rng) - 0.5f) * 2.0f;
        glm::vec3 pos(r * cosf(theta), z, r * sinf(theta));
        glm::vec3 vel = glm::vec3(-sinf(theta), 0.0f, cosf(theta)) * sqrtf(1.0f / (r + 1.0f)) * 80.0f;
        particles[i].position = pos;
        particles[i].velocity = vel;
        particles[i].mass = 1.0f;
        particles[i].radius = 0.5f;
        particles[i].color = glm::vec4(1.0f, 0.9f, 0.6f, 1.0f);
    }
    // central black hole (non-rendered via particle shader; renderer can add special effect)
    if (!particles.empty()) {
        particles[0].mass = 200000.0f;
        particles[0].radius = 8.0f; // event horizon approx
        particles[0].position = glm::vec3(0);
        particles[0].velocity = glm::vec3(0);
        particles[0].color = glm::vec4(10.0f, 8.0f, 6.0f, 1.0f);
    }
}

void SimulationEngine::applyBlackHoleEventHorizon() {
    if (particles.empty()) return;
    const Particle& bhole = particles[0];
    const float horizon = bhole.radius * 1.2f;
    particles.erase(std::remove_if(particles.begin()+1, particles.end(), [&](const Particle& p){
        return glm::length(p.position - bhole.position) < horizon;
    }), particles.end());
}

void SimulationEngine::initSupernova(int n) {
    particles.resize(n);
    std::uniform_real_distribution<float> uni(0.0f, 1.0f);
    for (int i = 0; i < n; ++i) {
        glm::vec3 dir = glm::normalize(glm::vec3(uni(rng) - 0.5f, uni(rng) - 0.5f, uni(rng) - 0.5f));
        float speed = 200.0f * uni(rng);
        particles[i].position = glm::vec3(0.0f);
        particles[i].velocity = dir * speed;
        particles[i].mass = 0.5f;
        particles[i].radius = 0.6f;
        particles[i].color = glm::vec4(2.0f, 0.5f + uni(rng) * 0.5f, 0.2f, 1.0f);
    }
}

void SimulationEngine::initInteractions(int n) {
    particles.resize(n);
    std::uniform_real_distribution<float> uni(0.0f, 1.0f);
    for (int i = 0; i < n; ++i) {
        particles[i].position = glm::vec3((uni(rng) - 0.5f) * 200.0f, (uni(rng) - 0.5f) * 200.0f, (uni(rng) - 0.5f) * 200.0f);
        particles[i].velocity = glm::vec3(0.0f);
        particles[i].mass = 1.0f;
        particles[i].radius = 1.0f;
        particles[i].color = glm::vec4(0.8f, 0.9f, 1.0f, 1.0f);
    }
}
