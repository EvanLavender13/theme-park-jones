#include "tools/tools.h"

#include "sim/park/edits.h"
#include "sim/park/geometry.h"
#include "sim/park/intent.h"
#include "sim/park/surface.h"

#include <algorithm>
#include <array>
#include <math.h>
#include <span>
#include <stddef.h>
#include <utility>
#include <vector>

namespace tpj {
namespace {

ParkPoint plus(ParkPoint a, ParkPoint b) { return {a.X + b.X, a.Z + b.Z}; }
ParkPoint minus(ParkPoint a, ParkPoint b) { return {a.X - b.X, a.Z - b.Z}; }
ParkPoint scaled(ParkPoint a, double by) { return {a.X * by, a.Z * by}; }
double dot(ParkPoint a, ParkPoint b) { return a.X * b.X + a.Z * b.Z; }
double cross(ParkPoint a, ParkPoint b) { return a.X * b.Z - a.Z * b.X; }
double lengthOf(ParkPoint a) { return sqrt(dot(a, a)); }

// Two places this near each other are one corner of the edge lines, in meters.
constexpr double SAME_PLACE = 1e-6;
// How far below 0 the dot product of two directions a quarter turn apart may fall by rounding.
constexpr double QUARTER_TURN_SLACK = 1e-9;

// A step of an edge line: where it starts, its unit direction, its length, and the unit direction
// away from the paving, which lies on its left.
struct EdgeStep {
  ParkPoint From;
  ParkPoint Along;
  double Length = 0.0;
  ParkPoint Out;
};

// The steps of the surface's edge lines of the kind, in the surface's order, leaving out each
// shorter than MIN_SEGMENT_LENGTH.
std::vector<EdgeStep> edgeSteps(const PavedSurface &surface, PathKind kind) {
  std::vector<EdgeStep> steps;
  for (const EdgeLine &edge : surface.Edges) {
    if (edge.Kind != kind) {
      continue;
    }
    const std::span<const ParkPoint> points = edgePoints(surface, edge);
    for (size_t i = 0; i + 1 < points.size(); ++i) {
      const ParkPoint way = minus(points[i + 1], points[i]);
      const double length = lengthOf(way);
      if (length < MIN_SEGMENT_LENGTH) {
        continue;
      }
      const ParkPoint along = scaled(way, 1.0 / length);
      steps.push_back({points[i], along, length, {-along.Z, along.X}});
    }
  }
  return steps;
}

// The place on the step nearest the point.
ParkPoint nearestOn(const EdgeStep &step, ParkPoint point) {
  const double t = std::clamp(dot(minus(point, step.From), step.Along), 0.0, step.Length);
  return plus(step.From, scaled(step.Along, t));
}

// The pose of a box of the size that fronts the place, facing against the outward direction.
Pose poseFronting(ParkPoint at, ParkPoint out, FootprintSize size) {
  const ParkPoint center = plus(at, scaled(out, FRONTAGE_SETBACK + 0.5 * size.Depth));
  return Pose{center.X, center.Z, -out.X, -out.Z};
}

// A rectangle on the ground, its sides along the axes.
struct Bounds {
  double MinX = 0.0;
  double MinZ = 0.0;
  double MaxX = 0.0;
  double MaxZ = 0.0;

