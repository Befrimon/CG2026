#version 450
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inTangent;

layout(location = 0) out vec4 gPos;
layout(location = 1) out vec4 gNorm;
layout(location = 2) out vec4 gAlbedo;

layout(binding = 1) uniform sampler2D diffuseMap;
layout(binding = 2) uniform sampler2D normalMap;

// ОБНОВЛЕНО
layout(push_constant) uniform Push {
    mat4 model;
    vec4 diff;
    vec4 spec;
    vec4 amb;
    float shininess;
    float hasDisp;
} push;

void main() {
    vec4 texColor = texture(diffuseMap, inUV);

    // --- 1. АЛЬФА ТЕСТ (ОБРЕЗКА ПО МАСКЕ) ---
    if (texColor.a < 0.5) {
        discard;
    }

    vec3 mapNorm = texture(normalMap, inUV).xyz * 2.0 - 1.0;

    vec3 N = normalize(inNormal);
    vec3 T = normalize(inTangent);

    T = normalize(T - dot(T, N) * N);

    vec3 finalNormal = N;
    if (length(T) > 0.1) {
        vec3 B = cross(N, T);
        mat3 TBN = mat3(T, B, N);
        finalNormal = normalize(TBN * mapNorm);
    }

    gPos = vec4(inPos, 1.0);
    gNorm = vec4(finalNormal, 1.0);
    gAlbedo = texColor * push.diff;
}
