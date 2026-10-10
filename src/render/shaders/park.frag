#version 450
#extension GL_GOOGLE_include_directive : require

#include "light.glsl"

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec4 inColor;
layout(location = 2) in float inViewDistance;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 color = linearOf(inColor.rgb) * incoming(inNormal);
    outColor = vec4(fadeToSky(color, inViewDistance), inColor.a);
}
