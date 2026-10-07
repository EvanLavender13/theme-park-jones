#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in uint inPiece;

layout(location = 0) out vec2 outGround;
layout(location = 1) flat out uint outPiece;
layout(location = 2) out float outViewDistance;

// SDL_GPU binds vertex-stage uniform buffers at set 1. The same block as park.vert.
layout(set = 1, binding = 0) uniform Camera {
    mat4 viewProjection;
    vec4 eye;
} camera;

void main() {
    outGround = inPosition.xz;
    outPiece = inPiece;
    outViewDistance = distance(inPosition, camera.eye.xyz);
    gl_Position = camera.viewProjection * vec4(inPosition, 1.0);
}
