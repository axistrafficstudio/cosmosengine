#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "../core/SimulationEngine.h"
#include "ShaderProgram.h"

struct Camera {
    glm::vec3 position{0.0f, 50.0f, 900.0f};
    glm::vec3 target{0.0f, 0.0f, 0.0f};
    float pitch{-0.1f};
    float yaw{3.14f};
    float fov{60.0f};
    float distance{900.0f};
};

class RenderingEngine {
public:
    bool init(int width, int height);
    void resize(int width, int height);
    void render(const SimulationEngine& sim, const Camera& camera, bool showVectors, bool isBlackHoleModule);
    ~RenderingEngine();

    // Post-process controls
    void setExposure(float v) { exposure = v; }
    void setBloomThreshold(float v) { bloomThreshold = v; }
    float getExposure() const { return exposure; }
    float getBloomThreshold() const { return bloomThreshold; }
    void setBlurPasses(int p) { blurPasses = p; }
    int getBlurPasses() const { return blurPasses; }

private:
    struct GPUVertex {
        glm::vec3 position;
        float radius;
        glm::vec4 color;
    glm::vec3 velocity;
    float pad0;
    };
    unsigned int particleVAO = 0, particleVBO = 0;
    void* mappedPtr = nullptr;
    size_t mappedCapacity = 0;
    unsigned int quadVAO = 0, quadVBO = 0;
    unsigned int hdrFBO = 0, colorTex = 0, brightTex = 0, depthRBO = 0;
    unsigned int pingpongFBO[2]{0,0}, pingpongTex[2]{0,0};
    int pingW = 1, pingH = 1;
    // UI glass blur (quarter res)
    unsigned int uiFBO[2]{0,0}, uiTex[2]{0,0};
    int uiW = 1, uiH = 1;
    int uiLastIndex = 0; // which uiTex currently holds last blur output

    ShaderProgram particleProg;
    ShaderProgram blurProg;
    ShaderProgram compositeProg;

    int viewportW = 1, viewportH = 1;
    float exposure = 1.2f;
    float bloomThreshold = 0.6f;
    int blurPasses = 3;
    int uiBlurPasses = 6;
    std::vector<GPUVertex> gpuVertices;

    // Black hole lensing and ring parameters
    bool lensingEnabled = false;
    float lensStrength = 0.25f;  // warp intensity
    float lensRadiusScale = 1.0f; // scale vs projected event horizon
    float ringIntensity = 1.2f;
    float ringWidth = 0.06f; // normalized to lens radius
    float beamingStrength = 0.6f; // doppler-like boost
    glm::vec3 diskInnerColor{1.2f, 0.6f, 0.2f};
    glm::vec3 diskOuterColor{1.0f, 0.8f, 0.5f};
    // Accretion disk shape/orientation
    float diskInnerR = 0.6f;   // in units of lensRadius
    float diskOuterR = 1.6f;
    float diskTilt = 0.6f;     // 0..0.9 (0 = face-on)
    float diskPA = 0.0f;       // radians rotation
    float diskBrightness = 1.0f;
    float diskRotSpeed = 1.5f; // angular speed for streaks
    // Time
    double timeStart = 0.0;
    float timeElapsed = 0.0f;
    // Background and extra effects
    float starDensity = 0.6f;
    float haloIntensity = 0.8f;
    float tailAngle = 0.6f; // radians, direction of matter tail

    void setupParticleBuffers(size_t maxParticles);
    void ensureFramebuffer();
    void drawFullscreenQuad();

public:
    // Expose for UI glass sampling
    unsigned int getUIBlurTexture() const { return uiTex[uiLastIndex]; }
    int getViewportWidth() const { return viewportW; }
    int getViewportHeight() const { return viewportH; }
    void setUIBlurPasses(int p) { uiBlurPasses = p; }
    int getUIBlurPasses() const { return uiBlurPasses; }
    // Black hole UI controls
    void setLensStrength(float v){ lensStrength = v; }
    float getLensStrength() const { return lensStrength; }
    void setLensRadiusScale(float v){ lensRadiusScale = v; }
    float getLensRadiusScale() const { return lensRadiusScale; }
    void setRingIntensity(float v){ ringIntensity = v; }
    float getRingIntensity() const { return ringIntensity; }
    void setRingWidth(float v){ ringWidth = v; }
    float getRingWidth() const { return ringWidth; }
    void setBeamingStrength(float v){ beamingStrength = v; }
    float getBeamingStrength() const { return beamingStrength; }
    void setDiskColors(const glm::vec3& inner, const glm::vec3& outer){ diskInnerColor=inner; diskOuterColor=outer; }
    glm::vec3 getDiskInnerColor() const { return diskInnerColor; }
    glm::vec3 getDiskOuterColor() const { return diskOuterColor; }
    void setDiskRadii(float innerR, float outerR){ diskInnerR=innerR; diskOuterR=outerR; }
    float getDiskInnerR() const { return diskInnerR; }
    float getDiskOuterR() const { return diskOuterR; }
    void setDiskTilt(float t){ diskTilt=t; }
    float getDiskTilt() const { return diskTilt; }
    void setDiskPA(float a){ diskPA=a; }
    float getDiskPA() const { return diskPA; }
    void setDiskBrightness(float b){ diskBrightness=b; }
    float getDiskBrightness() const { return diskBrightness; }
    void setDiskRotSpeed(float w){ diskRotSpeed=w; }
    float getDiskRotSpeed() const { return diskRotSpeed; }
    // Background & extras
    void setStarDensity(float d){ starDensity=d; }
    float getStarDensity() const { return starDensity; }
    void setHaloIntensity(float h){ haloIntensity=h; }
    float getHaloIntensity() const { return haloIntensity; }
    void setTailAngle(float a){ tailAngle=a; }
    float getTailAngle() const { return tailAngle; }
};
