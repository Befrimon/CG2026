#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "Window.h"
#include "Input.h"
#include "Timer.h"
#include "Mesh.h"
#include "Renderer.h"
#include <glm/gtc/matrix_transform.hpp>

int main() {
    Window window(1280, 720, "Hertra Framework");
    Input::get().init(window.handle());
    Renderer renderer(window);
    Timer timer;

    auto meshes = loadOBJ("assets/sponza.obj");
    for (const auto& m : meshes) {
        renderer.uploadMesh(m);
    }

    glm::vec3 pos(0, 10, 20);
    float yaw = -90.f, pitch = -20.f;

    while (!window.shouldClose()) {
        Input::get().beginFrame();
        window.poll();
        timer.tick();

        float dt = timer.dt();
        float total = timer.total();

        glm::vec2 md = Input::get().delta();
        yaw += md.x * 0.1f;
        pitch = glm::clamp(pitch - md.y * 0.1f, -89.f, 89.f);

        glm::vec3 dir;
        dir.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        dir.y = sin(glm::radians(pitch));
        dir.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        dir = glm::normalize(dir);

        glm::vec3 right = glm::normalize(glm::cross(dir, glm::vec3(0, 1, 0)));

        float speed = Input::get().isDown(GLFW_KEY_LEFT_SHIFT) ? 450.f : 250.f;
        if (Input::get().isDown(GLFW_KEY_W)) pos += dir * speed * dt;
        if (Input::get().isDown(GLFW_KEY_S)) pos -= dir * speed * dt;
        if (Input::get().isDown(GLFW_KEY_A)) pos -= right * speed * dt;
        if (Input::get().isDown(GLFW_KEY_D)) pos += right * speed * dt;

        renderer.clearLights();

        Light dirLight{};
        dirLight.direction = glm::normalize(glm::vec4(-0.5f, -1.0f, -0.5f, 0.0f));
        dirLight.color = glm::vec4(0.8f, 0.8f, 0.7f, 1.0f);
        dirLight.info.x = 1.0f; // Directional
        dirLight.info.w = 0.0f; // Shadow map quad 0

        // Более плотные рамки обеспечивают более высокое разрешение карты теней!
        glm::mat4 lProj = glm::ortho(-50.f, 50.f, -50.f, 50.f, 0.1f, 500.f);
        lProj[1][1] *= -1;
        glm::mat4 lView = glm::lookAt(glm::vec3(-dirLight.direction * 150.f), glm::vec3(0), glm::vec3(0, 1, 0));
        dirLight.lightSpaceMatrix = lProj * lView;
        renderer.addLight(dirLight);

        Light ptLight{};
        ptLight.position = glm::vec4(10.0f * glm::sin(total), 5.0f, 10.0f * glm::cos(total), 1.0f);
        ptLight.color = glm::vec4(1.0f, 0.2f, 0.2f, 1.0f);
        ptLight.info.x = 0.0f; // Point
        ptLight.info.w = -1.0f; // Нет теней
        renderer.addLight(ptLight);

        Light spotLight{};
        spotLight.position = glm::vec4(pos.x, pos.y, pos.z, 1.0f);
        spotLight.direction = glm::vec4(dir.x, dir.y, dir.z, 0.0f);
        spotLight.color = glm::vec4(0.0f, 0.8f, 1.0f, 1.0f);
        spotLight.info.x = 2.0f; // Spot
        spotLight.info.y = glm::cos(glm::radians(12.5f));
        spotLight.info.z = glm::cos(glm::radians(17.5f));
        spotLight.info.w = 1.0f; // Shadow map quad 1

        glm::mat4 sProj = glm::perspective(glm::radians(35.f), 1.0f, 0.1f, 1000.f);
        sProj[1][1] *= -1;
        glm::mat4 sView = glm::lookAt(pos, pos + dir, glm::vec3(0, 1, 0));
        spotLight.lightSpaceMatrix = sProj * sView;
        // renderer.addLight(spotLight);

        renderer.clearDraws();
        for (int i = 0; i < (int)meshes.size(); i++) {
            renderer.queueDraw(i, glm::mat4(1.0f));
        }

        glm::mat4 view = glm::lookAt(pos, pos + dir, glm::vec3(0, 1, 0));
        glm::mat4 proj = glm::perspective(glm::radians(60.f), window.aspect(), 0.1f, 10000.f);
        proj[1][1] *= -1;

        renderer.renderFrame(pos, view, proj);
    }
    return 0;
}
