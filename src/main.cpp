#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include <cstdio>

#include "core/SimulationEngine.h"
#include "rendering/RenderingEngine.h"
#include "ui/UIManager.h"

static void glfwErrorCallback(int error, const char* desc) {
    fprintf(stderr, "GLFW error %d: %s\n", error, desc);
}

int main() {
    glfwSetErrorCallback(glfwErrorCallback);
    if (!glfwInit()) return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1600, 900, "Cosmos Engine", nullptr, nullptr);
    if (!window) { glfwTerminate(); return -1; }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        glfwTerminate();
        return -1;
    }

    UIManager ui; ui.init(window, "#version 450");

    SimulationEngine sim;
    SimulationSettings settings;
    settings.particleCount = 80000; // cap enforced to 200k on reset
    sim.reset(settings);

    RenderingEngine renderer;
    renderer.init(1600, 900);

    Camera camera;
    auto lastTime = std::chrono::high_resolution_clock::now();
    double fps = 0.0;

    // Input state (orbit camera)
    bool rotating = false; bool panning = false;
    double lastX = 0, lastY = 0; double scrollAccum = 0.0;
    float lastYaw = 0.0f;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Resize
        int fbw=0, fbh=0; glfwGetFramebufferSize(window, &fbw, &fbh);
        renderer.resize(fbw, fbh);

        // Orbit camera controls: RMB rotate around target, MMB pan target, wheel zoom
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
            double x,y; glfwGetCursorPos(window, &x,&y);
            if (!rotating) { rotating = true; lastX = x; lastY = y; }
            double dx = x - lastX, dy = y - lastY; lastX = x; lastY = y;
            float prevYaw = camera.yaw;
            camera.yaw += (float)dx * 0.005f;
            camera.pitch = glm::clamp(camera.pitch + (float)dy * 0.005f, -1.5f, 1.5f);
            // rotate scene to keep visual lock (optional: only for BH)
            if (settings.module == SimulationModule::BlackHole) {
                sim.rotateAll(camera.yaw - prevYaw);
            }
        } else rotating = false;

        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS) {
            double x,y; glfwGetCursorPos(window, &x,&y);
            if (!panning) { panning = true; lastX = x; lastY = y; }
            double dx = x - lastX, dy = y - lastY; lastX = x; lastY = y;
            glm::vec3 forward = glm::vec3(cosf(camera.pitch) * sinf(camera.yaw), sinf(camera.pitch), cosf(camera.pitch) * cosf(camera.yaw));
            glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0,1,0)));
            glm::vec3 up = glm::normalize(glm::cross(right, forward));
            camera.target -= right * (float)dx * 0.5f;
            camera.target += up * (float)dy * 0.5f;
        } else panning = false;

        // Wheel zoom (approx): usar teclas como fallback
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) scrollAccum -= 2.0;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) scrollAccum += 2.0;
        camera.distance = glm::clamp(camera.distance + (float)scrollAccum, 50.0f, 5000.0f);
        scrollAccum *= 0.85; // decay

        // Recompute position from orbit params
        glm::vec3 dir = glm::normalize(glm::vec3(cosf(camera.pitch) * sinf(camera.yaw), sinf(camera.pitch), cosf(camera.pitch) * cosf(camera.yaw)));
        camera.position = camera.target - dir * camera.distance;

        // Map left-click to world position near camera target plane
        double mx, my; glfwGetCursorPos(window, &mx, &my);
        int ww, hh; glfwGetWindowSize(window, &ww, &hh);
        bool lmb = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        settings.toolEngaged = lmb;
        if (lmb) {
            // Convert mouse to NDC
            float x = (float)((mx / ww) * 2.0 - 1.0);
            float y = (float)((1.0 - my / hh) * 2.0 - 1.0);
            // Build ray in view space
            float aspect = (float)ww / (float)hh;
            glm::mat4 proj = glm::perspective(glm::radians(camera.fov), aspect, 0.1f, 5000.0f);
            glm::vec3 fwd = glm::normalize(glm::vec3(cosf(camera.pitch) * sinf(camera.yaw), sinf(camera.pitch), cosf(camera.pitch) * cosf(camera.yaw)));
            glm::vec3 right = glm::normalize(glm::cross(fwd, glm::vec3(0,1,0)));
            glm::vec3 up = glm::normalize(glm::cross(right, fwd));
            glm::mat4 view = glm::lookAt(camera.position, camera.position + fwd, up);
            glm::mat4 invVP = glm::inverse(proj * view);
            glm::vec4 p0 = invVP * glm::vec4(x, y, -1.0f, 1.0f);
            glm::vec4 p1 = invVP * glm::vec4(x, y,  1.0f, 1.0f);
            p0 /= p0.w; p1 /= p1.w;
            glm::vec3 rayO = glm::vec3(p0);
            glm::vec3 rayD = glm::normalize(glm::vec3(p1 - p0));
            // Intersect with plane through camera.target with normal = up x right (i.e., plane perpendicular to forward)
            glm::vec3 planeN = fwd; // plane facing camera
            float denom = glm::dot(planeN, rayD);
            glm::vec3 hit = camera.target;
            if (fabsf(denom) > 1e-4f) {
                float t = glm::dot(camera.target - rayO, planeN) / denom;
                if (t > 0.0f) hit = rayO + rayD * t; else hit = camera.target;
            }
            settings.toolWorld = hit;
        }

        // Update simulation
        sim.update(settings);

        // UI frame
        ui.beginFrame();
        auto now = std::chrono::high_resolution_clock::now();
        double dt = std::chrono::duration<double>(now - lastTime).count();
        lastTime = now;
        fps = 1.0 / (dt + 1e-6);
    bool reset = ui.drawDock(settings, camera, (float)fps, sim.getParticles().size(), &renderer);
    if (settings.particleCount > 200000) settings.particleCount = 200000;
        if (reset) sim.reset(settings);

    // Render (enable lensing when module is BlackHole)
    bool isBH = (settings.module == SimulationModule::BlackHole);
    renderer.render(sim, camera, false, isBH);

        // Draw indicator for tool
        if (settings.toolEngaged && settings.tool != InteractionTool::None) {
            ImGui::SetNextWindowBgAlpha(0.3f);
            ImGui::Begin("Interaccion", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs);
            ImGui::Text("(%.1f, %.1f, %.1f)", settings.toolWorld.x, settings.toolWorld.y, settings.toolWorld.z);
            ImGui::End();
        }

        ui.endFrame();
        glfwSwapBuffers(window);
    }

    ui.shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
