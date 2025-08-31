#include "SimulationEngine.h"
#include <glm/gtc/constants.hpp>
#include <glm/gtx/norm.hpp>
#include <algorithm>

SimulationEngine::SimulationEngine() : rng(std::random_device{}()) {}

void SimulationEngine::reset(const SimulationSettings& s) {
    particles.clear();
    particles.shrink_to_fit();
    BarnesHutParams p; p.G = s.gravityG; p.softening = s.softening; p.theta = s.theta;
    bh = BarnesHut(p);

    switch (s.module) {
        case SimulationModule::Galaxy: initGalaxy(s.particleCount); break;
        case SimulationModule::BlackHole: initBlackHole(s.particleCount); break;
        case SimulationModule::Supernova: initSupernova(s.particleCount); break;
        case SimulationModule::Interactions: initInteractions(s.particleCount); break;
    }
}

void SimulationEngine::update(const SimulationSettings& s) {
    BarnesHutParams p; p.G = s.gravityG; p.softening = s.softening; p.theta = s.theta;
    bh = BarnesHut(p);
    bh.build(particles);

    // zero forces
    for (auto& pt : particles) pt.force = glm::vec3(0.0f);

    // compute forces
    for (int i = 0; i < (int)particles.size(); ++i) {
        particles[i].force = particles[i].mass * bh.computeForce(i, particles);
    }

    integrate(s);
    if (s.collisions) handleCollisions(s.restitution);
    if (s.module == SimulationModule::BlackHole) applyBlackHoleEventHorizon();
}

void SimulationEngine::integrate(const SimulationSettings& s) {
    float dt = s.timeStep;
    float damp = s.damping;
    for (auto& pt : particles) {
        glm::vec3 accel = (pt.mass > 0.0f) ? (pt.force / pt.mass) : glm::vec3(0.0f);
        pt.velocity += accel * dt;
        pt.velocity *= (1.0f - damp);
        pt.position += pt.velocity * dt;
    }
}

void SimulationEngine::handleCollisions(float restitution) {
    // naive O(n^2) for now; can be improved with spatial hashing
    for (size_t i = 0; i < particles.size(); ++i) {
        for (size_t j = i + 1; j < particles.size(); ++j) {
            glm::vec3 r = particles[j].position - particles[i].position;
            float dist2 = glm::length2(r);
            float minDist = particles[i].radius + particles[j].radius;
            if (dist2 < minDist * minDist) {
                glm::vec3 n = glm::normalize(r);
                float mi = particles[i].mass, mj = particles[j].mass;
                glm::vec3 vi = particles[i].velocity;
                glm::vec3 vj = particles[j].velocity;
                float vi_n = glm::dot(vi, n);
                float vj_n = glm::dot(vj, n);
                float pi = (2.0f * (vi_n - vj_n)) / (mi + mj);
                particles[i].velocity = vi - pi * mj * n * restitution;
                particles[j].velocity = vj + pi * mi * n * restitution;
                // separate
                float dist = sqrtf(dist2) + 1e-6f;
                float overlap = minDist - dist;
                particles[i].position -= n * (overlap * (mj / (mi + mj)));
                particles[j].position += n * (overlap * (mi / (mi + mj)));
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