  bool overlaps(const Bounds &other) const {
    return MinX <= other.MaxX && other.MinX <= MaxX && MinZ <= other.MaxZ && other.MinZ <= MaxZ;
  }
};

template <typename Points> Bounds boundsOf(const Points &points) {
  Bounds bounds{points[0].X, points[0].Z, points[0].X, points[0].Z};
  for (const ParkPoint &at : points) {
    bounds.MinX = std::min(bounds.MinX, at.X);
    bounds.MinZ = std::min(bounds.MinZ, at.Z);
    bounds.MaxX = std::max(bounds.MaxX, at.X);
    bounds.MaxZ = std::max(bounds.MaxZ, at.Z);
  }
  return bounds;
}

// A piece of paving and the rectangle that bounds it.
struct NearPiece {
  const SurfacePiece *Piece = nullptr;
  Bounds Around;
};

// The least and greatest projections of the corners onto the axis.
template <typename Points>
std::pair<double, double> extentOf(const Points &points, ParkPoint axis) {
  double least = dot(points[0], axis);
  double greatest = least;
  for (const ParkPoint &at : points) {
    least = std::min(least, dot(at, axis));
    greatest = std::max(greatest, dot(at, axis));
  }
  return {least, greatest};
}

// Whether the rectangle, with its corners in order and its two directions, overlaps the piece by
// more than CONTACT_TOLERANCE, by the separating-axis test the world uses.
bool overlapsPiece(const std::array<ParkPoint, 4> &rect, ParkPoint forward, ParkPoint right,
                   std::span<const ParkPoint> corners) {
  const auto overlapOn = [&corners, &rect](ParkPoint axis) {
    const auto [pieceLeast, pieceGreatest] = extentOf(corners, axis);
    const auto [rectLeast, rectGreatest] = extentOf(rect, axis);
    return std::min(pieceGreatest, rectGreatest) - std::max(pieceLeast, rectLeast) >
           CONTACT_TOLERANCE;
  };
  if (!overlapOn(forward) || !overlapOn(right)) {
    return false;
  }
  for (size_t i = 0; i < corners.size(); ++i) {
    const ParkPoint side = minus(corners[(i + 1) % corners.size()], corners[i]);
    const double length = lengthOf(side);
    if (length >= 1e-9 && !overlapOn(ParkPoint{-side.Z / length, side.X / length})) {
      return false;
    }
  }
  return true;
}

// The paving near where a box is placed, and the kind its front door serves.
struct Room {
  std::vector<NearPiece> Pieces;
  PathKind Served = PathKind::Guest;
};

// Whether the pose's box overlaps none of the pieces, every part of its front face lies at least
// FRONTAGE_SETBACK from all of them, within CONTACT_TOLERANCE, and its front door lies within
// CONNECTION_REACH of paving of the kind it serves, so it gets a connector.
bool fits(const Pose &pose, FootprintSize size, const Room &room) {
  const std::optional<Footprint> footprint = footprintOf(pose, size);
  if (!footprint) {
    return false;
  }
  const std::array<ParkPoint, 4> &box = footprint->Corners;
  const ParkPoint ahead = scaled(footprint->Forward, FRONTAGE_SETBACK);
  const std::array<ParkPoint, 4> strip{plus(box[0], ahead), plus(box[1], ahead), box[1], box[0]};
  const ParkPoint aside = scaled(footprint->Right, FRONTAGE_SETBACK);
  const Bounds around = boundsOf(std::array<ParkPoint, 4>{
      minus(strip[0], aside), plus(strip[1], aside), plus(box[2], aside), minus(box[3], aside)});
  const ParkPoint door = scaled(plus(box[0], box[1]), 0.5);
  const bool reaches = std::ranges::any_of(room.Pieces, [&](const NearPiece &near) {
    return near.Piece->Kind == room.Served && pieceDistance(*near.Piece, door) <= CONNECTION_REACH;
  });
  return reaches && std::ranges::all_of(room.Pieces, [&](const NearPiece &near) {
           if (!near.Around.overlaps(around)) {
             return true;
           }
           const std::span<const ParkPoint> corners = pieceCorners(*near.Piece);
           return !overlapsPiece(box, footprint->Forward, footprint->Right, corners) &&
                  !overlapsPiece(strip, footprint->Forward, footprint->Right, corners) &&
                  pieceDistance(*near.Piece, box[0]) >= FRONTAGE_SETBACK - CONTACT_TOLERANCE &&
                  pieceDistance(*near.Piece, box[1]) >= FRONTAGE_SETBACK - CONTACT_TOLERANCE;
         });
}

// The pose fronting the place, moved straight out along the outward direction by the least push up
// to FRONTAGE_PUSH that fits, or none.
std::optional<Pose> pushedToFit(ParkPoint at, ParkPoint out, FootprintSize size, const Room &room) {
  const auto pushed = [&](double push) {
    return poseFronting(plus(at, scaled(out, push)), out, size);
  };
  double before = 0.0;
  for (int n = 0;; ++n) {
    const double push = std::min(static_cast<double>(n) * FRONTAGE_STEP, FRONTAGE_PUSH);
    if (fits(pushed(push), size, room)) {
      if (n == 0) {
        return pushed(0.0);
      }
      // Find the least push that fits, between the last that didn't and this one.
      double low = before;
      double high = push;
      for (int k = 0; k < 10; ++k) {
        const double middle = 0.5 * (low + high);
        if (fits(pushed(middle), size, room)) {
          high = middle;
        } else {
          low = middle;
        }
      }
      return pushed(high);
    }
    if (push >= FRONTAGE_PUSH) {
      return std::nullopt;
    }
    before = push;
  }
}

// The pose fronting the place, moved along the edge, square to the outward direction, by the least
// distance up to the box's width that fits, either way, or none.
std::optional<Pose> slidToFit(ParkPoint at, ParkPoint out, FootprintSize size, const Room &room) {
  const ParkPoint along{out.Z, -out.X};
  const auto slid = [&](double by) { return poseFronting(plus(at, scaled(along, by)), out, size); };
  std::optional<double> least;
  for (const double sense : {1.0, -1.0}) {
    for (int n = 1; static_cast<double>(n - 1) * FRONTAGE_STEP < size.Width; ++n) {
      const double by = std::min(static_cast<double>(n) * FRONTAGE_STEP, size.Width);
      if (!fits(slid(sense * by), size, room)) {
        continue;
      }
      double low = by - FRONTAGE_STEP;
      double high = by;
      for (int k = 0; k < 10; ++k) {
        const double middle = 0.5 * (low + high);
        if (fits(slid(sense * middle), size, room)) {
          high = middle;
        } else {
          low = middle;
        }
      }
      if (!least || high < std::abs(*least)) {
        least = sense * high;
      }
      break;
    }
  }
  if (!least) {
    return std::nullopt;
  }
  return slid(*least);
}

// A place a box might front: where it lies, the outward direction there, its distance from the
// pointer, and the order it was found in.
struct Candidate {
  ParkPoint At;
  ParkPoint Out;
  double Distance = 0.0;
  size_t Order = 0;
  size_t Step = 0;
};

// The pieces any place within reach of the place could put a box of the size over.
std::vector<NearPiece> piecesNear(const PavedSurface &surface, ParkPoint at, double reach,
                                  FootprintSize size) {
  const double diagonal =
      std::hypot(size.Width + 2.0 * FRONTAGE_SETBACK, size.Depth + FRONTAGE_SETBACK);
  const double grow = reach + FRONTAGE_PUSH + diagonal;
  const Bounds searched{at.X - grow, at.Z - grow, at.X + grow, at.Z + grow};
  std::vector<NearPiece> near;
  for (const SurfacePiece &piece : surface.Pieces) {
    const std::span<const ParkPoint> corners = pieceCorners(piece);
    if (corners.empty()) {
      continue;
    }
    const Bounds around = boundsOf(corners);
    if (around.overlaps(searched)) {
      near.push_back({&piece, around});
    }
  }
  return near;
}

// Places every FRONTAGE_STEP along the steps within reach of the first place, on steps whose
// outward direction lies within a quarter turn of its own, so the box never jumps across a path,
// nearest the pointer first.
std::vector<Candidate> candidatesNear(const std::vector<EdgeStep> &steps, ParkPoint at,
                                      ParkPoint out, double reach, ParkPoint point) {
  std::vector<Candidate> candidates;
  for (size_t index = 0; index < steps.size(); ++index) {
    const EdgeStep &step = steps[index];
    if (dot(step.Out, out) < -QUARTER_TURN_SLACK) {
      continue;
    }
    const ParkPoint offset = minus(at, step.From);
    const double middle = dot(offset, step.Along);
    const double aside = cross(step.Along, offset);
    if (std::abs(aside) > reach) {
      continue;
    }
    const double half = sqrt(reach * reach - aside * aside);
    const double from = std::max(0.0, middle - half);
    const double to = std::min(step.Length, middle + half);
    const auto add = [&](double along) {
      const ParkPoint place = plus(step.From, scaled(step.Along, along));
      if (lengthOf(minus(place, at)) <= reach) {
        candidates.push_back(
            {place, step.Out, lengthOf(minus(point, place)), candidates.size(), index});
      }
    };
    if (from == 0.0) {
      add(0.0);
    }
    for (auto n = static_cast<long long>(ceil(from / FRONTAGE_STEP));
         static_cast<double>(n) * FRONTAGE_STEP < to; ++n) {
      if (n > 0) {
        add(static_cast<double>(n) * FRONTAGE_STEP);
      }
    }
    if (to == step.Length) {
      add(step.Length);
    }
  }
  std::ranges::sort(candidates, [](const Candidate &a, const Candidate &b) {
    return a.Distance != b.Distance ? a.Distance < b.Distance : a.Order < b.Order;
  });
  return candidates;
}

// The place on the steps nearest the point, its distance, and the index of its step. The first
// step found wins a tie.
struct NearestPlace {
  ParkPoint At;
  double Distance = 0.0;
  size_t Step = 0;
};

NearestPlace nearestPlace(const std::vector<EdgeStep> &steps, ParkPoint point) {
  NearestPlace nearest{nearestOn(steps.front(), point), 0.0, 0};
  nearest.Distance = lengthOf(minus(point, nearest.At));
  for (size_t i = 0; i < steps.size(); ++i) {
    const ParkPoint on = nearestOn(steps[i], point);
    const double from = lengthOf(minus(point, on));
    if (from < nearest.Distance) {
      nearest = {on, from, i};
    }
  }
  return nearest;
}

ParkPoint endOf(const EdgeStep &step) { return plus(step.From, scaled(step.Along, step.Length)); }

ParkPoint unitOf(ParkPoint a) { return scaled(a, 1.0 / lengthOf(a)); }

// Whether two steps' outward directions lie within FRONTAGE_CORNER_TURN of each other, so a corner
// between them is part of one smooth curve.
bool gentle(const EdgeStep &a, const EdgeStep &b) {
  return acos(std::clamp(dot(a.Out, b.Out), -1.0, 1.0)) <= FRONTAGE_CORNER_TURN;
}

// Whether the corner from the earlier step to the later turns away from the paving, as along the
// inside of a curve.
bool inside(const EdgeStep &earlier, const EdgeStep &later) {
  return dot(later.Along, earlier.Out) > 0.0;
}

// The step that continues the step at its end, or with backward at its start, across a gentle
// corner on the inside of a curve, or none.
std::optional<size_t> continuing(const std::vector<EdgeStep> &steps, size_t step, bool backward) {
  const ParkPoint corner = backward ? steps[step].From : endOf(steps[step]);
  for (size_t i = 0; i < steps.size(); ++i) {
    const ParkPoint meets = backward ? endOf(steps[i]) : steps[i].From;
    if (i != step && lengthOf(minus(meets, corner)) <= SAME_PLACE &&
        gentle(steps[step], steps[i]) &&
        (backward ? inside(steps[i], steps[step]) : inside(steps[step], steps[i]))) {
      return i;
    }
  }
  return std::nullopt;
}

// Where a box fronts the edge lines for the pointer, and the direction it faces away from.
struct Fronting {
  ParkPoint At;
  ParkPoint Out;
};

// Steps whose nearest place lies within this much farther from the pointer than the nearest one
// count toward where a box fronts a curve, in meters.
constexpr double BLEND_REACH = 1.0;

// Where a box fronts the edge lines for the pointer. At a corner where several steps meet, it
// faces the pointer around a gentle corner on the outside of a curve when the pointer lies between
// the two sides' outward directions, and otherwise starts from the side the pointer lies more in
// front of, the first on a tie. Along the inside of a curve, each step within BLEND_REACH of the
// nearest distance counts, weighted down to nothing at that reach, so the place and the facing
// change smoothly there too.
Fronting frontingFor(const std::vector<EdgeStep> &steps, const NearestPlace &nearest,
                     ParkPoint point) {
  size_t start = nearest.Step;
  const ParkPoint toward = minus(point, nearest.At);
  if (lengthOf(toward) > MIN_SEGMENT_LENGTH) {
    const ParkPoint u = unitOf(toward);
    for (size_t i = 0; i < steps.size(); ++i) {
      if (i == start || lengthOf(minus(nearestOn(steps[i], nearest.At), nearest.At)) > SAME_PLACE) {
        continue;
      }
      const EdgeStep &a = steps[start];
      const EdgeStep &b = steps[i];
      const double sense = cross(a.Out, b.Out);
      const bool between = cross(a.Out, u) * sense >= 0.0 && cross(u, b.Out) * sense >= 0.0 &&
                           dot(u, a.Out) > 0.0 && dot(u, b.Out) > 0.0;
      const bool outside =
          lengthOf(minus(endOf(a), b.From)) <= SAME_PLACE ? !inside(a, b) : !inside(b, a);
      if (gentle(a, b) && outside) {
        if (between) {
          return {nearest.At, u};
        }
        return {nearest.At, dot(b.Out, u) > dot(a.Out, u) ? b.Out : a.Out};
      }
      if (!gentle(a, b) && dot(b.Out, u) > dot(a.Out, u)) {
        start = i;
      }
    }
  }
  ParkPoint place{0.0, 0.0};
  ParkPoint out{0.0, 0.0};
  double total = 0.0;
  const auto add = [&](size_t i) {
    const ParkPoint on = nearestOn(steps[i], point);
    const double weight = 1.0 - (lengthOf(minus(point, on)) - nearest.Distance) / BLEND_REACH;
    if (!(weight > 0.0)) {
      return false;
    }
    place = plus(place, scaled(on, weight));
    out = plus(out, scaled(steps[i].Out, weight));
    total += weight;
    return true;
  };
  add(start);
  for (const bool backward : {false, true}) {
    size_t at = start;
    for (size_t walked = 0; walked < steps.size(); ++walked) {
      const std::optional<size_t> next = continuing(steps, at, backward);
      if (!next || *next == start || !add(*next)) {
        break;
      }
      at = *next;
    }
  }
  if (!(total > 0.0) || !(lengthOf(out) > 0.0)) {
    return {nearest.At, steps[start].Out};
  }
  return {scaled(place, 1.0 / total), unitOf(out)};
}

} // namespace

std::optional<Pose> frontagePose(const World &world, BoxKind kind, ParkPoint point) {
  const PathKind served = kind == BoxKind::Shop ? PathKind::Guest : PathKind::Backstage;
  const FootprintSize size = boxSize(kind);
  const PavedSurface surface = parkSurface(world);
  const std::vector<EdgeStep> steps = edgeSteps(surface, served);
  if (steps.empty()) {
    return std::nullopt;
  }
  const NearestPlace nearest = nearestPlace(steps, point);
  const bool onPaving = std::ranges::any_of(surface.Pieces, [served, point](const SurfacePiece &p) {
    return p.Kind == served && pieceDistance(p, point) == 0.0;
  });
  if (!onPaving && nearest.Distance > FRONTAGE_SETBACK + size.Depth) {
    return std::nullopt;
  }
  const Fronting fronting = frontingFor(steps, nearest, point);
  const ParkPoint out = fronting.Out;
  const double reach = 2.0 * size.Width;
  const Room near{piecesNear(surface, fronting.At, reach, size), served};
  const Pose first = poseFronting(fronting.At, out, size);
  if (fits(first, size, near)) {
    return first;
  }
  // The least move that fits, straight out or along the edge, keeping the facing.
  const std::optional<Pose> pushed = pushedToFit(fronting.At, out, size, near);
  const std::optional<Pose> slid = slidToFit(fronting.At, out, size, near);
  const auto moved = [&first](const Pose &pose) {
    return std::hypot(pose.X - first.X, pose.Z - first.Z);
  };
  if (pushed && (!slid || moved(*pushed) <= moved(*slid))) {
    return pushed;
  }
  if (slid) {
    return slid;
  }
  // Each place tried faces as a pointer the same distance straight out from it would make it face,
  // so the box turns smoothly between places along a curve, or else square to its own step.
  for (const Candidate &candidate : candidatesNear(steps, fronting.At, out, reach, point)) {
    const ParkPoint standing = plus(candidate.At, scaled(candidate.Out, nearest.Distance));
    const Fronting there =
        frontingFor(steps, NearestPlace{candidate.At, nearest.Distance, candidate.Step}, standing);
    if (const std::optional<Pose> pose = pushedToFit(candidate.At, there.Out, size, near)) {
      return pose;
    }
    if (const std::optional<Pose> pose = pushedToFit(candidate.At, candidate.Out, size, near)) {
      return pose;
    }
  }
  return first;
}

} // namespace tpj
