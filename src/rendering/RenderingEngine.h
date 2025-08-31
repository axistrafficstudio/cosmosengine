#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "../core/SimulationEngine.h"
#include "ShaderProgram.h"

struct Camera {
    glm::vec3 position{0.0f, 50.0f, 900.0f};
    float pitch{-0.1f};
    float yaw{3.14f};
    float fov{60.0f};
};

class RenderingEngine {
public:
    bool init(int width, int height);
    void resize(int width, int height);
    void render(const SimulationEngine& sim, const Camera& camera, bool showVectors);

private:
    unsigned int particleVAO = 0, particleVBO = 0;
    unsigned int quadVAO = 0, quadVBO = 0;
    unsigned int hdrFBO = 0, colorTex = 0, brightTex = 0, depthRBO = 0;
    unsigned int pingpongFBO[2]{0,0}, pingpongTex[2]{0,0};

    ShaderProgram particleProg;
    ShaderProgram brightProg;
    ShaderProgram blurProg;
    ShaderProgram compositeProg;

    int viewportW = 1, viewportH = 1;

    void setupParticleBuffers(size_t maxParticles);
    void ensureFramebuffer();
    void drawFullscreenQuad();
};
