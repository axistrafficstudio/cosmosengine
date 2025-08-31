#pragma once
#include <string>
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>
#include "../core/SimulationEngine.h"
#include "../rendering/RenderingEngine.h"

class UIManager {
public:
    bool init(GLFWwindow* window, const char* glslVersion = "#version 450");
    void shutdown();
    void beginFrame();
    void endFrame();

    bool drawDock(SimulationSettings& settings, Camera& camera, float fps, size_t particleCount);

private:
    bool showVectors = false;
};
