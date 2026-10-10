#include "render/day_light.h"

#include "sim/clock.h"
#include "sim/world.h"

#include <algorithm>
#include <array>
#include <math.h>
#include <numbers>
#include <stddef.h>

namespace tpj {
namespace {

// A color as authored, in sRGB.
struct Srgb {
  float R = 0.0f;
  float G = 0.0f;
  float B = 0.0f;
};

// The light authored at one second of the day. Colors are sRGB, and strengths scale them in
// linear light. The ambient light and the sky have no strength of their own.
struct DayKey {
  uint64_t Second = 0;
  Srgb SunColor;
  float SunStrength = 0.0f;
  float MoonStrength = 0.0f;
  Srgb AmbientSky;
  Srgb AmbientGround;
  Srgb Sky;
  float Exposure = 1.0f;
};

constexpr uint64_t hours(uint64_t h, uint64_t m = 0) { return h * 3600 + m * 60; }

constexpr DayKey NIGHT_KEY{0,
                           {1.00f, 0.60f, 0.40f},
                           0.0f,
                           0.10f,
                           {0.30f, 0.34f, 0.44f},
                           {0.12f, 0.12f, 0.16f},
                           {0.06f, 0.08f, 0.15f},
                           1.6f};

constexpr DayKey atSecond(DayKey key, uint64_t second) {
  key.Second = second;
  return key;
}

// The day's keyframes in order of the second, wrapping at midnight: night until dawn, a warm
// sunrise, a neutral midday, golden light from late afternoon, a warm sunset, and night from dusk.
constexpr std::array<DayKey, 8> DAY_KEYS{{
    NIGHT_KEY,
    atSecond(NIGHT_KEY, hours(5)),
    {hours(6),
     {1.00f, 0.60f, 0.38f},
     0.60f,
     0.04f,
     {0.52f, 0.50f, 0.58f},
     {0.30f, 0.26f, 0.24f},
     {0.80f, 0.64f, 0.60f},
     1.2f},
    {hours(8),
     {1.00f, 0.90f, 0.78f},
     0.75f,
     0.0f,
     {0.60f, 0.64f, 0.72f},
     {0.48f, 0.44f, 0.38f},
     {0.62f, 0.76f, 0.90f},
     1.0f},
    {hours(12),
     {1.00f, 0.97f, 0.92f},
     0.75f,
     0.0f,
     {0.62f, 0.66f, 0.74f},
     {0.50f, 0.46f, 0.40f},
     {0.62f, 0.76f, 0.90f},
     1.0f},
    {hours(17),
     {1.00f, 0.80f, 0.56f},
     0.75f,
     0.0f,
     {0.62f, 0.62f, 0.68f},
     {0.50f, 0.44f, 0.36f},
     {0.74f, 0.74f, 0.80f},
     1.0f},
    {hours(19, 30),
     {1.00f, 0.52f, 0.30f},
     0.60f,
     0.04f,
     {0.50f, 0.44f, 0.52f},
     {0.30f, 0.24f, 0.22f},
     {0.86f, 0.54f, 0.40f},
     1.2f},
    atSecond(NIGHT_KEY, hours(20, 30)),
}};

constexpr Srgb MOON_COLOR{0.62f, 0.72f, 1.00f};
// The sun's elevation at its highest, and the elevation below which its light fades out.
constexpr double SUN_PEAK_DEGREES = 60.0;
constexpr double TWILIGHT_DEGREES = 10.0;
// The moon stands still, high over the -X, -Z side.
constexpr double MOON_ELEVATION_DEGREES = 55.0;

constexpr double radians(double degrees) { return degrees * std::numbers::pi / 180.0; }

float linearOf(float encoded) {
  return encoded <= 0.04045f ? encoded / 12.92f : powf((encoded + 0.055f) / 1.055f, 2.4f);
}

Rgb linearOf(Srgb color) { return {linearOf(color.R), linearOf(color.G), linearOf(color.B)}; }

Rgb mix(Rgb a, Rgb b, float t) {
  return {a.R + (b.R - a.R) * t, a.G + (b.G - a.G) * t, a.B + (b.B - a.B) * t};
}

Rgb scaled(Rgb color, float strength) {
  return {color.R * strength, color.G * strength, color.B * strength};
}

float mix(float a, float b, float t) { return a + (b - a) * t; }

double smoothstep(double edge0, double edge1, double x) {
  const double t = std::clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
  return t * t * (3.0 - 2.0 * t);
}

// The direction toward a light at an elevation, with a heading on the ground of (cos, sin) of an
// angle in X and Z.
Vec3 directionAt(double elevation, double heading) {
  return {static_cast<float>(cos(elevation) * cos(heading)), static_cast<float>(sin(elevation)),
          static_cast<float>(cos(elevation) * sin(heading))};
}

} // namespace

DayLight dayLightAt(uint64_t second) {
  second %= DAY_SECONDS;
  size_t next = 0;
  while (next < DAY_KEYS.size() && DAY_KEYS[next].Second <= second) {
    ++next;
  }
  // After the last key, blend toward the first key of the next day.
  const DayKey &before = DAY_KEYS[next - 1];
  const DayKey &after = DAY_KEYS[next % DAY_KEYS.size()];
  const uint64_t afterSecond = next == DAY_KEYS.size() ? after.Second + DAY_SECONDS : after.Second;
  const auto t = static_cast<float>(static_cast<double>(second - before.Second) /
                                    static_cast<double>(afterSecond - before.Second));

  // The sun crosses from +X over +Z to -X between sunrise and sunset, and stands at the nearer end
  // outside them.
  const uint64_t inDay = std::clamp(second, SUNRISE_SECOND, SUNSET_SECOND);
  const double passed = static_cast<double>(inDay - SUNRISE_SECOND) /
                        static_cast<double>(SUNSET_SECOND - SUNRISE_SECOND);
  const double heading = std::numbers::pi * passed;
  const double elevation = radians(SUN_PEAK_DEGREES) * sin(heading);
  const bool isDay = second > SUNRISE_SECOND && second < SUNSET_SECOND;
  const double sunFade = isDay ? smoothstep(0.0, radians(TWILIGHT_DEGREES), elevation) : 0.0;

  DayLight light;
  light.SunDirection = directionAt(elevation, heading);
  light.Sun = scaled(mix(linearOf(before.SunColor), linearOf(after.SunColor), t),
                     mix(before.SunStrength, after.SunStrength, t) * static_cast<float>(sunFade));
  light.MoonDirection = directionAt(radians(MOON_ELEVATION_DEGREES), -0.75 * std::numbers::pi);
  light.Moon = scaled(linearOf(MOON_COLOR), mix(before.MoonStrength, after.MoonStrength, t));
  light.AmbientSky = mix(linearOf(before.AmbientSky), linearOf(after.AmbientSky), t);
  light.AmbientGround = mix(linearOf(before.AmbientGround), linearOf(after.AmbientGround), t);
  light.Sky = mix(linearOf(before.Sky), linearOf(after.Sky), t);
  light.Exposure = mix(before.Exposure, after.Exposure, t);
  return light;
}

DayLight dayLightOf(const World &world) { return dayLightAt(parkTime(world.Tick).Second); }

} // namespace tpj
