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
    settings.particleCount = 200000; // starting point
    sim.reset(settings);

    RenderingEngine renderer;
    renderer.init(1600, 900);

    Camera camera;
    auto lastTime = std::chrono::high_resolution_clock::now();
    double fps = 0.0;

    // Input state
    bool rotating = false; bool panning = false;
    double lastX = 0, lastY = 0;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Resize
        int fbw=0, fbh=0; glfwGetFramebufferSize(window, &fbw, &fbh);
        renderer.resize(fbw, fbh);

        // Camera controls
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
            double x,y; glfwGetCursorPos(window, &x,&y);
            if (!rotating) { rotating = true; lastX = x; lastY = y; }
            double dx = x - lastX, dy = y - lastY; lastX = x; lastY = y;
            camera.yaw += (float)dx * 0.005f;
            camera.pitch += (float)dy * 0.005f;
            camera.pitch = glm::clamp(camera.pitch, -1.5f, 1.5f);
        } else rotating = false;

        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS) {
            double x,y; glfwGetCursorPos(window, &x,&y);
            if (!panning) { panning = true; lastX = x; lastY = y; }
            double dx = x - lastX, dy = y - lastY; lastX = x; lastY = y;
            glm::vec3 forward = glm::vec3(cosf(camera.pitch) * sinf(camera.yaw), sinf(camera.pitch), cosf(camera.pitch) * cosf(camera.yaw));
            glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0,1,0)));
            glm::vec3 up = glm::normalize(glm::cross(right, forward));
            camera.position -= right * (float)dx * 0.5f;
            camera.position += up * (float)dy * 0.5f;
        } else panning = false;

    // WASD in camera space
    glm::vec3 forward = glm::normalize(glm::vec3(cosf(camera.pitch) * sinf(camera.yaw), sinf(camera.pitch), cosf(camera.pitch) * cosf(camera.yaw)));
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0,1,0)));
    glm::vec3 up = glm::normalize(glm::cross(right, forward));
    float moveSpeed = 10.0f;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.position += forward * moveSpeed;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.position -= forward * moveSpeed;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.position -= right * moveSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.position += right * moveSpeed;

        // Update simulation
        sim.update(settings);

        // UI frame
        ui.beginFrame();
        auto now = std::chrono::high_resolution_clock::now();
        double dt = std::chrono::duration<double>(now - lastTime).count();
        lastTime = now;
        fps = 1.0 / (dt + 1e-6);
    bool reset = ui.drawDock(settings, camera, (float)fps, sim.getParticles().size(), &renderer);
        if (reset) sim.reset(settings);

        // Render
        renderer.render(sim, camera, false);

        ui.endFrame();
        glfwSwapBuffers(window);
    }

    ui.shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
