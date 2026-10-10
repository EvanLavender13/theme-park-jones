// Turns linear light into the color shown on screen: exposed, tonemapped by Khronos PBR Neutral
// (https://github.com/KhronosGroup/ToneMapping, PBR_Neutral), and encoded to sRGB. PBR Neutral
// keeps the hue of every color and compresses only highlights.

vec3 pbrNeutral(vec3 color) {
    const float startCompression = 0.8 - 0.04;
    const float desaturation = 0.15;
    float x = min(color.r, min(color.g, color.b));
    float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
    color -= offset;
    float peak = max(color.r, max(color.g, color.b));
    if (peak < startCompression) {
        return color;
    }
    const float d = 1.0 - startCompression;
    float newPeak = 1.0 - d * d / (peak + d - startCompression);
    color *= newPeak / peak;
    float g = 1.0 - 1.0 / (desaturation * (peak - newPeak) + 1.0);
    return mix(color, vec3(newPeak), g);
}

vec3 encodedOf(vec3 linear) {
    vec3 clamped = clamp(linear, 0.0, 1.0);
    return mix(clamped * 12.92, 1.055 * pow(clamped, vec3(1.0 / 2.4)) - 0.055,
               step(vec3(0.0031308), clamped));
}

vec3 displayed(vec3 linear, float exposure) {
    return encodedOf(pbrNeutral(linear * exposure));
}
