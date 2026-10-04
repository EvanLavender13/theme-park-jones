#ifndef TPJ_RENDER_BAND_OVERLAY_H
#define TPJ_RENDER_BAND_OVERLAY_H

#include "render/park_mesh.h"
#include "sim/medium/network.h"
#include "sim/world.h"

#include <array>
#include <functional>
#include <span>
#include <vector>

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
// Viridis at quarters of its range, from dark purple to yellow: perceptually uniform and
// colorblind-safe.
inline constexpr std::array<Rgba, 5> OVERLAY_RAMP{{{0.267f, 0.004f, 0.329f, 1.0f},
                                                   {0.231f, 0.322f, 0.545f, 1.0f},
                                                   {0.129f, 0.565f, 0.549f, 1.0f},
                                                   {0.365f, 0.784f, 0.388f, 1.0f},
                                                   {0.992f, 0.906f, 0.145f, 1.0f}}};
// No value at all, in a gray the ramp never gives.
inline constexpr Rgba OVERLAY_ZERO_COLOR{0.55f, 0.55f, 0.55f, 1.0f};

// The values an overlay shades its places by, one for each place and in their order, such as an
// availability's.
using OverlayValues = std::function<std::vector<double>(std::span<const Place> places)>;

// The band along each guest path's line, in key order, with a cone beyond each end, shaded by the
// values one call of values gives at the lines' places, the ramp reaching its top at full, which
// is positive, and the gray OVERLAY_ZERO_COLOR for a value not above 0.
ParkMesh buildBandOverlay(const World &world, const OverlayValues &values, double full);

} // namespace tpj

#endif
