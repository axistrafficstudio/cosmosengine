#pragma once
#include <string>
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include "../core/SimulationEngine.h"
#include "../rendering/RenderingEngine.h"

class UIManager {
public:
    bool init(GLFWwindow* window, const char* glslVersion = "#version 450");
    void shutdown();
    void beginFrame();
    void endFrame();

    bool drawDock(SimulationSettings& settings, Camera& camera, float fps, size_t particleCount, RenderingEngine* renderer = nullptr);
    // Glassmorphic panel: draw a rounded translucent card with blurred scene
    void drawGlassPanelBegin(const char* title, RenderingEngine* renderer, const ImVec2& pos, const ImVec2& size, float alpha = 0.6f);
    void drawGlassPanelEnd();

private:
    bool showVectors = false;
};
