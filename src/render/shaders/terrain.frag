#version 450

layout(location = 0) in vec3 inWorldPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in float inViewDistance;

layout(location = 0) out vec4 outColor;

const vec3 GRASS = vec3(0.36, 0.55, 0.27);
const vec3 SKY = vec3(0.62, 0.76, 0.90);
const vec3 SUN_DIRECTION = vec3(0.4, 1.0, 0.3);

// Antialiased grid line coverage in [0, 1] for lines every `spacing` meters.
float gridLine(vec2 coord, float spacing) {
    vec2 cell = coord / spacing;
    vec2 distanceToLine = abs(fract(cell - 0.5) - 0.5) / fwidth(cell);
    return 1.0 - clamp(min(distanceToLine.x, distanceToLine.y), 0.0, 1.0);
}

void main() {
    float light = 0.55 + 0.45 * max(dot(normalize(inNormal), normalize(SUN_DIRECTION)), 0.0);
    vec3 color = GRASS * light;

    float nearFade = 1.0 - smoothstep(40.0, 160.0, inViewDistance);
    color = mix(color, color * 0.8, gridLine(inWorldPosition.xz, 1.0) * 0.5 * nearFade);
    color = mix(color, color * 0.6, gridLine(inWorldPosition.xz, 10.0) * max(nearFade, 0.3));

    color = mix(color, SKY, smoothstep(250.0, 700.0, inViewDistance));
    outColor = vec4(color, 1.0);
}
