#include "sim/park/internal/curves.h"

#include "sim/park/internal/around.h"

#include <algorithm>
#include <cmath>
#include <optional>

namespace tpj {

namespace {

// A segment end at a point: which end it is, the unit direction away from the point, half its
// segment's width, and its edge radius.
struct PointEnd {
  SegmentEnd End;
  ParkPoint Along;
  double Half = 0.0;
  double Edge = 0.0;
};

const PathPointIntent *findPoint(const PathLayout &layout, EntityKey key) {
  const auto found = std::ranges::lower_bound(layout.Points, key, {}, &LayoutPoint::Key);
  return found != layout.Points.end() && found->Key == key ? &found->Intent : nullptr;
}

// Each point's segment ends, counter-clockwise from +x, by the point's key. Ends whose segment
// names no point, the same point twice, or two points at one position are left out, as the ground
// lines leave them out.
class Ends {
public:
  explicit Ends(const PathLayout &layout) {
    for (const auto &[key, segment] : layout.Segments) {
      const PathPointIntent *from = findPoint(layout, segment.From);
      const PathPointIntent *to = findPoint(layout, segment.To);
      if (from == nullptr || to == nullptr || segment.From == segment.To) {
        continue;
      }
      const double dx = to->At.X - from->At.X;
      const double dz = to->At.Z - from->At.Z;
      const double length = std::sqrt(dx * dx + dz * dz);
      if (!(length > 0.0)) {
        continue;
      }
      const ParkPoint along{dx / length, dz / length};
      All.emplace_back(segment.From,
                       PointEnd{{key, 0}, along, segment.Width / 2.0, segment.FromEdge});
      All.emplace_back(
          segment.To,
          PointEnd{{key, 1}, {-along.X, -along.Z}, segment.Width / 2.0, segment.ToEdge});
    }
    std::ranges::sort(All, [](const auto &x, const auto &y) {
      if (x.first != y.first) {
        return x.first < y.first;
      }
      if (comesBefore(x.second.Along, y.second.Along) !=
          comesBefore(y.second.Along, x.second.Along)) {
        return comesBefore(x.second.Along, y.second.Along);
      }
      return x.second.End < y.second.End;
    });
  }

  // The point's ends, counter-clockwise.
  std::vector<PointEnd> at(EntityKey point) const {
    std::vector<PointEnd> found;
    const auto run =
        std::ranges::equal_range(All, point, {}, [](const auto &x) { return x.first; });
    for (const auto &[key, end] : run) {
      found.push_back(end);
    }
    return found;
  }

  std::vector<EntityKey> points() const {
    std::vector<EntityKey> keys;
    for (const auto &[key, end] : All) {
      if (keys.empty() || keys.back() != key) {
        keys.push_back(key);
      }
    }
    return keys;
  }

private:
  std::vector<std::pair<EntityKey, PointEnd>> All;
};

// The end of the two at a corner that starts its inside edge: the one the other lies
// counter-clockwise of.
const PointEnd &insideOf(const std::vector<PointEnd> &two) {
  return ccwOf(two[0].Along, two[1].Along) > 0.0 ? two[0] : two[1];
}

double narrowerHalf(const std::vector<PointEnd> &two) { return std::min(two[0].Half, two[1].Half); }

// Whether the point makes a corner in the layout's ground.
std::optional<ParkCorner> cornerAt(const PathGround &ground, EntityKey point) {
  const auto found = std::ranges::lower_bound(ground.Corners, point, {}, &ParkCorner::Point);
  if (found != ground.Corners.end() && found->Point == point) {
    return *found;
  }
  return std::nullopt;
}

// The end's edge radius once the writes so far are made.
double edgeOf(const CurveWrites &writes, const PointEnd &end) {
  // The last write to the end is the one that stands.
  const auto found =
      std::ranges::find_if(writes.Edges.rbegin(), writes.Edges.rend(),
                           [&end](const auto &write) { return write.first == end.End; });
  return found != writes.Edges.rend() ? found->second : end.Edge;
}

// Each corner of before that has three or more segments in after keeps its inside curve as the
// edge radius held for that edge.
void keepInsides(CurveWrites &writes, const PathGround &groundBefore, const Ends &was,
                 const Ends &is) {
  for (const ParkCorner &corner : groundBefore.Corners) {
    const std::vector<PointEnd> now = is.at(corner.Point);
    if (now.size() < 3) {
      continue;
    }
    const std::vector<PointEnd> two = was.at(corner.Point);
    const SegmentEnd inside = insideOf(two).End;
    if (std::ranges::any_of(now, [&inside](const PointEnd &end) { return end.End == inside; })) {
      writes.Edges.emplace_back(inside, std::max(corner.Radius - narrowerHalf(two), 0.0));
    }
  }
}

// Directions whose cross product is within this of zero run along one line.
constexpr double PARALLEL = 1e-9;

// The turn of the outline from one end's left side to the right side of the next end
// counter-clockwise, as the paving's edges fit it: the sine and cosine between them and how a
// fillet's touches grow with its radius.
struct Turn {
  double Sine = 0.0;
  double Cosine = 0.0;
  double Grow = 0.0;
  double HalfA = 0.0;
  double HalfB = 0.0;

  Turn(const PointEnd &a, const PointEnd &b)
      : Sine(ccwOf(a.Along, b.Along)), Cosine(a.Along.X * b.Along.X + a.Along.Z * b.Along.Z),
        Grow((Sine > 0.0 ? 1.0 : -1.0) * (1.0 + Cosine)), HalfA(a.Half), HalfB(b.Half) {}

