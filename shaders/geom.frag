#version 450
layout(location=0) in vec3 inPos;
layout(location=1) in vec3 inNorm;
layout(location=2) in vec2 inUV;

layout(binding=1) uniform sampler2D tex1;
layout(binding=2) uniform sampler2D tex2;
layout(push_constant) uniform PC { mat4 model; vec4 diff; vec4 spec; vec4 amb; float shin; } pc;

layout(location=0) out vec4 outPos;
layout(location=1) out vec4 outNorm;
layout(location=2) out vec4 outAlbedo;

void main() {
    vec4 texColor = texture(tex1, inUV);

    // Если часть текстуры полностью прозрачна (например, листья) - отбрасываем пиксель
    if (texColor.a < 0.1) {
        discard;
    }

    vec3 diffColor = pc.diff.rgb;
    // Если материал не задал цвет (черный 0,0,0) - используем оригинальный цвет текстуры
    if (length(diffColor) < 0.01) {
        diffColor = vec3(1.0);
    }

    outPos = vec4(inPos, 1.0);
    outNorm = vec4(normalize(inNorm), 1.0);

    // В альфа-канал G-Buffer'а прячем параметр shininess для спекуляра в deferred pass
    outAlbedo = vec4(texColor.rgb * diffColor, clamp(pc.shin / 256.0, 0.0, 1.0));
}
