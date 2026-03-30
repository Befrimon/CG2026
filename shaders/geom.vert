#version 450
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inTangent; // ПРИНИМАЕМ ТАНГЕНС ИЗ C++

layout(location = 0) out vec3 outPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outUV;
layout(location = 3) out vec3 outTangent;

layout(push_constant) uniform Push {
    mat4 model;
    vec4 diff;
    vec4 spec;
    vec4 amb;
    float shininess;
} push;

void main() {
    outPos = vec3(push.model * vec4(inPos, 1.0));
    // Нормаль и Тангенс умножаются на матрицу модели, чтобы вращаться вместе с объектом
    mat3 normalMatrix = mat3(transpose(inverse(push.model)));
    outNormal = normalMatrix * inNormal;
    outTangent = normalMatrix * inTangent;

    outUV = inUV;
}
