#version 450
#extension GL_GOOGLE_include_directive : require

#include "light.glsl"
#include "tonemap.glsl"

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec4 inColor;
layout(location = 2) in float inViewDistance;

layout(location = 0) out vec4 outColor;

// Tints and marks keep their colors at every hour: a fixed light gives their faces shape, and only
// the distance fade follows the hour, to the sky as it shows on screen.
const vec3 MARK_LIGHT_DIRECTION = vec3(0.4, 1.0, 0.3);

void main() {
    float shade = 0.55 + 0.45 * max(dot(normalize(inNormal), normalize(MARK_LIGHT_DIRECTION)), 0.0);
    vec3 sky = displayed(light.sky.rgb, light.settings.y);
    vec3 color = mix(inColor.rgb * shade, sky, smoothstep(250.0, 700.0, inViewDistance));
    outColor = vec4(color, inColor.a);
}
