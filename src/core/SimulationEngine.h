#pragma once
#include <vector>
#include <random>
#include <glm/glm.hpp>
#include "Particle.h"
#include "BarnesHut.h"

enum class SimulationModule {
    Galaxy,
    BlackHole,
    Supernova,
    Interactions
};

struct SimulationSettings {
    SimulationModule module = SimulationModule::Galaxy;
    int particleCount = 100000; // start with 100k; scalable
    float timeStep = 0.005f;
    float damping = 0.0f;
    float gravityG = 1.0f;
    float softening = 0.01f;
    float theta = 0.7f;
    bool collisions = false;
    float restitution = 1.0f; // 1 elastic, <1 inelastic
    int rebuildEveryN = 1; // build Barnes-Hut tree every N frames (1 = every frame)
};

class SimulationEngine {
public:
    SimulationEngine();
    void reset(const SimulationSettings& settings);
    void update(const SimulationSettings& settings);

    const std::vector<Particle>& getParticles() const { return particles; }
    std::vector<Particle>& getParticlesMutable() { return particles; }

private:
    std::vector<Particle> particles;
    BarnesHut bh;
    std::mt19937 rng;
    // performance controls
    int frameCounter = 0;
    size_t lastParticleCount = 0;
    BarnesHutParams lastBhParams{};

    void initGalaxy(int n);
    void initBlackHole(int n);
    void initSupernova(int n);
    void initInteractions(int n);
    void integrate(const SimulationSettings& settings);
    void handleCollisions(float restitution);
    void applyBlackHoleEventHorizon();
};
