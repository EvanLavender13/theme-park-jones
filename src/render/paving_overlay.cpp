#include "render/paving_overlay.h"

#include "sim/medium/network.h"
#include "sim/park/intent.h"
#include "sim/park/surface.h"
#include "sim/routes/networks.h"
#include "sim/tick_buffers.h"

#include <algorithm>
#include <math.h>
#include <optional>
#include <span>
#include <stddef.h>
#include <stdint.h>
#include <utility>
#include <vector>

namespace tpj {

namespace {

// How far, in meters, a point may lie outside a piece's outline and still be held by it, so a
// point on the outline is held whatever the rounding.
constexpr double HOLD_TOLERANCE = 1e-9;

// Positive when b lies counter-clockwise of a, seen from above, as sim/park's pieces wind.
double ccw(double ax, double az, double bx, double bz) { return az * bx - ax * bz; }

std::span<const ParkPoint> cornersOf(const OverlayPiece &piece) {
  return {piece.Corners.data(), piece.Count};
}

// Whether the piece holds the point, on or inside its outline.
bool holds(const OverlayPiece &piece, GroundPoint point) {
  const std::span<const ParkPoint> corners = cornersOf(piece);
  for (size_t i = 0; i < corners.size(); ++i) {
    const ParkPoint &a = corners[i];
    const ParkPoint &b = corners[(i + 1) % corners.size()];
    const double dx = b.X - a.X;
    const double dz = b.Z - a.Z;
    const double length = sqrt(dx * dx + dz * dz);
    if (length > 0.0 && ccw(dx, dz, point.X - a.X, point.Z - a.Z) < -HOLD_TOLERANCE * length) {
      return false;
    }
  }
  return true;
}

// The least and greatest of the piece's corners along the direction.
std::pair<double, double> extentAlong(const OverlayPiece &piece, double nx, double nz) {
  double least = INFINITY;
  double most = -INFINITY;
  for (const ParkPoint &corner : cornersOf(piece)) {
    const double along = corner.X * nx + corner.Z * nz;
    least = std::min(least, along);
    most = std::max(most, along);
  }
  return {least, most};
}

// Whether some edge of a separates the two pieces along its normal, touching included.
bool separatedByEdgeOf(const OverlayPiece &a, const OverlayPiece &b) {
  const std::span<const ParkPoint> corners = cornersOf(a);
  for (size_t i = 0; i < corners.size(); ++i) {
    const ParkPoint &from = corners[i];
    const ParkPoint &to = corners[(i + 1) % corners.size()];
    const double nx = to.Z - from.Z;
    const double nz = from.X - to.X;
    const auto [aLeast, aMost] = extentAlong(a, nx, nz);
    const auto [bLeast, bMost] = extentAlong(b, nx, nz);
    const double scale = sqrt(nx * nx + nz * nz);
    if (aMost <= bLeast + HOLD_TOLERANCE * scale || bMost <= aLeast + HOLD_TOLERANCE * scale) {
      return true;
    }
  }
  return false;
}

// Whether the two convex pieces share positive area.
bool overlap(const OverlayPiece &a, const OverlayPiece &b) {
  return !separatedByEdgeOf(a, b) && !separatedByEdgeOf(b, a);
}

// A piece's bounds across X and Z.
struct Bounds {
  double LeastX = INFINITY;
  double MostX = -INFINITY;
  double LeastZ = INFINITY;
  double MostZ = -INFINITY;
};

Bounds boundsOf(const OverlayPiece &piece) {
  Bounds bounds;
  for (const ParkPoint &corner : cornersOf(piece)) {
    bounds.LeastX = std::min(bounds.LeastX, corner.X);
    bounds.MostX = std::max(bounds.MostX, corner.X);
    bounds.LeastZ = std::min(bounds.LeastZ, corner.Z);
    bounds.MostZ = std::max(bounds.MostZ, corner.Z);
  }
  return bounds;
}

// The lists building the shape reuses: the guest segments' keys, each guest segment's ends as
// (point, segment), each piece's owner, and the search for overlaps' bounds, order, and pairs.
struct ShapeScratch {
  std::vector<EntityKey> Segments;
  std::vector<std::pair<EntityKey, EntityKey>> Ends;
  std::vector<EntityKey> Owners;
  std::vector<Bounds> PieceBounds;
  std::vector<uint32_t> Order;
  std::vector<std::pair<uint32_t, uint32_t>> Pairs;
};

// Fills each piece's overlaps: the pieces of other owners it shares positive area with, ascending.
void findOverlaps(OverlayShape &shape, const std::vector<EntityKey> &owners,
                  ShapeScratch &scratch) {
  const size_t count = shape.Pieces.size();
  std::vector<Bounds> &bounds = scratch.PieceBounds;
  std::vector<uint32_t> &order = scratch.Order;
  fillGrowing(bounds, count, Bounds{});
  fillGrowing(order, count, uint32_t{0});
  for (uint32_t i = 0; i < count; ++i) {
    bounds[i] = boundsOf(shape.Pieces[i]);
    order[i] = i;
  }
  std::ranges::sort(
      order, [&bounds](uint32_t a, uint32_t b) { return bounds[a].LeastX < bounds[b].LeastX; });
  std::vector<std::pair<uint32_t, uint32_t>> &pairs = scratch.Pairs;
  pairs.clear();
  for (size_t i = 0; i < count; ++i) {
    const uint32_t a = order[i];
    for (size_t j = i + 1; j < count && bounds[order[j]].LeastX < bounds[a].MostX; ++j) {
      const uint32_t b = order[j];
      if (owners[a] == owners[b] || bounds[b].LeastZ >= bounds[a].MostZ ||
          bounds[a].LeastZ >= bounds[b].MostZ || !overlap(shape.Pieces[a], shape.Pieces[b])) {
        continue;
      }
      pairs.emplace_back(a, b);
      pairs.emplace_back(b, a);
    }
  }
  std::ranges::sort(pairs);
  size_t next = 0;
  for (uint32_t i = 0; i < count; ++i) {
    OverlayPiece &piece = shape.Pieces[i];
    piece.FirstOverlap = static_cast<uint32_t>(shape.Overlaps.size());
    for (; next < pairs.size() && pairs[next].first == i; ++next) {
      shape.Overlaps.push_back(pairs[next].second);
    }
    piece.OverlapCount = static_cast<uint32_t>(shape.Overlaps.size()) - piece.FirstOverlap;
  }
}

// The index of the line with the key, or none.
std::optional<uint32_t> lineOf(const OverlayShape &shape, EntityKey key) {
  const auto at = std::ranges::lower_bound(shape.Lines, key, {}, &OverlayLine::Key);
  if (at == shape.Lines.end() || at->Key != key) {
    return std::nullopt;
  }
  return static_cast<uint32_t>(at - shape.Lines.begin());
}

// The nearest place on a line to a point, and its squared distance.
struct LinePlace {
  double Distance = 0.0;
  double Squared = INFINITY;
};

// The nearest place on the line's points, a part strictly nearer replacing the one before.
LinePlace nearestOnLine(const OverlayShape &shape, const OverlayLine &line, GroundPoint point) {
  const std::span<const CarrierPoint> points(shape.Points.data() + line.FirstPoint,
                                             line.PointCount);
  LinePlace nearest;
  for (size_t i = 0; i + 1 < points.size(); ++i) {
    const CarrierPoint &a = points[i];
    const CarrierPoint &b = points[i + 1];
    const double dx = b.X - a.X;
    const double dz = b.Z - a.Z;
    const double squaredLength = dx * dx + dz * dz;
    double t = 0.0;
    if (squaredLength > 0.0) {
      t = std::clamp(((point.X - a.X) * dx + (point.Z - a.Z) * dz) / squaredLength, 0.0, 1.0);
    }
    const double x = a.X + t * dx - point.X;
    const double z = a.Z + t * dz - point.Z;
    const double squared = x * x + z * z;
    if (squared < nearest.Squared) {
      nearest = {a.Distance + t * (b.Distance - a.Distance), squared};
    }
  }
  return nearest;
}

// Adds the guest segments' lines to the shape, and puts each guest segment's ends, as
// (point, segment), sorted, in scratch.Ends.
void addLines(const World &world, std::vector<SegmentRecord> &records, OverlayShape &shape,
              ShapeScratch &scratch) {
  std::vector<EntityKey> &segments = scratch.Segments;
  std::vector<std::pair<EntityKey, EntityKey>> &ends = scratch.Ends;
  segments.clear();
  ends.clear();
  segmentRecords(world, records);
  for (const SegmentRecord &segment : records) {
    if (segment.Kind == PathKind::Guest) {
      segments.push_back(segment.Key);
      ends.emplace_back(segment.From, segment.Key);
      ends.emplace_back(segment.To, segment.Key);
    }
  }
  std::ranges::sort(ends);
  for (const Carrier &carrier : parkNetwork(world, PathKind::Guest).carriers()) {
    if (!std::ranges::binary_search(segments, carrier.Key)) {
      continue;
    }
    shape.Lines.push_back({carrier.Key, static_cast<uint32_t>(shape.Points.size()),
                           static_cast<uint32_t>(carrier.Points.size())});
    shape.Points.insert(shape.Points.end(), carrier.Points.begin(), carrier.Points.end());
  }
}

// Adds the surface's piece to the shape with its lines, its owner's line when it is a segment and
// the lines of the segments ending at it when it is a point, and its triangles. Adds nothing and
// gives false when it has no line.
bool addPiece(OverlayShape &shape, const SurfacePiece &surfacePiece,
              const std::vector<std::pair<EntityKey, EntityKey>> &ends) {
  OverlayPiece piece;
  piece.Corners = surfacePiece.Corners;
  piece.Count = static_cast<uint32_t>(surfacePiece.Count);
  piece.FirstLine = static_cast<uint32_t>(shape.LineRefs.size());
  if (const std::optional<uint32_t> line = lineOf(shape, surfacePiece.Owner)) {
    shape.LineRefs.push_back(*line);
  } else {
    const auto [first, last] = std::ranges::equal_range(ends, surfacePiece.Owner, {},
                                                        &std::pair<EntityKey, EntityKey>::first);
    for (auto end = first; end != last; ++end) {
      if (const std::optional<uint32_t> at = lineOf(shape, end->second)) {
        shape.LineRefs.push_back(*at);
      }
    }
    std::sort(shape.LineRefs.begin() + piece.FirstLine, shape.LineRefs.end());
  }
  piece.LineCount = static_cast<uint32_t>(shape.LineRefs.size()) - piece.FirstLine;
  if (piece.LineCount == 0) {
    return false;
  }
  const auto index = static_cast<uint32_t>(shape.Pieces.size());
  const auto first = static_cast<uint32_t>(shape.Vertices.size());
  for (const ParkPoint &corner : cornersOf(piece)) {
    shape.Vertices.push_back(
        {{static_cast<float>(corner.X), OVERLAY_HEIGHT, static_cast<float>(corner.Z)}, index});
  }
  for (uint32_t k = 1; k + 1 < piece.Count; ++k) {
    shape.Indices.insert(shape.Indices.end(), {first, first + k, first + k + 1});
  }
  shape.Pieces.push_back(piece);
  return true;
}

} // namespace

void buildOverlayShape(const World &world, OverlayShapeLists &lists) {
  OverlayShape &shape = lists.Shape;
  shape.Vertices.clear();
  shape.Indices.clear();
  shape.Pieces.clear();
  shape.LineRefs.clear();
  shape.Lines.clear();
  shape.Points.clear();
  shape.Overlaps.clear();
  ShapeScratch &scratch = lists.Buffers.get<ShapeScratch>();
  addLines(world, lists.Segments, shape, scratch);
  std::vector<EntityKey> &owners = scratch.Owners;
  owners.clear();
  for (const auto &[owner, paving] : resolvedSurface(world).Owners) {
    for (const SurfacePiece &piece : paving.Pieces) {
      if (piece.Kind == PathKind::Guest && addPiece(shape, piece, scratch.Ends)) {
        owners.push_back(piece.Owner);
      }
    }
  }
  findOverlaps(shape, owners, scratch);
}

std::optional<Place> overlayPlace(const OverlayShape &shape, GroundPoint point,
                                  std::vector<uint32_t> &lines) {
  lines.clear();
  if (!isfinite(point.X) || !isfinite(point.Z)) {
    return std::nullopt;
  }
  for (const OverlayPiece &piece : shape.Pieces) {
    if (holds(piece, point)) {
      lines.insert(lines.end(), shape.LineRefs.begin() + piece.FirstLine,
                   shape.LineRefs.begin() + piece.FirstLine + piece.LineCount);
    }
  }
  std::ranges::sort(lines);
  std::optional<Place> nearest;
  double nearestSquared = INFINITY;
  // Lines in ascending index are in ascending key, and only a strictly nearer one replaces.
  for (const uint32_t index : lines) {
    const OverlayLine &line = shape.Lines[index];
    const LinePlace place = nearestOnLine(shape, line, point);
    if (place.Squared < nearestSquared) {
      nearestSquared = place.Squared;
      nearest = Place{line.Key, place.Distance};
    }
  }
  return nearest;
}

OverlayShades shadeOverlay(const OverlayShape &shape, std::span<const Place> places,
                           std::span<const double> values, double half) {
  ShadeLists lists;
  OverlayShades shades;
  shadeOverlay(shape, places, values, half, lists, shades);
  return shades;
}

void shadeOverlay(const OverlayShape &shape, std::span<const Place> places,
                  std::span<const double> values, double half, ShadeLists &lists,
                  OverlayShades &shades) {
  shades.Half = half;
  fillGrowing(shades.FirstKnots, shape.Lines.size() + 1, uint32_t{0});
  // Each place's line, or NO_LINE when it is on none of the shape's.
  constexpr uint32_t NO_LINE = UINT32_MAX;
  std::vector<uint32_t> &lineOfPlace = lists.LineOfPlace;
  fillGrowing(lineOfPlace, places.size(), NO_LINE);
  for (size_t i = 0; i < places.size(); ++i) {
    if (const std::optional<uint32_t> line = lineOf(shape, places[i].Carrier)) {
      lineOfPlace[i] = *line;
      ++shades.FirstKnots[*line + 1];
    }
  }
  for (size_t line = 0; line < shape.Lines.size(); ++line) {
    shades.FirstKnots[line + 1] += shades.FirstKnots[line];
  }
  fillGrowing(shades.Knots, shades.FirstKnots.back(), OverlayKnot{});
  std::vector<uint32_t> &next = lists.Next;
  copyGrowing(next, std::span<const uint32_t>(shades.FirstKnots).first(shape.Lines.size()));
  for (size_t i = 0; i < places.size(); ++i) {
    if (lineOfPlace[i] != NO_LINE) {
      shades.Knots[next[lineOfPlace[i]]++] = {places[i].Distance, values[i]};
    }
  }
  // An insertion sort by distance on each line: stable, as equal distances keep the places' order,
  // and it allocates nothing. Places that come in order take one pass.
  for (size_t line = 0; line < shape.Lines.size(); ++line) {
    const auto first = shades.Knots.begin() + shades.FirstKnots[line];
    const auto last = shades.Knots.begin() + shades.FirstKnots[line + 1];
    for (auto at = first; at != last; ++at) {
      const OverlayKnot knot = *at;
      auto hole = at;
      while (hole != first && knot.Distance < (hole - 1)->Distance) {
        *hole = *(hole - 1);
        --hole;
      }
      *hole = knot;
    }
  }
}

} // namespace tpj
