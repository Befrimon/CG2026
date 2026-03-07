#version 450
struct Light { vec4 pos; vec4 col; vec4 dir; vec4 info; mat4 lsm; };

// Идеальное 16-байтное выравнивание:
layout(binding=0) uniform UBO {
    mat4 v;
    mat4 p;
    vec4 vp;
    vec4 info; // info.x = numLights
    Light l[4];
} ubo;

layout(input_attachment_index=0, binding=1) uniform subpassInput gPos;
layout(input_attachment_index=1, binding=2) uniform subpassInput gNorm;
layout(input_attachment_index=2, binding=3) uniform subpassInput gAlbedo;
layout(binding=4) uniform sampler2D shadowMap;

layout(location=0) out vec4 fragColor;

float ShadowCalc(vec4 posLS, int quad) {
    vec3 proj = posLS.xyz / posLS.w;
    proj.xy = proj.xy * 0.5 + 0.5;

    // Защита от артефактов за пределами теневой карты
    if (proj.z > 1.0 || proj.z < 0.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0)
        return 1.0;

    float qx = (quad % 2) * 0.5;
    float qy = (quad / 2) * 0.5;
    proj.xy = proj.xy * 0.5 + vec2(qx, qy);
    float c = texture(shadowMap, proj.xy).r;

    // Минимальный bias, так как у нас есть аппаратный
    return proj.z - 0.001 > c ? 0.2 : 1.0;
}

void main() {
    vec4 normData = subpassLoad(gNorm);
    if (length(normData.xyz) < 0.1) {
        fragColor = vec4(0.05, 0.05, 0.05, 1.0); // Фон
        return;
    }

    vec3 pos = subpassLoad(gPos).xyz;
    vec3 norm = normalize(normData.xyz);
    vec4 alb = subpassLoad(gAlbedo);

    float shininess = alb.a * 256.0;

    vec3 viewDir = normalize(ubo.vp.xyz - pos);
    vec3 res = alb.rgb * 0.05; // Ambient

    int numLights = int(ubo.info.x);

    for(int i = 0; i < numLights; i++) {
        Light l = ubo.l[i];
        vec3 L = l.info.x == 1.0 ? normalize(-l.dir.xyz) : normalize(l.pos.xyz - pos);

        float diff = max(dot(norm, L), 0.0);

        float spec = 0.0;
        if (shininess > 1.0 && diff > 0.0) {
            vec3 halfDir = normalize(L + viewDir);
            spec = pow(max(dot(norm, halfDir), 0.0), shininess) * 0.5;
        }

        float att = 1.0;
        if(l.info.x == 0.0) { // Point light
            float d = length(l.pos.xyz - pos);
            att = 1.0 / (1.0 + 0.09*d + 0.032*d*d);
        } else if(l.info.x == 2.0) { // Spot light
            float t = dot(L, normalize(-l.dir.xyz));
            float eps = l.info.y - l.info.z;
            att = clamp((t - l.info.z) / eps, 0.0, 1.0);
        }

        float sh = 1.0;
        if(l.info.w >= 0.0) sh = ShadowCalc(l.lsm * vec4(pos, 1.0), int(l.info.w));

        res += (alb.rgb * diff + spec) * l.col.rgb * att * sh;
    }

    fragColor = vec4(res, 1.0);
}
