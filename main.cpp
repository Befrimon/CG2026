#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "Window.h"
#include "Input.h"
#include "Timer.h"
#include "Mesh.h"
#include "Renderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <random>
#include <glm/gtc/constants.hpp>

MeshData createSphere(float radius, int sectors, int rings) {
    MeshData mesh;
    mesh.material.diffuse = glm::vec4(0.0f);
    mesh.material.specular = glm::vec4(0.0f);
    mesh.material.ambient = glm::vec4(1.0f);
    mesh.material.shininess = 0.0f;

    for (int i = 0; i <= rings; ++i) {
        float v = (float)i / rings;
        float phi = v * glm::pi<float>();
        for (int j = 0; j <= sectors; ++j) {
            float u = (float)j / sectors;
            float theta = u * 2.0f * glm::pi<float>();

            float x = glm::cos(theta) * glm::sin(phi);
            float y = glm::cos(phi);
            float z = glm::sin(theta) * glm::sin(phi);

            Vertex vert{};
            vert.pos = glm::vec3(x, y, z) * radius;
            vert.normal = glm::vec3(x, y, z);
            vert.uv = glm::vec2(u, v);
            // ДОБАВЛЕНО: Тангенс для сферы
            vert.tangent = glm::vec3(-glm::sin(theta), 0.0f, glm::cos(theta));
            mesh.vertices.push_back(vert);
        }
    }

    for (int i = 0; i < rings; ++i) {
        for (int j = 0; j < sectors; ++j) {
            uint32_t first = (i * (sectors + 1)) + j;
            uint32_t second = first + sectors + 1;
            mesh.indices.push_back(first);
            mesh.indices.push_back(second);
            mesh.indices.push_back(first + 1);
            mesh.indices.push_back(second);
            mesh.indices.push_back(second + 1);
            mesh.indices.push_back(first + 1);
        }
    }
    return mesh;
}

struct LightSphere {
    glm::vec3 pos;
    glm::vec4 color;
    float velocityY;
    float restTimer;
    bool isResting;

    void respawn(std::mt19937& rng) {
        std::uniform_real_distribution<float> distX(-80.0f, 80.0f);
        std::uniform_real_distribution<float> distZ(-30.0f, 30.0f);
        std::uniform_real_distribution<float> distY(80.0f, 150.0f);
        std::uniform_real_distribution<float> col(0.3f, 1.0f);
        std::uniform_real_distribution<float> timer(2.0f, 5.0f);

        pos = glm::vec3(distX(rng), distY(rng), distZ(rng));
        color = glm::vec4(col(rng), col(rng), col(rng), 1.0f);
        velocityY = 0.0f;
        restTimer = timer(rng);
        isResting = false;
    }
};

int main() {
    Window window(1280, 720, "Hertra Framework");
    Input::get().init(window.handle());
    Renderer renderer(window);
    Timer timer;

    auto meshes = loadOBJ("assets/san-miguel.obj");
    for (const auto& m : meshes) {
        renderer.uploadMesh(m);
    }
    int sponzaMeshCount = meshes.size();

    MeshData sphereData = createSphere(1.0f, 16, 16);
    renderer.uploadMesh(sphereData);
    int sphereMeshIdx = sponzaMeshCount;

    std::mt19937 rng(std::random_device{}());
    std::vector<LightSphere> spheres(100);
    for(auto& s : spheres) s.respawn(rng);

    glm::vec3 pos(0, 10, 20);
    float yaw = -90.f, pitch = -20.f;

    while (!window.shouldClose()) {
        Input::get().beginFrame();
        window.poll();
        timer.tick();

        float dt = timer.dt();

        glm::vec2 md = Input::get().delta();
        yaw += md.x * 0.1f;
        pitch = glm::clamp(pitch - md.y * 0.1f, -89.f, 89.f);

        glm::vec3 dir;
        dir.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        dir.y = sin(glm::radians(pitch));
        dir.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        dir = glm::normalize(dir);

        glm::vec3 right = glm::normalize(glm::cross(dir, glm::vec3(0, 1, 0)));

        float speed = Input::get().isDown(GLFW_KEY_LEFT_SHIFT) ? 50.f : 30.f;
        if (Input::get().isDown(GLFW_KEY_W)) pos += dir * speed * dt;
        if (Input::get().isDown(GLFW_KEY_S)) pos -= dir * speed * dt;
        if (Input::get().isDown(GLFW_KEY_A)) pos -= right * speed * dt;
        if (Input::get().isDown(GLFW_KEY_D)) pos += right * speed * dt;

        renderer.clearLights();
        renderer.clearDraws();

        for (int i = 0; i < sponzaMeshCount; i++) {
            renderer.queueDraw(i, glm::mat4(1.0f));
        }

        Light dirLight{};
        dirLight.direction = glm::normalize(glm::vec4(-0.5f, -1.0f, -0.5f, 0.0f));
        dirLight.color = glm::vec4(0.8f, 0.8f, 0.7f, 1.0f);
        dirLight.info.x = 1.0f; // Directional
        dirLight.info.w = 0.0f; // Shadow map quad 0
        glm::mat4 lProj = glm::ortho(-50.f, 50.f, -50.f, 50.f, 0.1f, 500.f);
        lProj[1][1] *= -1;
        glm::mat4 lView = glm::lookAt(glm::vec3(-dirLight.direction * 150.f), glm::vec3(0), glm::vec3(0, 1, 0));
        dirLight.lightSpaceMatrix = lProj * lView;
        renderer.addLight(dirLight);

        for(auto& s : spheres) {
            if (!s.isResting) {
                s.velocityY -= 90.8f * dt;
                s.pos.y += s.velocityY * dt;

                if (s.pos.y <= 2.0f) {
                    s.pos.y = 2.0f;
                    s.isResting = true;
                }
            } else {
                s.restTimer -= dt;
                if (s.restTimer <= 0.0f) {
                    s.respawn(rng);
                }
            }

            Light ptLight{};
            ptLight.position = glm::vec4(s.pos, 1.0f);
            ptLight.color = s.color * 2.0f;
            ptLight.info.x = 0.0f;
            ptLight.info.w = -1.0f;

            renderer.addLight(ptLight);
        }

        glm::mat4 view = glm::lookAt(pos, pos + dir, glm::vec3(0, 1, 0));
        glm::mat4 proj = glm::perspective(glm::radians(60.f), window.aspect(), 0.1f, 10000.f);
        proj[1][1] *= -1;

        renderer.renderFrame(pos, view, proj);
    }
    return 0;
}
