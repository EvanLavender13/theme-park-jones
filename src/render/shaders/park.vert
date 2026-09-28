#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inColor;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec4 outColor;
layout(location = 2) out float outViewDistance;

// The ghost pipeline shares this shader and tests depth greater-or-equal against what the park
// pipeline wrote, so both must compute the same position for the same vertex.
invariant gl_Position;

// SDL_GPU binds vertex-stage uniform buffers at set 1. The same block as terrain.vert.
layout(set = 1, binding = 0) uniform Camera {
    mat4 viewProjection;
    vec4 eye;
} camera;

void main() {
    outNormal = inNormal;
    outColor = inColor;
    outViewDistance = distance(inPosition, camera.eye.xyz);
    gl_Position = camera.viewProjection * vec4(inPosition, 1.0);
}
