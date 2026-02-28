#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(binding = 0) uniform UBO {
    mat4  model;
    mat4  view;
    mat4  proj;
    vec4  lightPos;
    vec4  lightColor;
    vec4  viewPos;
    vec2  uvOffset;
    vec2  uvScale;
    float checkerSize;
} ubo;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragUV;

void main() {
    vec4 worldPos = ubo.model * vec4(inPos, 1.0);

    fragPos = worldPos.xyz;
    fragNormal = mat3(transpose(inverse(ubo.model))) * inNormal;

    fragUV = inUV * ubo.uvScale + ubo.uvOffset;

    gl_Position = ubo.proj * ubo.view * worldPos;
}
