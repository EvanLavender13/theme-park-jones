#version 450

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec4 inColor;
layout(location = 2) in float inViewDistance;

layout(location = 0) out vec4 outColor;

const vec3 SKY = vec3(0.62, 0.76, 0.90);
const vec3 SUN_DIRECTION = vec3(0.4, 1.0, 0.3);

void main() {
    float light = 0.55 + 0.45 * max(dot(normalize(inNormal), normalize(SUN_DIRECTION)), 0.0);
    vec3 color = inColor.rgb * light;
    color = mix(color, SKY, smoothstep(250.0, 700.0, inViewDistance));
    outColor = vec4(color, inColor.a);
}
