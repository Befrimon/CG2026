#version 450

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUV;

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

layout(binding = 1) uniform sampler2D tex1;
layout(binding = 2) uniform sampler2D tex2;

layout(location = 0) out vec4 outColor;

void main() {
    ivec2 cell    = ivec2(floor(fragUV * ubo.checkerSize));
    int   isOdd   = (cell.x + cell.y) & 1;
    vec4  texColor = isOdd == 0
                     ? texture(tex1, fragUV)
                     : texture(tex2, fragUV);

    vec3 N = normalize(fragNormal);
    vec3 L = normalize(ubo.lightPos.xyz - fragPos);
    vec3 V = normalize(ubo.viewPos.xyz  - fragPos);
    vec3 R = reflect(-L, N);

    float diff = max(dot(N, L), 0.0);

    float spec = pow(max(dot(R, V), 0.0), ubo.lightColor.w);

    vec3 ambient  = 0.1  * ubo.lightColor.rgb;
    vec3 diffuse  = diff * ubo.lightColor.rgb;
    vec3 specular = spec * ubo.lightColor.rgb;

    vec3 lit = (ambient + diffuse + specular) * texColor.rgb;
    outColor = vec4(lit, texColor.a);
}
