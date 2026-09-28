#ifndef TPJ_RENDER_GRAPH_OVERLAY_H
#define TPJ_RENDER_GRAPH_OVERLAY_H

#include "render/park_mesh.h"
#include "render/renderer.h"
#include "sim/entity_key.h"
#include "sim/medium/network.h"
#include "sim/park/intent.h"
#include "sim/world.h"

#include <optional>
#include <stdint.h>
#include <vector>

namespace tpj {

// The graph view's colors, opaque and distinct from each other and from the paths' ribbons.
inline constexpr Rgba GRAPH_NODE_COLOR{1.0f, 1.0f, 1.0f, 1.0f};

constexpr Rgba graphColor(PathKind kind) {
  return kind == PathKind::Guest ? Rgba{0.10f, 0.85f, 1.0f, 1.0f} : Rgba{1.0f, 0.30f, 0.80f, 1.0f};
}

// In window units.
inline constexpr float GRAPH_LINE_THICKNESS = 2.0f;
inline constexpr float GRAPH_NODE_RADIUS = 4.0f;

// A position in a window, from its top left corner.
struct WindowPoint {
  float X = 0.0f;
  float Y = 0.0f;
};

// A carrier segment, from the carrier's point at Segment to the next, as drawn: clipped to the
// near plane and projected.
struct GraphLine {
  PathKind Kind = PathKind::Guest;
  EntityKey Carrier = NULL_KEY;
  uint32_t Segment = 0;
  WindowPoint From;
  WindowPoint To;
};

struct GraphNode {
  PathKind Kind = PathKind::Guest;
  uint32_t Node = 0;
  WindowPoint At;
};

struct GraphOverlay {
  std::vector<GraphLine> Lines;
  std::vector<GraphNode> Nodes;
};

// Where drawFrame's projection puts the ground point in a window of the width and height. None
// when the point's view depth is not positive, or the width or height is not. See render/SPEC.md.
std::optional<WindowPoint> windowPoint(const CameraView &view, float width, float height,
                                       GroundPoint point);

// Both networks' segments and nodes in a window of the width and height, with segments clipped to
// the near plane. See render/SPEC.md.
GraphOverlay buildGraphOverlay(const World &world, const CameraView &view, float width,
                               float height);

} // namespace tpj

#endif
