#version 450
#extension GL_GOOGLE_include_directive : require

#include "light.glsl"
#include "tonemap.glsl"

// Colors each pixel of the paving overlay's pieces by the value at its overlay place: the nearest
// place on its piece's lines, unless a piece of another owner that holds the pixel has a nearer
// one, or one as near on a lower line, which then draws the pixel instead.

layout(location = 0) in vec2 inGround;
layout(location = 1) flat in uint inPiece;
layout(location = 2) in float inViewDistance;

layout(location = 0) out vec4 outColor;

// SDL_GPU binds fragment-stage storage buffers at set 2 and uniform buffers at set 3. Both buffers
// are 32-bit words, as setOverlayShape and setOverlayShades pack them.
layout(std430, set = 2, binding = 0) readonly buffer Shape {
    uint shape[];
};
layout(std430, set = 2, binding = 1) readonly buffer Shades {
    uint shades[];
};
layout(std140, set = 3, binding = 1) uniform Ramp {
    vec4 colors[5];
    vec4 middle;
} ramp;

// The words of one piece: eight corner coordinates, the corner count, and the first and count of
// its lines and of its overlaps.
const uint PIECE_WORDS = 13u;
// How far inside a piece's outline, in meters, a pixel must lie to count as held by another piece.
// Near that piece's outline the rasterizer may not draw it there, so the pixel is kept here and
// never left bare.
const float HOLD_MARGIN = 1e-4;

float word(uint index) { return uintBitsToFloat(shape[index]); }

uint pieceBase(uint piece) { return shape[0] + piece * PIECE_WORDS; }

bool holds(uint piece, vec2 point) {
    uint base = pieceBase(piece);
    uint count = shape[base + 8u];
    for (uint i = 0u; i < count; ++i) {
        uint j = (i + 1u) % count;
        vec2 a = vec2(word(base + 2u * i), word(base + 2u * i + 1u));
        vec2 b = vec2(word(base + 2u * j), word(base + 2u * j + 1u));
        vec2 edge = b - a;
        vec2 toPoint = point - a;
        if (edge.y * toPoint.x - edge.x * toPoint.y < HOLD_MARGIN * length(edge)) {
            return false;
        }
    }
    return true;
}

// The nearest place on the piece's lines: its line's index, its distance along the line, and its
// squared distance from the point. A strictly nearer place replaces the one before, lines in
// ascending index.
void nearestOn(uint piece, vec2 point, out uint bestLine, out float bestAlong,
               out float bestSquared) {
    uint base = pieceBase(piece);
    uint firstLine = shape[base + 9u];
    uint lineCount = shape[base + 10u];
    bestLine = 0xffffffffu;
    bestAlong = 0.0;
    bestSquared = 1.0 / 0.0;
    for (uint k = 0u; k < lineCount; ++k) {
        uint line = shape[shape[1] + firstLine + k];
        uint firstPoint = shape[shape[2] + 2u * line];
        uint pointCount = shape[shape[2] + 2u * line + 1u];
        for (uint i = 0u; i + 1u < pointCount; ++i) {
            uint at = shape[3] + 3u * (firstPoint + i);
            vec2 a = vec2(word(at), word(at + 1u));
            vec2 b = vec2(word(at + 3u), word(at + 4u));
            vec2 step = b - a;
            float squaredLength = dot(step, step);
            float t = squaredLength > 0.0 ? clamp(dot(point - a, step) / squaredLength, 0.0, 1.0)
                                          : 0.0;
            vec2 offset = a + t * step - point;
            float squared = dot(offset, offset);
            if (squared < bestSquared) {
                bestSquared = squared;
                bestLine = line;
                bestAlong = mix(word(at + 2u), word(at + 5u), t);
            }
        }
    }
}

float shadeWord(uint index) { return uintBitsToFloat(shades[index]); }

// The value at a distance along a line, blended between the knots around it.
float valueAt(uint line, float along) {
    uint first = shades[1u + line];
    uint end = shades[2u + line];
    if (first == end) {
        return 0.0;
    }
    uint knots = shades[0];
    uint low = first;
    uint high = end - 1u;
    if (along <= shadeWord(knots + 2u * low)) {
        return shadeWord(knots + 2u * low + 1u);
    }
    if (along >= shadeWord(knots + 2u * high)) {
        return shadeWord(knots + 2u * high + 1u);
    }
    // The last knot at or before the distance, and the one after it.
    while (high - low > 1u) {
        uint middle = (low + high) / 2u;
        if (shadeWord(knots + 2u * middle) <= along) {
            low = middle;
        } else {
            high = middle;
        }
    }
    float from = shadeWord(knots + 2u * low);
    float to = shadeWord(knots + 2u * high);
    float t = to > from ? (along - from) / (to - from) : 0.0;
    return mix(shadeWord(knots + 2u * low + 1u), shadeWord(knots + 2u * high + 1u), t);
}

vec3 rampColor(float value) {
    // 0, the worst value, and anything not above it take the ramp's bottom.
    float shown = value > 0.0 ? value : 0.0;
    // A value v shades v / (v + middle) of the way up the ramp, so no value clips at the top.
    float scaled = shown / (shown + ramp.middle.x) * 4.0;
    uint index = min(uint(scaled), 3u);
    return mix(ramp.colors[index].rgb, ramp.colors[index + 1u].rgb, scaled - float(index));
}

void main() {
    uint line;
    float along;
    float squared;
    nearestOn(inPiece, inGround, line, along, squared);
    uint base = pieceBase(inPiece);
    uint firstOverlap = shape[base + 11u];
    uint overlapCount = shape[base + 12u];
    for (uint k = 0u; k < overlapCount; ++k) {
        uint other = shape[shape[4] + firstOverlap + k];
        if (!holds(other, inGround)) {
            continue;
        }
        uint otherLine;
        float otherAlong;
        float otherSquared;
        nearestOn(other, inGround, otherLine, otherAlong, otherSquared);
        if (otherSquared < squared || (otherSquared == squared && otherLine < line)) {
            discard;
        }
    }
    // The ramp's own colors at every hour, faded with distance to the sky as it shows on screen.
    vec3 sky = displayed(light.sky.rgb, light.settings.y);
    vec3 color = mix(rampColor(valueAt(line, along)), sky, smoothstep(250.0, 700.0, inViewDistance));
    outColor = vec4(color, 1.0);
}
