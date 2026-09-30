#ifndef TPJ_RENDER_FOOD_OVERLAY_H
#define TPJ_RENDER_FOOD_OVERLAY_H

#include "render/park_mesh.h"
#include "sim/medium/network.h"
#include "sim/world.h"

#include <array>
#include <functional>

namespace tpj {

// The overlay shades the ground this far, in meters, either side of each guest path's line.
inline constexpr double OVERLAY_BAND = 6.0;
// The most distance, in meters, between the places the overlay samples along a line, besides its
// points and stops.
inline constexpr double OVERLAY_SPACING = 1.0;
// The band is a low tent, highest on its line and lowest at its edges, so where bands overlap the
// nearest line's lies on top. It lies over the terrain and under every path.
inline constexpr float OVERLAY_TOP_LIFT = 0.015f;
inline constexpr float OVERLAY_EDGE_LIFT = 0.005f;
// The availability at the ramp's top: a meal's full relief with no walk and no wait.
inline constexpr double OVERLAY_FULL = 0.5;
// Viridis at quarters of its range, from dark purple to yellow: perceptually uniform and
// colorblind-safe.
inline constexpr std::array<Rgba, 5> OVERLAY_RAMP{{{0.267f, 0.004f, 0.329f, 1.0f},
                                                   {0.231f, 0.322f, 0.545f, 1.0f},
                                                   {0.129f, 0.565f, 0.549f, 1.0f},
                                                   {0.365f, 0.784f, 0.388f, 1.0f},
                                                   {0.992f, 0.906f, 0.145f, 1.0f}}};
// No food at all, in a gray the ramp never gives.
inline constexpr Rgba OVERLAY_ZERO_COLOR{0.55f, 0.55f, 0.55f, 1.0f};

// The ramp's color for an availability, saturating at OVERLAY_FULL, or OVERLAY_ZERO_COLOR for a
// value not above 0.
Rgba foodColor(double value);

// The value the overlay shades a place by, such as the food availability there.
using OverlayValue = std::function<double(const Place &)>;

// The band along each guest path's line, in key order, shaded by the value at the line's places,
// with a cone beyond each end.
ParkMesh buildFoodOverlay(const World &world, const OverlayValue &value);

} // namespace tpj

#endif
