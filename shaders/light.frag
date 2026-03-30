#version 450
layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

struct Light {
    vec4 position;
    vec4 color;
    vec4 direction;
    vec4 info;
    mat4 lightSpaceMatrix;
};

layout(binding = 0) uniform GlobalUBO {
    mat4 view;
    mat4 proj;
    vec4 viewPos;
    vec4 info;
    Light lights[100]; // <--- ИЗМЕНЕНО С 128 НА 100
} ubo;

layout(binding = 1, input_attachment_index = 0) uniform subpassInput gPos;
layout(binding = 2, input_attachment_index = 1) uniform subpassInput gNorm;
layout(binding = 3, input_attachment_index = 2) uniform subpassInput gAlbedo;
layout(binding = 4) uniform sampler2D shadowMap;

void main() {
    vec3 fragPos = subpassLoad(gPos).xyz;
    vec3 normal = subpassLoad(gNorm).xyz;
    vec3 albedo = subpassLoad(gAlbedo).rgb;

    vec3 viewDir = normalize(ubo.viewPos.xyz - fragPos);
    vec3 result = albedo * 0.05; // ambient

    int numLights = int(ubo.info.x);
    for(int i = 0; i < numLights; i++) {
        Light light = ubo.lights[i];

        vec3 lightDir;
        float attenuation = 1.0;

        if (light.info.x == 1.0) { // Directional
            lightDir = normalize(-light.direction.xyz);
        } else { // Point / Spot
            lightDir = normalize(light.position.xyz - fragPos);
            float dist = length(light.position.xyz - fragPos);
            attenuation = 1.0 / (1.0 + 0.09 * dist + 0.032 * (dist * dist));
        }

        // Diffuse
        float diff = max(dot(normal, lightDir), 0.0);
        vec3 diffuse = diff * light.color.rgb * albedo;

        // Shadow calculation
        float shadow = 0.0;
        if (light.info.w >= 0.0) {
            vec4 fragPosLightSpace = light.lightSpaceMatrix * vec4(fragPos, 1.0);
            vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
            projCoords.xy = projCoords.xy * 0.5 + 0.5;

            if(projCoords.z >= 0.0 && projCoords.z <= 1.0) {
                float closestDepth = texture(shadowMap, projCoords.xy).r;
                float currentDepth = projCoords.z;
                float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.0005);
                shadow = currentDepth - bias > closestDepth ? 1.0 : 0.0;
            }
        }

        result += (1.0 - shadow) * diffuse * attenuation;
    }

    outColor = vec4(result, 1.0);
}
