// The hour's light, shared by every shader that lights or tonemaps. SDL_GPU binds fragment-stage
// uniform buffers at set 3. Matches LightUniforms in renderer.cpp.
layout(std140, set = 3, binding = 0) uniform Light {
    vec4 sunDirection;
    vec4 sun;
    vec4 moonDirection;
    vec4 moon;
    vec4 ambientSky;
    vec4 ambientGround;
    vec4 sky;
    // x is 1 when the light gives brightness only, and y is the exposure.
    vec4 settings;
} light;

vec3 linearOf(vec3 encoded) {
    return mix(encoded / 12.92, pow((encoded + 0.055) / 1.055, vec3(2.4)),
               step(vec3(0.04045), encoded));
}

// The light reaching a surface facing `normal`: the sun, the moon, and the ambient light mixed
// from the ground's color below to the sky's above. With brightness only, its luminance in every
// channel.
vec3 incoming(vec3 normal) {
    vec3 n = normalize(normal);
    vec3 sum = light.sun.rgb * max(dot(n, light.sunDirection.xyz), 0.0) +
               light.moon.rgb * max(dot(n, light.moonDirection.xyz), 0.0) +
               mix(light.ambientGround.rgb, light.ambientSky.rgb, 0.5 * n.y + 0.5);
    if (light.settings.x > 0.5) {
        return vec3(dot(sum, vec3(0.2126, 0.7152, 0.0722)));
    }
    return sum;
}

vec3 fadeToSky(vec3 color, float viewDistance) {
    return mix(color, light.sky.rgb, smoothstep(250.0, 700.0, viewDistance));
}
