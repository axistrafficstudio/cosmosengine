#include "UIManager.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>

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

    // Modern glassmorphic panel
    ImVec2 panelSize = ImVec2(370, 540);
    ImVec2 panelPos = ImVec2(20, 20);
    drawGlassPanelBegin("Cosmos Engine", renderer, panelPos, panelSize, 0.55f);

    ImGui::Text("FPS: %.1f", fps);
    ImGui::Text("Partículas: %zu", particleCount);

    const char* modules[] = {"Galaxia", "Agujero Negro", "Supernova", "Interacciones"};
    int mi = (int)s.module;
    if (ImGui::Combo("Simulation", &mi, modules, IM_ARRAYSIZE(modules))) {
        s.module = (SimulationModule)mi;
        resetRequested = true;
    }

    ImGui::Separator();
    ImGui::Text("Interacciones");
    const char* tools[] = {"Ninguna", "Atraer", "Repeler", "Arrastrar"};
    int ti = (int)s.tool;
    if (ImGui::Combo("Herramienta", &ti, tools, IM_ARRAYSIZE(tools))) {
        s.tool = (InteractionTool)ti;
    }
    ImGui::SliderFloat("Radio", &s.toolRadius, 1.0f, 500.0f);
    ImGui::SliderFloat("Intensidad", &s.toolStrength, 1.0f, 5000.0f, "%.0f");
    if (s.particleCount > 200000) s.particleCount = 200000;
    ImGui::SliderInt("Cantidad", &s.particleCount, 1000, 200000);
    ImGui::SliderFloat("dt", &s.timeStep, 0.0001f, 0.05f, "%.4f");
    ImGui::SliderFloat("Amortiguación", &s.damping, 0.0f, 0.2f);
    ImGui::SliderFloat("G", &s.gravityG, 0.01f, 5.0f);
    ImGui::SliderFloat("Suavizado", &s.softening, 0.0f, 0.1f);
    ImGui::SliderFloat("Theta", &s.theta, 0.4f, 1.2f);
    ImGui::Checkbox("Colisiones", &s.collisions);
    ImGui::SliderFloat("Restitución", &s.restitution, 0.0f, 1.0f);
    ImGui::SliderInt("Rebuild Tree Every N Frames", &s.rebuildEveryN, 1, 10);

    if (renderer) {
        ImGui::Separator();
        float exposure = renderer->getExposure();
        float threshold = renderer->getBloomThreshold();
        if (ImGui::SliderFloat("Exposici3n", &exposure, 0.1f, 3.0f)) renderer->setExposure(exposure);
        if (ImGui::SliderFloat("Umbral Bloom", &threshold, 0.1f, 5.0f)) renderer->setBloomThreshold(threshold);
        int blurPasses = renderer->getBlurPasses();
        if (ImGui::SliderInt("Pasadas Bloom", &blurPasses, 0, 10)) renderer->setBlurPasses(blurPasses);
        int uiBlur = renderer->getUIBlurPasses();
        if (ImGui::SliderInt("Desenfoque UI", &uiBlur, 0, 12)) renderer->setUIBlurPasses(uiBlur);

    // Black hole visual controls
    ImGui::Separator();
    ImGui::Text("Agujero Negro (visual)");
    float lensK = renderer->getLensStrength();
    if (ImGui::SliderFloat("Fuerza lente", &lensK, 0.0f, 1.0f)) renderer->setLensStrength(lensK);
    float lensScale = renderer->getLensRadiusScale();
    if (ImGui::SliderFloat("Radio lente", &lensScale, 0.5f, 2.0f)) renderer->setLensRadiusScale(lensScale);
    float ringI = renderer->getRingIntensity();
    if (ImGui::SliderFloat("Brillo anillo", &ringI, 0.0f, 3.0f)) renderer->setRingIntensity(ringI);
    float ringW = renderer->getRingWidth();
    if (ImGui::SliderFloat("Grosor anillo", &ringW, 0.005f, 0.2f)) renderer->setRingWidth(ringW);
    float beam = renderer->getBeamingStrength();
    if (ImGui::SliderFloat("Beaming", &beam, 0.0f, 1.5f)) renderer->setBeamingStrength(beam);
    }

    ImGui::Separator();
    ImGui::Text("Cámara");
    ImGui::SliderFloat("FOV", &cam.fov, 20.f, 90.f);
    ImGui::SliderFloat3("Posición", &cam.position.x, -2000.f, 2000.f);

    ImGui::Separator();
    if (ImGui::Button("Reiniciar")) resetRequested = true;

    drawGlassPanelEnd();

    return resetRequested;
}

void UIManager::drawGlassPanelBegin(const char* title, RenderingEngine* renderer, const ImVec2& pos, const ImVec2& size, float alpha) {
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 16.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.9f, 0.9f, 0.95f, alpha));
    ImGui::Begin(title, nullptr, flags);

    // draw blurred backdrop inside window
    if (renderer) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p0 = ImGui::GetWindowPos();
        ImVec2 p1 = ImVec2(p0.x + size.x, p0.y + size.y);
        ImTextureID tex = (ImTextureID)(intptr_t)renderer->getUIBlurTexture();
        // UVs cover portion of the screen; here we sample full blurred tex
        dl->AddImageRounded(tex, p0, p1, ImVec2(0,0), ImVec2(1,1), IM_COL32_WHITE, 16.0f);
        // overlay frosted tint
        dl->AddRectFilled(p0, p1, IM_COL32(255,255,255,(int)(alpha*255)), 16.0f);
        // soft border
        dl->AddRect(p0, p1, IM_COL32(255,255,255,40), 16.0f, 0, 2.0f);
    }
    ImGui::Dummy(ImVec2(0, 8));
}

void UIManager::drawGlassPanelEnd() {
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    ImGui::End();
}
