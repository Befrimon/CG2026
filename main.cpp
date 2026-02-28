#include "Window.h"
#include "Input.h"
#include "Timer.h"
#include "Renderer.h"
#include "Mesh.h"

#include <glm/gtc/matrix_transform.hpp>
#include <filesystem>
#include <iostream>
#include <string>
#include <cmath>

namespace fs = std::filesystem;

static std::string findOBJ() {
    if (!fs::exists("assets")) return {};
    for (auto& e : fs::directory_iterator("assets"))
        if (e.path().extension() == ".obj")
            return e.path().string();
    return {};
}

static std::string findTexture(const fs::path& dir, const std::string& filename = "") {
    for (auto& e : fs::directory_iterator(dir)) {
        if (e.path().string() != filename && filename != "")
          continue;
        auto ext = e.path().extension().string();
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg")
            return e.path().string();
    }
    return {};
}

int main() {
    std::string objPath = findOBJ();
    if (objPath.empty()) {
        std::cerr << "no .obj found in assets/\n";
        return 1;
    }

    Window   window(1280, 720, "VulkanApp");
    Input::get().init(window.handle());

    Renderer renderer(window);
    renderer.setLight({5, 8, 5}, {1, 1, 1}, 64.f);
    renderer.setUV({0, 0}, {1, 1});

    auto meshes = loadOBJ(objPath);
    if (meshes.empty()) {
        std::cerr << "failed to load " << objPath << "\n";
        return 1;
    }

    std::string fallbackTex = findTexture(fs::path(objPath).parent_path());
    if (!fallbackTex.empty())
        std::cout << "fallback texture: " << fallbackTex << "\n";

    for (auto& m : meshes) {
        if (m.texturePath.empty() && !fallbackTex.empty())
            m.texturePath = fallbackTex;
        renderer.uploadMesh(m);
    }

    glm::vec3 pos   = {0, 1, 3};
    float     yaw   = -90.f;
    float     pitch = -10.f;
    Timer     timer;

    while (!window.shouldClose()) {
        Input::get().beginFrame();
        window.poll();
        timer.tick();

        float dt    = timer.dt();
        float total = timer.total();

        if (Input::get().isDown(GLFW_KEY_ESCAPE)) break;

        auto d = Input::get().delta();
        yaw   += d.x * 0.12f;
        pitch -= d.y * 0.12f;
        pitch  = glm::clamp(pitch, -89.f, 89.f);

        glm::vec3 dir = glm::normalize(glm::vec3(
            cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
            sin(glm::radians(pitch)),
            sin(glm::radians(yaw)) * cos(glm::radians(pitch))
        ));
        glm::vec3 right = glm::normalize(glm::cross(dir, {0, 1, 0}));
        renderer.setLight({5 * glm::sin(total), 8, 5 * glm::cos(total)}, {1, 1, 1}, 64.f);
        renderer.setUV({0.f, total}, {1.f, 1.f});

        float speed = 5.f * dt;
        if (Input::get().isDown(GLFW_KEY_LEFT_SHIFT)) speed *= 3.f;
        if (Input::get().isDown(GLFW_KEY_W)) pos += dir   * speed;
        if (Input::get().isDown(GLFW_KEY_S)) pos -= dir   * speed;
        if (Input::get().isDown(GLFW_KEY_A)) pos -= right * speed;
        if (Input::get().isDown(GLFW_KEY_D)) pos += right * speed;
        if (Input::get().isDown(GLFW_KEY_SPACE))        pos.y += speed;
        if (Input::get().isDown(GLFW_KEY_LEFT_CONTROL)) pos.y -= speed;

        renderer.setCamera(pos, pos + dir, {0, 1, 0});

        if (!renderer.beginFrame()) continue;
        for (int i = 0; i < renderer.meshCount(); i++)
            renderer.draw(i,
              glm::mat4(
                0.1, 0.f, 0.f, 0.f,
                0.f, 0.1, 0.f, 0.f,
                0.f, 0.f, 0.1, 0.f,
                0.f, 0.f, 0.f, 0.1
              ) /* *glm::mat4(
                glm::cos(total), 0.f, glm::sin(total), 0.f,
                0.f,1.f,0.f,0.f,
                -glm::sin(total), 0.f, glm::cos(total), 0.f,
                0.f, 0.f, 0.f, 1.f
              )*/
            );
        renderer.endFrame();
    }
}
