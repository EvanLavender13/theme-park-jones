#version 450

layout(location = 0) out vec2 outTexCoord;

// One triangle that covers the screen, from three vertices with no vertex buffer.
void main() {
    vec2 corner = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2));
    // Texture rows run down the screen while clip space's y runs up.
    outTexCoord = vec2(corner.x, 1.0 - corner.y);
    gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);
}
