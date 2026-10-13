#ifndef TPJ_RENDER_PAVING_OVERLAY_H
#define TPJ_RENDER_PAVING_OVERLAY_H

#include "render/park_mesh.h"
#include "sim/entity_key.h"
#include "sim/medium/network.h"
#include "sim/park/intent.h"
#include "sim/world.h"

#include <array>
#include <optional>
#include <span>
#include <stdint.h>
#include <vector>

namespace tpj {

// The shading lies this far above the ground: over the guest paving and under its edge lines,
// which lie EDGE_LIFT above it.
inline constexpr float OVERLAY_HEIGHT = pathLift(PathKind::Guest) + 0.0025f;
// Viridis at quarters of its range, from dark purple, which 0, the worst value, takes, to yellow:
// perceptually uniform and colorblind-safe.
inline constexpr std::array<Rgba, 5> OVERLAY_RAMP{{{0.267f, 0.004f, 0.329f, 1.0f},
                                                   {0.231f, 0.322f, 0.545f, 1.0f},
                                                   {0.129f, 0.565f, 0.549f, 1.0f},
                                                   {0.365f, 0.784f, 0.388f, 1.0f},
                                                   {0.992f, 0.906f, 0.145f, 1.0f}}};

// A vertex of the overlay's pieces: where it lies, and the index of its piece in the shape.
struct OverlayVertex {
  float Position[3] = {};
  uint32_t Piece = 0;
};

// A piece of guest paving: its corners, counter-clockwise seen from above, and the ranges of its
// lines in the shape's LineRefs and of the pieces it overlaps in the shape's Overlaps.
struct OverlayPiece {
  std::array<ParkPoint, 4> Corners{};
  uint32_t Count = 0;
  uint32_t FirstLine = 0;
  uint32_t LineCount = 0;
  uint32_t FirstOverlap = 0;
  uint32_t OverlapCount = 0;
};

// A guest segment's line: its key and the range of its points in the shape's Points.
struct OverlayLine {
  EntityKey Key = NULL_KEY;
  uint32_t FirstPoint = 0;
  uint32_t PointCount = 0;
};

// The guest paving as the overlay draws it: the triangles of its pieces, the pieces, for each
// piece the indices of its lines in ascending order, the guest segments' lines in ascending key
// order with their points, and for each piece the indices of the pieces of other owners it
// overlaps.
struct OverlayShape {
  std::vector<OverlayVertex> Vertices;
  std::vector<uint32_t> Indices;
  std::vector<OverlayPiece> Pieces;
  std::vector<uint32_t> LineRefs;
  std::vector<OverlayLine> Lines;
  std::vector<CarrierPoint> Points;
  std::vector<uint32_t> Overlaps;
};

// The guest paving's overlay shape: each piece of the paved surface, without walkways, that
// belongs to a guest segment or to a point where guest segments meet, with its lines and overlaps.
OverlayShape buildOverlayShape(const World &world);

// The overlay place of a ground point: the nearest place on the lines of the pieces that hold it,
// ties to the lower segment key, or none when no piece holds it or it is not finite.
std::optional<Place> overlayPlace(const OverlayShape &shape, GroundPoint point);

// A value at a distance along a line.
struct OverlayKnot {
  double Distance = 0.0;
  double Value = 0.0;
};

// The values the overlay blends between: where each of the shape's lines' knots begin, with one
// more entry where the last line's end, the knots, and the value that shades halfway up the
// ramp.
struct OverlayShades {
  std::vector<uint32_t> FirstKnots;
  std::vector<OverlayKnot> Knots;
  double Half = 1.0;
};

// The shades of the values at the places: for each line of the shape, the places on it in
// ascending distance, with their values. Places on no line of the shape are left out.
OverlayShades shadeOverlay(const OverlayShape &shape, std::span<const Place> places,
                           std::span<const double> values, double half);

// The lists shading reuses: each place's line, and the next free knot of each line.
struct ShadeLists {
  std::vector<uint32_t> LineOfPlace;
  std::vector<uint32_t> Next;
};

// As the returning form, into shades and lists the caller keeps. Allocates nothing once they have
// grown.
void shadeOverlay(const OverlayShape &shape, std::span<const Place> places,
                  std::span<const double> values, double half, ShadeLists &lists,
                  OverlayShades &shades);

} // namespace tpj

#endif
