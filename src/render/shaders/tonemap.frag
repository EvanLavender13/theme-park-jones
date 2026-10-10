#version 450
#extension GL_GOOGLE_include_directive : require

#include "light.glsl"
#include "tonemap.glsl"

layout(location = 0) in vec2 inTexCoord;

layout(location = 0) out vec4 outColor;

// SDL_GPU binds fragment-stage samplers at set 2.
layout(set = 2, binding = 0) uniform sampler2D scene;

void main() {
    outColor = vec4(displayed(texture(scene, inTexCoord).rgb, light.settings.y), 1.0);
}
