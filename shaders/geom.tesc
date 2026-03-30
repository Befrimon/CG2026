#version 450
layout (vertices = 3) out;

layout(location = 0) in vec3 inPos[];
layout(location = 1) in vec3 inNormal[];
layout(location = 2) in vec2 inUV[];
layout(location = 3) in vec3 inTangent[];

layout(location = 0) out vec3 outPos[];
layout(location = 1) out vec3 outNormal[];
layout(location = 2) out vec2 outUV[];
layout(location = 3) out vec3 outTangent[];

layout(binding = 0) uniform GeomUBO {
    mat4 view;
    mat4 proj;
    vec4 camPos;
} ubo;

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
    outPos[gl_InvocationID] = inPos[gl_InvocationID];
    outNormal[gl_InvocationID] = inNormal[gl_InvocationID];
    outUV[gl_InvocationID] = inUV[gl_InvocationID];
    outTangent[gl_InvocationID] = inTangent[gl_InvocationID];

    if (gl_InvocationID == 0) {
        // --- 2. ОПТИМИЗАЦИЯ ТЕССЕЛЯЦИИ ---
        if (push.hasDisp < 0.5) {
            // Если карты рельефа нет - треугольник остается 1 к 1 (супер-быстро)
            gl_TessLevelInner[0] = 1.0;
            gl_TessLevelOuter[0] = 1.0;
            gl_TessLevelOuter[1] = 1.0;
            gl_TessLevelOuter[2] = 1.0;
        } else {
            vec3 center = (inPos[0] + inPos[1] + inPos[2]) / 3.0;
            float dist = distance(ubo.camPos.xyz, center);

            float maxTess = 4.0;  // Снижено для FPS
            float minTess = 1.0;
            float maxDist = 40.0; // Тесселируем кирпичи только если подошли вплотную
            float minDist = 5.0;

            float factor = clamp((dist - minDist) / (maxDist - minDist), 0.0, 1.0);
            float tessLevel = mix(maxTess, minTess, factor);

            gl_TessLevelInner[0] = tessLevel;
            gl_TessLevelOuter[0] = tessLevel;
            gl_TessLevelOuter[1] = tessLevel;
            gl_TessLevelOuter[2] = tessLevel;
        }
    }
}