  bool fillets() const { return std::abs(Sine) > PARALLEL && Grow != 0.0; }
  // How far from the point a fillet of the radius touches the first end, and the second.
  double touchA(double r) const { return (HalfB + HalfA * Cosine + Grow * r) / Sine; }
  double touchB(double r) const { return (HalfA + HalfB * Cosine + Grow * r) / Sine; }
  // The radius whose fillet touches the first end, or the second, the distance from the point.
  double radiusA(double d) const {
    return std::max((d * Sine - HalfB - HalfA * Cosine) / Grow, 0.0);
  }
  double radiusB(double d) const {
    return std::max((d * Sine - HalfA - HalfB * Cosine) / Grow, 0.0);
  }
};

// Each run of ends added at a point that had two or more segments lands inside the edge from the
// nearest end clockwise of it that the edit didn't add to the next one. The edges on either side of
// the run touch those two ends where that edge did, so its curve stays where it was on both sides.
// Edges between added ends keep its radius. A sharp edge stays sharp.
void landAdded(CurveWrites &writes, const std::vector<PointEnd> &now,
               const std::vector<SegmentEnd> &added) {
  const auto isAdded = [&added](const PointEnd &end) {
    return std::ranges::find(added, end.End) != added.end();
  };
  const size_t count = now.size();
  for (size_t first = 0; first < count; ++first) {
    const PointEnd &a = now[first];
    if (isAdded(a) || !isAdded(now[(first + 1) % count])) {
      continue;
    }
    // The run of added ends after a, and the old end b after it.
    size_t last = (first + 1) % count;
    while (isAdded(now[(last + 1) % count]) && (last + 1) % count != first) {
      last = (last + 1) % count;
    }
    const PointEnd &b = now[(last + 1) % count];
    const double radius = edgeOf(writes, a);
    const Turn old(a, b);
    for (size_t i = (first + 1) % count; i != (last + 1) % count; i = (i + 1) % count) {
      writes.Edges.emplace_back(now[i].End, radius);
    }
    if (!(radius > 0.0) || !old.fillets()) {
      continue;
    }
    const Turn toRun(a, now[(first + 1) % count]);
    const Turn fromRun(now[last], b);
    if (toRun.fillets()) {
      writes.Edges.emplace_back(a.End, toRun.radiusA(old.touchA(radius)));
    }
    if (fromRun.fillets()) {
      writes.Edges.emplace_back(now[last].End, fromRun.radiusB(old.touchB(radius)));
    }
  }
}

// Each end that stays at a point that loses segment ends, and whose edge ran to one of them, now
// starts an edge that spans the gap. That edge touches it where its old edge did, so a curve a
// join cut in two comes back when the join goes.
void closeGaps(CurveWrites &writes, const std::vector<PointEnd> &had,
               const std::vector<PointEnd> &now) {
  const auto stays = [&now](const PointEnd &end) {
    return std::ranges::any_of(now, [&end](const PointEnd &kept) { return kept.End == end.End; });
  };
  if (now.size() < 2) {
    return;
  }
  for (size_t i = 0; i < had.size(); ++i) {
    const PointEnd &next = had[(i + 1) % had.size()];
    if (!stays(had[i]) || stays(next) || !(had[i].Edge > 0.0)) {
      continue;
    }
    const auto kept =
        std::ranges::find_if(now, [&had, i](const PointEnd &end) { return end.End == had[i].End; });
    const size_t at = static_cast<size_t>(kept - now.begin());
    const Turn old(had[i], next);
    const Turn spanning(*kept, now[(at + 1) % now.size()]);
    if (old.fillets() && spanning.fillets()) {
      writes.Edges.emplace_back(kept->End, spanning.radiusA(old.touchA(had[i].Edge)));
    }
  }
}

} // namespace

CurveWrites keptCurves(const PathLayout &before, const PathLayout &after,
                       const std::vector<SegmentEnd> &added) {
  const Ends was(before);
  const Ends is(after);
  const PathGround groundAfter = groundOf(after);
  CurveWrites writes;
  keepInsides(writes, groundOf(before), was, is);
  for (const EntityKey point : is.points()) {
    const std::vector<PointEnd> now = is.at(point);
    const std::vector<PointEnd> previous = was.at(point);
    const size_t had = previous.size();
    // An edge's old touch is measured from where the point was, so only a point the edit leaves in
    // place closes its gaps. A point that moves, as a corner's point does when its bend splits,
    // keeps the edges the edit gives it.
    const PathPointIntent *from = findPoint(before, point);
    const PathPointIntent *to = findPoint(after, point);
    if (from != nullptr && to != nullptr && from->At == to->At) {
      closeGaps(writes, previous, now);
    }
    if (had >= 2) {
      landAdded(writes, now, added);
    }
    const PathPointIntent *intent = findPoint(after, point);
    if (intent == nullptr) {
      continue;
    }
    if (had >= 3 && now.size() == 2 && cornerAt(groundAfter, point)) {
      // A junction left with two segments becomes a corner whose inside curve is its inside edge's.
      const double inside = edgeOf(writes, insideOf(now));
      const double half = narrowerHalf(now);
      writes.Radii.emplace_back(point,
                                inside > 0.0 ? inside + half : std::min(intent->Radius, half));
    } else if (had >= 2 && now.size() == 1) {
      writes.Radii.emplace_back(point, DEFAULT_CORNER_RADIUS);
    }
  }
  return writes;
}

} // namespace tpj
