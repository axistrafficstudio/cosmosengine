#include "RenderingEngine.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <cstdio>

static const float QUAD_VERTS[] = {
    // positions   // texcoords
    -1.f, -1.f, 0.f, 0.f,
     1.f, -1.f, 1.f, 0.f,
     1.f,  1.f, 1.f, 1.f,
    -1.f, -1.f, 0.f, 0.f,
     1.f,  1.f, 1.f, 1.f,
    -1.f,  1.f, 0.f, 1.f
};

bool RenderingEngine::init(int width, int height) {
    viewportW = width; viewportH = height;

    // particle shader
    particleProg.loadFromFiles("shaders/particles.vert", "shaders/particles.frag");
    blurProg.loadFromFiles("shaders/quad.vert", "shaders/gaussian_blur.frag");
    compositeProg.loadFromFiles("shaders/quad.vert", "shaders/composite.frag");

    setupParticleBuffers(1);

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(QUAD_VERTS), QUAD_VERTS, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);

    ensureFramebuffer();

    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);

    return true;
}

RenderingEngine::~RenderingEngine() {
    if (particleVBO) glDeleteBuffers(1, &particleVBO);
    if (particleVAO) glDeleteVertexArrays(1, &particleVAO);
    if (quadVBO) glDeleteBuffers(1, &quadVBO);
    if (quadVAO) glDeleteVertexArrays(1, &quadVAO);
    if (colorTex) glDeleteTextures(1, &colorTex);
    if (brightTex) glDeleteTextures(1, &brightTex);
    if (pingpongTex[0]) glDeleteTextures(1, &pingpongTex[0]);
    if (pingpongTex[1]) glDeleteTextures(1, &pingpongTex[1]);
    if (depthRBO) glDeleteRenderbuffers(1, &depthRBO);
    if (hdrFBO) glDeleteFramebuffers(1, &hdrFBO);
    if (pingpongFBO[0]) glDeleteFramebuffers(1, &pingpongFBO[0]);
    if (pingpongFBO[1]) glDeleteFramebuffers(1, &pingpongFBO[1]);
}

void RenderingEngine::resize(int width, int height) {
    viewportW = width; viewportH = height;
    ensureFramebuffer();
}

void RenderingEngine::setupParticleBuffers(size_t maxParticles) {
    if (!particleVAO) glGenVertexArrays(1, &particleVAO);
    if (!particleVBO) glGenBuffers(1, &particleVBO);

    glBindVertexArray(particleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, particleVBO);
    glBufferData(GL_ARRAY_BUFFER, maxParticles * sizeof(GPUVertex), nullptr, GL_DYNAMIC_DRAW);

    // layout: position (vec3), radius (float), color(vec4)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GPUVertex), (void*)offsetof(GPUVertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(GPUVertex), (void*)offsetof(GPUVertex, radius));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(GPUVertex), (void*)offsetof(GPUVertex, color));
    glBindVertexArray(0);
}

void RenderingEngine::ensureFramebuffer() {
    // Guard against zero-sized framebuffer (can happen before first valid resize)
    if (viewportW <= 0 || viewportH <= 0) { viewportW = 1; viewportH = 1; }
    if (!hdrFBO) glGenFramebuffers(1, &hdrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);

    auto createTex = [&](unsigned int& tex){
        if (!tex) glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, viewportW, viewportH, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    };

    createTex(colorTex);
    createTex(brightTex);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, brightTex, 0);

    if (!depthRBO) glGenRenderbuffers(1, &depthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, depthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, viewportW, viewportH);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRBO);

    unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);

    // pingpong buffers for blur
    for (int i = 0; i < 2; ++i) {
        if (!pingpongFBO[i]) glGenFramebuffers(1, &pingpongFBO[i]);
        if (!pingpongTex[i]) glGenTextures(1, &pingpongTex[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
        glBindTexture(GL_TEXTURE_2D, pingpongTex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, viewportW, viewportH, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongTex[i], 0);
    }

    // Check completeness
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "FBO incomplete: 0x%X (viewport %dx%d)\n", status, viewportW, viewportH);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderingEngine::drawFullscreenQuad() {
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void RenderingEngine::render(const SimulationEngine& sim, const Camera& cam, bool showVectors) {
    const auto& pts = sim.getParticles();
    if (pts.empty()) return;

    // Resize particle buffer if needed and upload compact GPU data
    glBindBuffer(GL_ARRAY_BUFFER, particleVBO);
    GLint size = 0; glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &size);
    size_t needed = pts.size() * sizeof(GPUVertex);
    if ((size_t)size < needed) {
        setupParticleBuffers(pts.size());
        glBindBuffer(GL_ARRAY_BUFFER, particleVBO);
    }
    gpuVertices.resize(pts.size());
    for (size_t i = 0; i < pts.size(); ++i) {
        gpuVertices[i].position = pts[i].position;
        gpuVertices[i].radius = pts[i].radius;
        gpuVertices[i].color = pts[i].color;
    }
    glBufferSubData(GL_ARRAY_BUFFER, 0, needed, gpuVertices.data());

    // Camera matrices
    float aspect = (float)viewportW / (float)viewportH;
    glm::mat4 proj = glm::perspective(glm::radians(cam.fov), aspect, 0.1f, 5000.0f);
    glm::vec3 fwd = glm::normalize(glm::vec3(cosf(cam.pitch) * sinf(cam.yaw), sinf(cam.pitch), cosf(cam.pitch) * cosf(cam.yaw)));
    glm::vec3 right = glm::normalize(glm::cross(fwd, glm::vec3(0,1,0)));
    glm::vec3 up = glm::normalize(glm::cross(right, fwd));
    glm::mat4 view = glm::lookAt(cam.position, cam.position + fwd, up);

    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
    glViewport(0, 0, viewportW, viewportH);
    glClearColor(0.0f, 0.0f, 0.02f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    // Render particles to MRT: color and bright
    particleProg.use();
    particleProg.setMat4("uView", view);
    particleProg.setMat4("uProj", proj);
    particleProg.setFloat("bloomThreshold", bloomThreshold);
    unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);

    glBindVertexArray(particleVAO);
    glDrawArrays(GL_POINTS, 0, (GLsizei)pts.size());
    glBindVertexArray(0);

    glDisable(GL_DEPTH_TEST);

    // Bright pass already separated via shader (uses two outputs), now blur brightTex
    bool horizontal = true, first = true;
    int blurPasses = 8;
    blurProg.use();
    blurProg.setInt("inputTex", 0);
    for (int i = 0; i < blurPasses; ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[horizontal]);
        blurProg.setInt("horizontal", horizontal ? 1 : 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, first ? brightTex : pingpongTex[!horizontal]);
        drawFullscreenQuad();
        horizontal = !horizontal;
        if (first) first = false;
    }

    // Composite
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    compositeProg.use();
    compositeProg.setFloat("exposure", exposure);
    // crude lensing switch by module: we don't have module here; always off. Kept for future UI toggle.
    compositeProg.setInt("lensEnabled", 0);
    compositeProg.setVec3("dummy", glm::vec3(0)); // no-op to keep uniform location active if optimized
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, colorTex);
    compositeProg.setInt("sceneTex", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, pingpongTex[!horizontal]);
    compositeProg.setInt("bloomTex", 1);
    drawFullscreenQuad();
}
