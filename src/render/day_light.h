#ifndef TPJ_RENDER_DAY_LIGHT_H
#define TPJ_RENDER_DAY_LIGHT_H

#include "render/math.h"

#include <stdint.h>

namespace tpj {

struct World;

// A color in linear light.
struct Rgb {
  float R = 0.0f;
  float G = 0.0f;
  float B = 0.0f;
};

// The light at one moment of the day. The directions point toward their lights, and the colors are
// linear and include their strengths.
struct DayLight {
  Vec3 SunDirection;
  Rgb Sun;
  Vec3 MoonDirection;
  Rgb Moon;
  Rgb AmbientSky;
  Rgb AmbientGround;
  Rgb Sky;
  float Exposure = 1.0f;
};

// The second of the day the sun rises at: 6:00.
inline constexpr uint64_t SUNRISE_SECOND = 6ULL * 3600;
// The second of the day the sun sets at: 19:30.
inline constexpr uint64_t SUNSET_SECOND = 19ULL * 3600 + 1800;

// The light at a second of the day, from 0 at midnight.
DayLight dayLightAt(uint64_t second);
// The light at the world's time of day.
DayLight dayLightOf(const World &world);

} // namespace tpj

#endif
