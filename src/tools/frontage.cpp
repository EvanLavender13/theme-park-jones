#include "tools/tools.h"

#include "sim/park/geometry.h"
#include "sim/park/intent.h"
#include "tools/internal/snapping.h"

#include <algorithm>
#include <math.h>
#include <stddef.h>
#include <vector>

namespace tpj {
namespace {

// The place on a ground line nearest a point: where it lies, its distance from the point, the unit
// direction of the piece it lies on, and whether it is the line's first or last point.
struct LinePlace {
  ParkPoint At;
  double Distance = 0.0;
  ParkPoint Along;
  bool First = false;
  bool Last = false;
};

// The nearest place on the line, which has at least two points, or none when every piece has zero
// length. The earlier piece wins a tie.
std::optional<LinePlace> nearestOnLine(const std::vector<CarrierPoint> &line, ParkPoint point) {
  std::optional<LinePlace> nearest;
  for (size_t i = 0; i + 1 < line.size(); ++i) {
    const double dx = line[i + 1].X - line[i].X;
    const double dz = line[i + 1].Z - line[i].Z;
    const double length = sqrt(dx * dx + dz * dz);
    if (!(length > 0.0)) {
      continue;
    }
    const double t = std::clamp(
        ((point.X - line[i].X) * dx + (point.Z - line[i].Z) * dz) / (length * length), 0.0, 1.0);
    // A place at a piece's end is that end exactly, so it can be compared with the line's ends.
    const ParkPoint at = t == 1.0 ? ParkPoint{line[i + 1].X, line[i + 1].Z}
                                  : ParkPoint{line[i].X + t * dx, line[i].Z + t * dz};
    const double distance = distanceBetween(point, at);
    if (!nearest || distance < nearest->Distance) {
      nearest = LinePlace{at, distance, {dx / length, dz / length}, false, false};
    }
  }
  if (nearest) {
    nearest->First = nearest->At == ParkPoint{line.front().X, line.front().Z};
    nearest->Last = nearest->At == ParkPoint{line.back().X, line.back().Z};
  }
  return nearest;
}

} // namespace

std::optional<Pose> frontagePose(const World &world, BoxKind kind, ParkPoint point) {
  const PathKind served = kind == BoxKind::Shop ? PathKind::Guest : PathKind::Backstage;
  const double depth = boxSize(kind).Depth;
  const std::vector<ParkCorner> corners = parkCorners(world);
  std::optional<LinePlace> best;
  double bestEdge = 0.0;
  double bestHalfWidth = 0.0;
  for (const ParkSegment &segment : parkSegments(world)) {
    if (segment.Kind != served || segment.Line.size() < 2) {
      continue;
    }
    const std::optional<LinePlace> place = nearestOnLine(segment.Line, point);
    if (!place || (place->First && roundedAt(corners, segment.From) == nullptr) ||
        (place->Last && roundedAt(corners, segment.To) == nullptr)) {
      continue;
    }
    const double halfWidth = 0.5 * segment.Width;
    if (place->Distance > halfWidth + FRONTAGE_SETBACK + depth) {
      continue;
    }
    const double edge = place->Distance - halfWidth;
    if (!best || edge < bestEdge) {
      best = place;
      bestEdge = edge;
      bestHalfWidth = halfWidth;
    }
  }
  if (!best) {
    return std::nullopt;
  }
  ParkPoint out{-best->Along.Z, best->Along.X};
  if (best->Distance > MIN_SEGMENT_LENGTH) {
    out = {(point.X - best->At.X) / best->Distance, (point.Z - best->At.Z) / best->Distance};
  }
  const double reach = bestHalfWidth + FRONTAGE_SETBACK + 0.5 * depth;
  return Pose{best->At.X + out.X * reach, best->At.Z + out.Z * reach, -out.X, -out.Z};
}

} // namespace tpj
