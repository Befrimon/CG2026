#version 450
void main() {
    vec2 uvs[3] = vec2[](vec2(0,0), vec2(2,0), vec2(0,2));
    gl_Position = vec4(uvs[gl_VertexIndex]*2.0 - 1.0, 0.0, 1.0);
}
