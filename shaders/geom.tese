#version 450
layout(triangles, equal_spacing, ccw) in;

layout(location = 0) in vec3 inPos[];
layout(location = 1) in vec3 inNormal[];
layout(location = 2) in vec2 inUV[];
layout(location = 3) in vec3 inTangent[];

layout(location = 0) out vec3 outPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outUV;
layout(location = 3) out vec3 outTangent;

layout(binding = 0) uniform GeomUBO {
    mat4 view;
    mat4 proj;
    vec4 camPos;
} ubo;

layout(binding = 3) uniform sampler2D dispMap;

void main() {
    outUV = gl_TessCoord.x * inUV[0] + gl_TessCoord.y * inUV[1] + gl_TessCoord.z * inUV[2];
    outNormal = normalize(gl_TessCoord.x * inNormal[0] + gl_TessCoord.y * inNormal[1] + gl_TessCoord.z * inNormal[2]);
    outTangent = normalize(gl_TessCoord.x * inTangent[0] + gl_TessCoord.y * inTangent[1] + gl_TessCoord.z * inTangent[2]);

    vec3 p = gl_TessCoord.x * inPos[0] + gl_TessCoord.y * inPos[1] + gl_TessCoord.z * inPos[2];

    float displacement = texture(dispMap, outUV).r;
    float dispScale = 0.05;

    p += outNormal * (displacement * dispScale);
    outPos = p;

    gl_Position = ubo.proj * ubo.view * vec4(outPos, 1.0);
}
