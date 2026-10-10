#version 450
#extension GL_GOOGLE_include_directive : require

#include "light.glsl"
#include "tonemap.glsl"

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec4 inColor;
layout(location = 2) in float inViewDistance;

layout(location = 0) out vec4 outColor;

// A ghost's preview of the park, lit and tonemapped exactly as the committed park is, so where it
// lies on what is already there it blends in.
void main() {
    vec3 color = fadeToSky(linearOf(inColor.rgb) * incoming(inNormal), inViewDistance);
    outColor = vec4(displayed(color, light.settings.y), inColor.a);
}
