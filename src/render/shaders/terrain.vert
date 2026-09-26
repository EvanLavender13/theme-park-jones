#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec3 outWorldPosition;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out float outViewDistance;

// SDL_GPU binds vertex-stage uniform buffers at set 1.
layout(set = 1, binding = 0) uniform Camera {
    mat4 viewProjection;
    vec4 eye;
} camera;

void main() {
    outWorldPosition = inPosition;
    outNormal = inNormal;
    outViewDistance = distance(inPosition, camera.eye.xyz);
    gl_Position = camera.viewProjection * vec4(inPosition, 1.0);
}
