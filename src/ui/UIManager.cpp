#include "UIManager.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

bool UIManager::init(GLFWwindow* window, const char* glslVersion) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glslVersion);
    return true;
}

void UIManager::shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void UIManager::beginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void UIManager::endFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

bool UIManager::drawDock(SimulationSettings& s, Camera& cam, float fps, size_t particleCount, RenderingEngine* renderer) {
    bool resetRequested = false;

    ImGui::Begin("Cosmos Engine", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("FPS: %.1f", fps);
    ImGui::Text("Particles: %zu", particleCount);

    const char* modules[] = {"Galaxy", "Black Hole", "Supernova", "Interactions"};
    int mi = (int)s.module;
    if (ImGui::Combo("Simulation", &mi, modules, IM_ARRAYSIZE(modules))) {
        s.module = (SimulationModule)mi;
        resetRequested = true;
    }

    ImGui::Separator();
    ImGui::SliderInt("Count", &s.particleCount, 1000, 1000000);
    ImGui::SliderFloat("dt", &s.timeStep, 0.0001f, 0.05f, "%.4f");
    ImGui::SliderFloat("Damping", &s.damping, 0.0f, 0.2f);
    ImGui::SliderFloat("G", &s.gravityG, 0.01f, 5.0f);
    ImGui::SliderFloat("Softening", &s.softening, 0.0f, 0.1f);
    ImGui::SliderFloat("Theta", &s.theta, 0.4f, 1.2f);
    ImGui::Checkbox("Collisions", &s.collisions);
    ImGui::SliderFloat("Restitution", &s.restitution, 0.0f, 1.0f);
    ImGui::SliderInt("Rebuild Tree Every N Frames", &s.rebuildEveryN, 1, 10);

    if (renderer) {
        ImGui::Separator();
        float exposure = renderer->getExposure();
        float threshold = renderer->getBloomThreshold();
        if (ImGui::SliderFloat("Exposure", &exposure, 0.1f, 3.0f)) renderer->setExposure(exposure);
        if (ImGui::SliderFloat("Bloom Threshold", &threshold, 0.1f, 5.0f)) renderer->setBloomThreshold(threshold);
    }

    ImGui::Separator();
    ImGui::Text("Camera");
    ImGui::SliderFloat("FOV", &cam.fov, 20.f, 90.f);
    ImGui::SliderFloat3("Position", &cam.position.x, -2000.f, 2000.f);

    ImGui::Separator();
    if (ImGui::Button("Reset")) resetRequested = true;

    ImGui::End();

    return resetRequested;
}
