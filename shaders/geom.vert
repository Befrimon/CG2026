#version 450
layout(location=0) in vec3 inPos;
layout(location=1) in vec3 inNorm;
layout(location=2) in vec2 inUV;
layout(binding=0) uniform UBO { mat4 view; mat4 proj; } ubo;
layout(push_constant) uniform PC { mat4 model; vec4 diff; vec4 spec; vec4 amb; float shin; } pc;
layout(location=0) out vec3 outPos;
layout(location=1) out vec3 outNorm;
layout(location=2) out vec2 outUV;
void main() {
    vec4 wPos = pc.model * vec4(inPos, 1.0);
    outPos = wPos.xyz;
    outNorm = mat3(transpose(inverse(pc.model))) * inNorm;
    outUV = inUV;
    gl_Position = ubo.proj * ubo.view * wPos;
}
