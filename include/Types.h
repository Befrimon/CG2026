#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <array>

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec3 tangent;

    static VkVertexInputBindingDescription binding() {
        return {0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX};
    }
    static std::array<VkVertexInputAttributeDescription, 4> attrs() {
        return {{
            {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos)},
            {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)},
            {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)},
            {3, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, tangent)},
        }};
    }
};

struct Material {
    glm::vec4 diffuse;
    glm::vec4 specular;
    glm::vec4 ambient;
    float shininess;
    float _pad[3];
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::string diffuseTex;
    std::string normalTex;
    std::string dispTex;
    Material material;
};

struct Light {
    glm::vec4 position;
    glm::vec4 color;
    glm::vec4 direction;
    glm::vec4 info;
    glm::mat4 lightSpaceMatrix;
};

struct GlobalUBO {
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec4 viewPos;
    glm::vec4 info;
    Light lights[100];
};

struct GeomUBO {
    glm::mat4 view;
    glm::mat4 proj;
    alignas(16) glm::vec4 camPos;
};

// ОБНОВЛЕНО: Добавлен флаг hasDisp и выравнивание до 128 байт
struct GeomPush {
    glm::mat4 model;
    glm::vec4 diff;
    glm::vec4 spec;
    glm::vec4 amb;
    float shininess;
    float hasDisp;
    float _pad[2];
};

struct ShadowPush {
    glm::mat4 mvp;
};
