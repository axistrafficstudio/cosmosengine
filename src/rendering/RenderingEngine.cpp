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

    return true;
}

RenderingEngine::~RenderingEngine() {
    if (particleVBO && mappedPtr) {
        glBindBuffer(GL_ARRAY_BUFFER, particleVBO);
        glUnmapBuffer(GL_ARRAY_BUFFER);
        mappedPtr = nullptr;
    }
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
    if (width <= 0 || height <= 0) return;
    if (width == viewportW && height == viewportH) return;
    viewportW = width; viewportH = height;
    ensureFramebuffer();
}

void RenderingEngine::setupParticleBuffers(size_t maxParticles) {
    if (!particleVAO) glGenVertexArrays(1, &particleVAO);
    if (!particleVBO) glGenBuffers(1, &particleVBO);

    glBindVertexArray(particleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, particleVBO);
    // Use dynamic draw with glBufferData; stable across drivers
    mappedPtr = nullptr;
    mappedCapacity = maxParticles * sizeof(GPUVertex);
    glBufferData(GL_ARRAY_BUFFER, mappedCapacity, nullptr, GL_DYNAMIC_DRAW);

    // layout: position (vec3), radius (float), color(vec4), velocity(vec3), pad(float)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GPUVertex), (void*)offsetof(GPUVertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(GPUVertex), (void*)offsetof(GPUVertex, radius));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(GPUVertex), (void*)offsetof(GPUVertex, color));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(GPUVertex), (void*)offsetof(GPUVertex, velocity));
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

    // pingpong buffers for blur (half resolution)
    pingW = std::max(1, viewportW / 2);
    pingH = std::max(1, viewportH / 2);
    for (int i = 0; i < 2; ++i) {
        if (!pingpongFBO[i]) glGenFramebuffers(1, &pingpongFBO[i]);
        if (!pingpongTex[i]) glGenTextures(1, &pingpongTex[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
        glBindTexture(GL_TEXTURE_2D, pingpongTex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, pingW, pingH, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongTex[i], 0);
    }

    // UI blur buffers (quarter resolution of full viewport)
    uiW = std::max(1, viewportW / 4);
    uiH = std::max(1, viewportH / 4);
    for (int i = 0; i < 2; ++i) {
        if (!uiFBO[i]) glGenFramebuffers(1, &uiFBO[i]);
        if (!uiTex[i]) glGenTextures(1, &uiTex[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, uiFBO[i]);
        glBindTexture(GL_TEXTURE_2D, uiTex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, uiW, uiH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, uiTex[i], 0);
    }
    uiLastIndex = 0;

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

void RenderingEngine::render(const SimulationEngine& sim, const Camera& cam, bool showVectors, bool isBlackHoleModule) {
    const auto& pts = sim.getParticles();
    if (pts.empty()) return;

    // Resize if needed and upload compact GPU data
    glBindBuffer(GL_ARRAY_BUFFER, particleVBO);
    size_t needed = pts.size() * sizeof(GPUVertex);
    if (needed > mappedCapacity) {
        setupParticleBuffers(pts.size());
        glBindBuffer(GL_ARRAY_BUFFER, particleVBO);
    }
    gpuVertices.resize(pts.size());
    for (size_t i = 0; i < pts.size(); ++i) {
        gpuVertices[i].position = pts[i].position;
        gpuVertices[i].radius = pts[i].radius;
        gpuVertices[i].color = pts[i].color;
        gpuVertices[i].velocity = pts[i].velocity;
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
    glDisable(GL_DEPTH_TEST);

    // Render particles to MRT: color and bright
    particleProg.use();
    particleProg.setMat4("uView", view);
    particleProg.setMat4("uProj", proj);
    particleProg.setFloat("bloomThreshold", bloomThreshold);
    unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);

    glBindVertexArray(particleVAO);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glDrawArrays(GL_POINTS, 0, (GLsizei)pts.size());
    glDisable(GL_BLEND);
    glBindVertexArray(0);

    glDisable(GL_DEPTH_TEST);

    // Bright pass already separated via shader (uses two outputs), now blur brightTex
    bool horizontal = true, first = true;
    blurProg.use();
    blurProg.setInt("inputTex", 0);
    int passes = glm::clamp(blurPasses, 0, 10);
    for (int i = 0; i < passes; ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[horizontal]);
        blurProg.setInt("horizontal", horizontal ? 1 : 0);
        glActiveTexture(GL_TEXTURE0);
        if (first) {
            // Downsample: render brightTex to half res target
            glBindTexture(GL_TEXTURE_2D, brightTex);
        } else {
            glBindTexture(GL_TEXTURE_2D, pingpongTex[!horizontal]);
        }
        glViewport(0, 0, pingW, pingH);
        drawFullscreenQuad();
        horizontal = !horizontal;
        if (first) first = false;
    }
    glViewport(0, 0, viewportW, viewportH);

    // Composite
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    compositeProg.use();
    compositeProg.setFloat("exposure", exposure);
    // control black hole lensing + accretion ring shading
    lensingEnabled = isBlackHoleModule;
    compositeProg.setInt("lensEnabled", lensingEnabled ? 1 : 0);
    compositeProg.setFloat("lensStrength", lensStrength);
    compositeProg.setFloat("lensRadiusScale", lensRadiusScale);
    compositeProg.setFloat("ringIntensity", ringIntensity);
    compositeProg.setFloat("ringWidth", ringWidth);
    compositeProg.setFloat("beamingStrength", beamingStrength);
    compositeProg.setVec3("diskInnerColor", diskInnerColor);
    compositeProg.setVec3("diskOuterColor", diskOuterColor);
    compositeProg.setFloat("timeSec", timeElapsed);
    compositeProg.setFloat("starDensity", starDensity);
    compositeProg.setFloat("haloIntensity", haloIntensity);
    compositeProg.setFloat("tailAngle", tailAngle);
    compositeProg.setFloat("diskInnerR", diskInnerR);
    compositeProg.setFloat("diskOuterR", diskOuterR);
    compositeProg.setFloat("diskTilt", diskTilt);
    compositeProg.setFloat("diskPA", diskPA);
    compositeProg.setFloat("diskBrightness", diskBrightness);
    compositeProg.setFloat("diskRotSpeed", diskRotSpeed);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, colorTex);
    compositeProg.setInt("sceneTex", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, first ? brightTex : pingpongTex[!horizontal]);
    compositeProg.setInt("bloomTex", 1);
    drawFullscreenQuad();

    // Produce blurred backdrop for glass UI at quarter resolution
    // 1) Render composite into uiTex[0]
    glBindFramebuffer(GL_FRAMEBUFFER, uiFBO[0]);
    glViewport(0, 0, uiW, uiH);
    glClear(GL_COLOR_BUFFER_BIT);
    compositeProg.use();
    compositeProg.setFloat("exposure", exposure);
    compositeProg.setInt("lensEnabled", lensingEnabled ? 1 : 0);
    compositeProg.setFloat("lensStrength", lensStrength);
    compositeProg.setFloat("lensRadiusScale", lensRadiusScale);
    compositeProg.setFloat("ringIntensity", ringIntensity);
    compositeProg.setFloat("ringWidth", ringWidth);
    compositeProg.setFloat("beamingStrength", beamingStrength);
    compositeProg.setVec3("diskInnerColor", diskInnerColor);
    compositeProg.setVec3("diskOuterColor", diskOuterColor);
    compositeProg.setFloat("timeSec", timeElapsed);
    compositeProg.setFloat("starDensity", starDensity);
    compositeProg.setFloat("haloIntensity", haloIntensity);
    compositeProg.setFloat("tailAngle", tailAngle);
    compositeProg.setFloat("diskInnerR", diskInnerR);
    compositeProg.setFloat("diskOuterR", diskOuterR);
    compositeProg.setFloat("diskTilt", diskTilt);
    compositeProg.setFloat("diskPA", diskPA);
    compositeProg.setFloat("diskBrightness", diskBrightness);
    compositeProg.setFloat("diskRotSpeed", diskRotSpeed);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, colorTex);
    compositeProg.setInt("sceneTex", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, first ? brightTex : pingpongTex[!horizontal]);
    compositeProg.setInt("bloomTex", 1);
    drawFullscreenQuad();

    // 2) Blur ping-pong on uiTex
    bool h2 = true, first2 = true;
    blurProg.use();
    blurProg.setInt("inputTex", 0);
    int passes2 = glm::clamp(uiBlurPasses, 0, 12);
    for (int i = 0; i < passes2; ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, uiFBO[h2]);
        blurProg.setInt("horizontal", h2 ? 1 : 0);
        glActiveTexture(GL_TEXTURE0);
        if (first2) glBindTexture(GL_TEXTURE_2D, uiTex[0]); else glBindTexture(GL_TEXTURE_2D, uiTex[!h2]);
        glViewport(0, 0, uiW, uiH);
        drawFullscreenQuad();
        h2 = !h2;
        if (first2) first2 = false;
    }
    uiLastIndex = first2 ? 0 : (!h2);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, viewportW, viewportH);
}
