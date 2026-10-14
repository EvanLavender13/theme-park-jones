#include "sim/furniture/furniture.h"

#include "sim/medium/field.h"
#include "sim/medium/network.h"
#include "sim/mix.h"
#include "sim/park/edits.h"
#include "sim/park/geometry.h"
#include "sim/park/surface.h"
#include "sim/routes/networks.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <optional>
#include <set>
#include <span>
#include <stddef.h>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

namespace tpj {
namespace {

// How far a piece's front stands from the paving, in meters.
constexpr double VERGE_GAP = 0.3;
// How far a stretch's nearest piece stands from the end of a side that stops, in meters.
constexpr double END_GAP = 1.0;
// The least room between neighbouring pieces, in meters.
constexpr double PIECE_GAP = 0.4;
// The margin kept around a piece, in meters, so neighbours keep PIECE_GAP apart.
constexpr double CLEARANCE = PIECE_GAP / 2.0;
// The margin neighbours in a stretch keep around each other, in meters. It is less than CLEARANCE
// so that on a gentle curve, where neighbours turn toward each other, they can still stand
// PIECE_GAP apart along their line.
constexpr double NEIGHBOUR_CLEARANCE = CLEARANCE / 2.0;
// How far apart the places along a side that are tested lie, in meters.
constexpr double SAMPLE_STEP = 0.5;
// How many times the stretch between a place where a piece can stand and one where it can't is
// halved to find where a run ends.
constexpr int REFINE_HALVINGS = 10;
// How far a piece moves along its line at a time where it would touch the one before, in meters.
constexpr double NUDGE_STEP = 0.05;
// How far a piece steps back from the paving at a time where it would touch it, as on the inside of
// a tight curve, and how far it may step back in all, in meters.
constexpr double SETBACK_STEP = 0.1;
constexpr double MAX_SETBACK = 1.0;
// The side of the obstacle grid's square cells, in meters.
constexpr double GRID_CELL = 8.0;
// How far before a segment's From point a stretch's start can lie, in meters, so that every
// start keys as a whole number of centimeters at least 0.
constexpr double KEY_ORIGIN = 512.0;

constexpr uint64_t PIECE_PURPOSE = hashName("furniture-piece");

// A piece of furniture a resolution laid: its segment, the side it stands on, its kind, and its
// pose.
struct FurniturePieceState {
  EntityKey Segment = NULL_KEY;
  FurnishedSide Side = FurnishedSide::Left;
  FurnitureKind Kind = FurnitureKind::Bench;
  Pose At;

  bool operator==(const FurniturePieceState &) const = default;
};

template <typename Visitor> void visitFields(Visitor &visitor, FurniturePieceState &piece) {
  visitor.field("segment", piece.Segment);
  visitor.field("side", piece.Side);
  visitor.field("kind", piece.Kind);
  visitor.field("x", piece.At.X);
  visitor.field("z", piece.At.Z);
  visitor.field("facing-x", piece.At.FacingX);
  visitor.field("facing-z", piece.At.FacingZ);
}

constexpr uint64_t LAID_PURPOSE = hashName("laid-furniture");

// One chain a resolution laid: two hashes of everything its laying read, and the keys of the
// pieces it laid.
struct LaidChain {
  uint64_t Read = 0;
  uint64_t ReadCheck = 0;
  std::vector<EntityKey> Pieces;

  bool operator==(const LaidChain &) const = default;
};

template <typename Visitor> void visitFields(Visitor &visitor, LaidChain &chain) {
  visitor.field("read", chain.Read);
  visitor.field("read-check", chain.ReadCheck);
  visitor.field("pieces", chain.Pieces);
}

// Derived, on its own entity: every chain the last resolution laid, so the next lays again only
// the chains whose line, furnishing, or surroundings changed, and keeps the rest as they were.
struct LaidChains {
  std::vector<LaidChain> Chains;
};

template <typename Visitor> void visitFields(Visitor &visitor, LaidChains &laid) {
  visitor.field("chains", laid.Chains);
}

ParkPoint plus(ParkPoint a, ParkPoint b) { return {a.X + b.X, a.Z + b.Z}; }
ParkPoint minus(ParkPoint a, ParkPoint b) { return {a.X - b.X, a.Z - b.Z}; }
ParkPoint scaled(ParkPoint a, double by) { return {a.X * by, a.Z * by}; }
double dotOf(ParkPoint a, ParkPoint b) { return a.X * b.X + a.Z * b.Z; }
double lengthOf(ParkPoint a) { return std::sqrt(dotOf(a, a)); }
// The direction a quarter turn counter-clockwise of the given one, seen from above: its left.
ParkPoint leftOf(ParkPoint a) { return {a.Z, -a.X}; }
ParkPoint rightOf(ParkPoint a) { return {-a.Z, a.X}; }

bool isFurnished(const ParkSegment &segment) {
  return segment.Kind == PathKind::Guest &&
         (segment.Furnished.Left.has_value() || segment.Furnished.Right.has_value()) &&
         !segment.Line.empty();
}

// A convex polygon of at most four corners.
struct Outline {
  std::array<ParkPoint, 4> Corners{};
  size_t Count = 0;
};

// Whether the two outlines' projections on the side's normal overlap by no more than
// CONTACT_TOLERANCE. A side of no length separates nothing.
bool separatedAcross(const Outline &a, const Outline &b, ParkPoint side) {
  const double length = lengthOf(side);
  if (!(length > 0.0)) {
    return false;
  }
  const ParkPoint axis = scaled(leftOf(side), 1.0 / length);
  const auto extent = [axis](const Outline &outline) {
    double low = dotOf(outline.Corners[0], axis);
    double high = low;
    for (size_t i = 1; i < outline.Count; ++i) {
      const double along = dotOf(outline.Corners[i], axis);
      low = std::min(low, along);
      high = std::max(high, along);
    }
    return std::pair{low, high};
  };
  const auto [lowA, highA] = extent(a);
  const auto [lowB, highB] = extent(b);
  return highA - lowB <= CONTACT_TOLERANCE || highB - lowA <= CONTACT_TOLERANCE;
}

// Whether the two outlines overlap by more than CONTACT_TOLERANCE, so outlines that only touch
// don't.
bool overlaps(const Outline &a, const Outline &b) {
  for (const Outline *outline : {&a, &b}) {
    for (size_t i = 0; i < outline->Count; ++i) {
      const ParkPoint side = minus(outline->Corners[(i + 1) % outline->Count], outline->Corners[i]);
      if (separatedAcross(a, b, side)) {
        return false;
      }
    }
  }
  return true;
}

Outline outlineOf(const Footprint &footprint) { return Outline{footprint.Corners, 4}; }

// The footprint grown by the distance on every side.
Outline grownOf(const Footprint &footprint, double by) {
  const ParkPoint ahead = scaled(footprint.Forward, by);
  const ParkPoint aside = scaled(footprint.Right, by);
  return Outline{{minus(plus(footprint.Corners[0], ahead), aside),
                  plus(plus(footprint.Corners[1], ahead), aside),
                  plus(minus(footprint.Corners[2], ahead), aside),
                  minus(minus(footprint.Corners[3], ahead), aside)},
                 4};
}

// What stands in a piece's way: nothing, only paving, or something else.
enum class Blocker : uint8_t { None, Paving, Other };

// The outlines a piece can't overlap, each marked as paving or not, entered in a grid of GRID_CELL
// cells over the park's square, so a test reads only the outlines near it.
class Obstacles {
public:
  Obstacles() : Cells(CELLS * CELLS) {}

  void add(const Outline &outline, bool paving) {
    const size_t index = Outlines.size();
    const Rect box = boundsOf(outline);
    Outlines.push_back(Entry{outline, box, paving});
    Seen.push_back(0);
    forEachCell(box, [index](std::vector<size_t> &cell) { cell.push_back(index); });
  }

  // What the outline overlaps among the outlines added: something other than paving if anything,
  // and otherwise paving if any.
  Blocker blocker(const Outline &outline) {
    Blocker found = Blocker::None;
    ++Stamp;
    visitNear(boundsOf(outline), [&](const Entry &entry) {
      if (found == Blocker::Other || (entry.Paving && found == Blocker::Paving) ||
          !overlaps(entry.Shape, outline)) {
        return;
      }
      found = entry.Paving ? Blocker::Paving : Blocker::Other;
    });
    return found;
  }

  // A rectangle on the ground, from its least to its greatest coordinates.
  struct Rect {
    double MinX = 0.0;
    double MinZ = 0.0;
    double MaxX = 0.0;
    double MaxZ = 0.0;
  };

  // Adds to the hasher each outline whose bounds reach any of the rectangles, once, in the order
  // the outlines were added: its corners and whether it is paving.
  void hashNear(Hasher &hasher, std::span<const Rect> rects) {
    ++Stamp;
    Near.clear();
    for (const Rect &rect : rects) {
      visitNear(rect, [this](const Entry &entry) {
        Near.push_back(static_cast<size_t>(&entry - Outlines.data()));
      });
    }
    std::ranges::sort(Near);
    for (const size_t index : Near) {
      const Entry &entry = Outlines[index];
      hasher.add(entry.Shape.Count);
      for (size_t i = 0; i < entry.Shape.Count; ++i) {
        hasher.add(std::bit_cast<uint64_t>(entry.Shape.Corners[i].X));
        hasher.add(std::bit_cast<uint64_t>(entry.Shape.Corners[i].Z));
      }
      hasher.add(entry.Paving ? 1U : 0U);
    }
  }

private:
  static constexpr size_t CELLS = static_cast<size_t>(PARK_SIZE / GRID_CELL);

  struct Entry {
    Outline Shape;
    Rect Bounds;
    bool Paving = false;
  };

  static Rect boundsOf(const Outline &outline) {
    Rect box{outline.Corners[0].X, outline.Corners[0].Z, outline.Corners[0].X,
             outline.Corners[0].Z};
    for (size_t i = 1; i < outline.Count; ++i) {
      box.MinX = std::min(box.MinX, outline.Corners[i].X);
      box.MaxX = std::max(box.MaxX, outline.Corners[i].X);
      box.MinZ = std::min(box.MinZ, outline.Corners[i].Z);
      box.MaxZ = std::max(box.MaxZ, outline.Corners[i].Z);
    }
    return box;
  }

  // The cell the coordinate lies in, clamped to the grid.
  static size_t cellOf(double coordinate) {
    const double cell = std::floor((coordinate + (PARK_SIZE / 2.0)) / GRID_CELL);
    if (!(cell > 0.0)) {
      return 0;
    }
    return cell >= static_cast<double>(CELLS - 1) ? CELLS - 1 : static_cast<size_t>(cell);
  }

  // Calls visit with each cell the rectangle reaches.
  template <typename Visit> void forEachCell(const Rect &box, Visit visit) {
    for (size_t z = cellOf(box.MinZ); z <= cellOf(box.MaxZ); ++z) {
      for (size_t x = cellOf(box.MinX); x <= cellOf(box.MaxX); ++x) {
        visit(Cells[(z * CELLS) + x]);
      }
    }
  }

  // Calls visit with each outline whose bounds reach the rectangle and that no call since the
  // stamp last changed has visited.
  template <typename Visit> void visitNear(const Rect &box, Visit visit) {
    forEachCell(box, [&](const std::vector<size_t> &cell) {
      for (const size_t index : cell) {
        const Entry &entry = Outlines[index];
        if (Seen[index] == Stamp || entry.Bounds.MinX > box.MaxX || entry.Bounds.MaxX < box.MinX ||
            entry.Bounds.MinZ > box.MaxZ || entry.Bounds.MaxZ < box.MinZ) {
          continue;
        }
        Seen[index] = Stamp;
        visit(entry);
      }
    });
  }

  std::vector<Entry> Outlines;
  std::vector<std::vector<size_t>> Cells;
  // The stamp when each outline was last visited, so an outline in several cells is visited once
  // a call.
  std::vector<uint64_t> Seen;
  uint64_t Stamp = 0;
  std::vector<size_t> Near;
};

// What furniture yields to, and the paved surface whose sides it stands along.
struct Surroundings {
  Obstacles Blocking;
  const PavedSurface *Surface = nullptr;
};

// Every piece of paving of either kind, walkways among them, every box's and entrance's
// footprint, and the frontage strip in front of each door with a walkway: from the door's face,
// as wide as the face, out to where its walkway ends.
Surroundings surroundingsOf(const World &world) {
  Surroundings found;
  for (const PathKind kind : {PathKind::Guest, PathKind::Backstage}) {
    std::vector<std::pair<EntityKey, ParkDoor>> doors;
    for (const ParkDoor &door : parkDoors(world, kind)) {
      doors.emplace_back(connectorKey(door.Entity, door.Side), door);
    }
    std::ranges::sort(doors, {}, &std::pair<EntityKey, ParkDoor>::first);
    for (const KeyedWalkway &keyed : keyedWalkways(world, kind)) {
      const auto door = std::ranges::lower_bound(doors, keyed.Connector, {},
                                                 &std::pair<EntityKey, ParkDoor>::first);
      if (door == doors.end() || door->first != keyed.Connector) {
        continue;
      }
      const ParkDoor &at = door->second;
      const double depth = dotOf(minus(keyed.Way.Meets, at.At), at.Out);
      if (!(depth > 0.0)) {
        continue;
      }
      const ParkPoint across = scaled(leftOf(at.Out), at.Width / 2.0);
      const ParkPoint ahead = scaled(at.Out, depth);
      found.Blocking.add(
          Outline{{minus(at.At, across), plus(at.At, across), plus(plus(at.At, across), ahead),
                   plus(minus(at.At, across), ahead)},
                  4},
          false);
    }
  }
  found.Surface = &walkedSurface(world);
  for (const SurfacePiece &piece : found.Surface->Pieces) {
    found.Blocking.add(Outline{piece.Corners, piece.Count}, true);
  }
  for (const ParkBox &box : parkBoxes(world)) {
    if (const std::optional<Footprint> footprint = footprintOf(box.At, boxSize(box.Kind))) {
      found.Blocking.add(outlineOf(*footprint), false);
    }
  }
  for (const ParkEntrance &entrance : parkEntrances(world)) {
    if (const std::optional<Footprint> footprint = footprintOf(entrance.At, ENTRANCE_SIZE)) {
      found.Blocking.add(outlineOf(*footprint), false);
    }
  }
  return found;
}

// A furnished side that furniture is laid along: its segment, whether it is the left side, its
// furnishing, its side as the surface gives it, and where its segment's stations are measured
// from, its From point and its unit direction.
struct Member {
  EntityKey Segment = NULL_KEY;
  bool Left = false;
  SideFurnishing Furnishing;
  const SegmentSide *Side = nullptr;
  ParkPoint From;
  ParkPoint Along;
};

// A member as a chain walks it: the member's index, and whether the chain walks it from its To end
// to its From end.
struct Step {
  size_t Member = 0;
  bool Reversed = false;
};

// Furnished sides joined end to end where each runs on into the next with the same furnishing, in
// the order a chain walks them. A closed chain runs on from its last side into its first.
struct Chain {
  std::vector<Step> Steps;
  bool Closed = false;
};

// The index of the member that is the linked side, when it is furnished as given and not yet in
// a chain.
std::optional<size_t> linkedMember(const std::vector<Member> &members,
                                   const std::vector<bool> &used, const SideLink &link,
                                   const SideFurnishing &furnishing) {
  if (link.Segment == NULL_KEY) {
    return std::nullopt;
  }
  // Members are in key order of their segment, the left side first.
  auto found = std::ranges::lower_bound(members, link.Segment, {}, &Member::Segment);
  if (found != members.end() && found->Segment == link.Segment && found->Left != link.Left) {
    ++found;
  }
  if (found == members.end() || found->Segment != link.Segment || found->Left != link.Left ||
      !(found->Furnishing == furnishing)) {
    return std::nullopt;
  }
  const auto index = static_cast<size_t>(found - members.begin());
  return used[index] ? std::nullopt : std::optional<size_t>(index);
}

// Joins the members into chains. Each chain is found from its lowest member not yet in a chain:
// back from it, and then forward from its first side. A closed chain starts at that member.
std::vector<Chain> chainsOf(const std::vector<Member> &members) {
  std::vector<Chain> chains;
  std::vector<bool> used(members.size(), false);
  for (size_t i = 0; i < members.size(); ++i) {
    if (used[i]) {
      continue;
    }
    const SideFurnishing &furnishing = members[i].Furnishing;
    Step first{i, false};
    bool closed = false;
    for (size_t walked = 0; walked < members.size(); ++walked) {
      const SegmentSide &side = *members[first.Member].Side;
      const SideLink &back = first.Reversed ? side.RunsOn[1] : side.RunsOn[0];
      const std::optional<size_t> previous = linkedMember(members, used, back, furnishing);
      if (!previous) {
        break;
      }
      if (*previous == i) {
        closed = true;
        first = Step{i, false};
        break;
      }
      first = Step{*previous, back.AtFrom};
    }
    Chain chain{{first}, closed};
    used[first.Member] = true;
    for (;;) {
      const Step &last = chain.Steps.back();
      const SegmentSide &side = *members[last.Member].Side;
      const SideLink &ahead = last.Reversed ? side.RunsOn[0] : side.RunsOn[1];
      const std::optional<size_t> next = linkedMember(members, used, ahead, furnishing);
      if (!next) {
        break;
      }
      used[*next] = true;
      chain.Steps.push_back(Step{*next, !ahead.AtFrom});
    }
    chains.push_back(std::move(chain));
  }
  return chains;
}

// Where one step of a chain lies along the chain's line: from Start to End in length along it,
// and, so its stations can be measured, the length along the line at a place on its body, Body,
// the station there, and Sign, 1 when stations grow along the line and -1 when they shrink.
struct StepSpan {
  double Start = 0.0;
  double End = 0.0;
  double Body = 0.0;
  double Station = 0.0;
  double Sign = 1.0;
};

// The line a chain's pieces stand on, their centers' line: its sides' lines joined end to end and
// moved off the paving by the distance. Places on it are given by their length along it, which
// wraps around a closed chain.
class ChainLine {
public:
  ChainLine(const std::vector<Member> &members, const Chain &chain, const PavedSurface &surface,
            double distance)
      : Closed(chain.Closed) {
    const Member &head = members[chain.Steps.front().Member];
    OutLeft = head.Left != chain.Steps.front().Reversed;
    std::vector<ParkPoint> raw;
    std::vector<std::array<size_t, 3>> rawSpans;
    for (const Step &step : chain.Steps) {
      const SegmentSide &side = *members[step.Member].Side;
      const std::span<const ParkPoint> points = sidePoints(surface, side);
      const size_t count = points.size();
      const size_t start = raw.empty() ? 0 : raw.size() - 1;
      for (size_t k = 0; k < count; ++k) {
        const ParkPoint point = points[step.Reversed ? count - 1 - k : k];
        if (raw.empty() || k > 0) {
          raw.push_back(point);
        }
      }
      const size_t body = step.Reversed ? count - 1 - side.Body : side.Body;
      rawSpans.push_back({start, start + body, start + count - 1});
    }
    const std::vector<size_t> kept = offsetLine(raw, distance);
    Lengths.reserve(Points.size());
    Lengths.push_back(0.0);
    for (size_t i = 1; i < Points.size(); ++i) {
      Lengths.push_back(Lengths.back() + lengthOf(minus(Points[i], Points[i - 1])));
    }
    for (size_t s = 0; s < chain.Steps.size(); ++s) {
      const Member &member = members[chain.Steps[s].Member];
      const auto &[start, body, end] = rawSpans[s];
      const double atBody = Lengths[kept[body]];
      Spans.push_back(StepSpan{Lengths[kept[start]], Lengths[kept[end]], atBody,
                               dotOf(minus(Points[kept[body]], member.From), member.Along),
                               chain.Steps[s].Reversed ? -1.0 : 1.0});
    }
  }

  double length() const { return Lengths.back(); }
  bool outLeft() const { return OutLeft; }
  const std::vector<StepSpan> &spans() const { return Spans; }

  // The position the length along the line, wrapped around a closed line and clamped to the ends
  // of an open one.
  ParkPoint at(double along) const {
    const double total = Lengths.back();
    if (Closed && total > 0.0) {
      along -= std::floor(along / total) * total;
    }
    along = std::clamp(along, 0.0, total);
    const auto next = std::ranges::upper_bound(Lengths, along);
    if (next == Lengths.end()) {
      return Points.back();
    }
    const auto k = static_cast<size_t>(next - Lengths.begin());
    const double piece = Lengths[k] - Lengths[k - 1];
    return plus(Points[k - 1],
                scaled(minus(Points[k], Points[k - 1]), (along - Lengths[k - 1]) / piece));
  }

  // The index of the step whose part of the line holds the length along it.
  size_t stepAt(double along) const {
    const double total = Lengths.back();
    if (Closed && total > 0.0) {
      along -= std::floor(along / total) * total;
    }
    for (size_t s = 0; s + 1 < Spans.size(); ++s) {
      if (along < Spans[s].End) {
        return s;
      }
    }
    return Spans.size() - 1;
  }

private:
  // Moves each raw position off the paving along the bisector of its two steps' outward
  // directions, far enough to lie the distance from both, at most twice the distance, into Points.
  // Where the line folds back on itself, as inside a bend tighter than the distance, each position
  // that would step backward is left out. Gives, for each raw position, the index of the position
  // kept for it, or of the one kept before it.
  std::vector<size_t> offsetLine(const std::vector<ParkPoint> &raw, double distance) {
    const size_t n = raw.size();
    const auto outOf = [this](ParkPoint d) { return OutLeft ? leftOf(d) : rightOf(d); };
    std::vector<ParkPoint> steps;
    steps.reserve(n);
    for (size_t i = 1; i < n; ++i) {
      const ParkPoint step = minus(raw[i], raw[i - 1]);
      const double length = lengthOf(step);
      steps.push_back(length > 0.0 ? scaled(step, 1.0 / length) : ParkPoint{});
    }
    std::vector<size_t> kept(n, 0);
    for (size_t i = 0; i < n; ++i) {
      // At the ends of an open line, a position has one step; a closed line wraps around.
      size_t beforeIndex = i - 1;
      size_t afterIndex = i;
      if (i == 0) {
        beforeIndex = Closed ? n - 2 : 0;
      }
      if (i + 1 == n) {
        afterIndex = Closed ? 0 : n - 2;
      }
      const ParkPoint before = steps[beforeIndex];
      const ParkPoint after = steps[afterIndex];
      ParkPoint off = outOf(before);
      const ParkPoint sum = plus(outOf(before), outOf(after));
      const double length = lengthOf(sum);
      if (length > 0.0) {
        const ParkPoint bisector = scaled(sum, 1.0 / length);
        off = scaled(bisector, 1.0 / std::max(dotOf(bisector, outOf(before)), 0.5));
      }
      const ParkPoint placed = plus(raw[i], scaled(off, distance));
      if (i > 0 && i + 1 < n && !(dotOf(minus(placed, Points.back()), steps[i - 1]) > 0.0)) {
        kept[i] = Points.size() - 1;
        continue;
      }
      kept[i] = Points.size();
      Points.push_back(placed);
    }
    return kept;
  }

  std::vector<ParkPoint> Points;
  std::vector<double> Lengths;
  std::vector<StepSpan> Spans;
  bool Closed = false;
  bool OutLeft = false;
};

// Where a piece stands: its pose, its footprint grown by CLEARANCE, and its footprint grown by
// NEIGHBOUR_CLEARANCE.
struct Standing {
  Pose At;
  Outline Grown;
  Outline Near;
};

// Pieces of one kind placed along a chain's line by length along it.
class Placing {
public:
  Placing(const ChainLine &line, FurnitureKind kind) : Line(line), Size(furnitureSize(kind)) {}

  // Where a piece centered at the length along the line can stand: square to the chord of the line
  // across its width, facing the paving. Where only the paving is in its way, it steps back from
  // the paving by SETBACK_STEP at a time, up to MAX_SETBACK. None when something else is in its
  // way, when it can't stand at any of those, or when the chord has no length.
  std::optional<Standing> standing(Obstacles &blocking, double along) const {
    const ParkPoint a = Line.at(along - (Size.Width / 2.0));
    const ParkPoint b = Line.at(along + (Size.Width / 2.0));
    const double length = lengthOf(minus(b, a));
    if (!(length > 0.0)) {
      return std::nullopt;
    }
    const ParkPoint d = scaled(minus(b, a), 1.0 / length);
    const ParkPoint out = Line.outLeft() ? leftOf(d) : rightOf(d);
    const ParkPoint center = Line.at(along);
    for (int step = 0; static_cast<double>(step) * SETBACK_STEP <= MAX_SETBACK + 1e-9; ++step) {
      const ParkPoint at = plus(center, scaled(out, static_cast<double>(step) * SETBACK_STEP));
      const Pose pose{at.X, at.Z, -out.X, -out.Z};
      const std::optional<Footprint> footprint = footprintOf(pose, Size);
      if (!footprint) {
        return std::nullopt;
      }
      const bool inside = std::ranges::all_of(footprint->Corners, [](ParkPoint corner) {
        return std::abs(corner.X) <= PARK_SIZE / 2.0 && std::abs(corner.Z) <= PARK_SIZE / 2.0;
      });
      if (!inside) {
        return std::nullopt;
      }
      const Outline grown = grownOf(*footprint, CLEARANCE);
      const Blocker blocker = blocking.blocker(grown);
      if (blocker == Blocker::None) {
        return Standing{pose, grown, grownOf(*footprint, NEIGHBOUR_CLEARANCE)};
      }
      if (blocker == Blocker::Other) {
        return std::nullopt;
      }
    }
    return std::nullopt;
  }

  FootprintSize size() const { return Size; }

private:
  const ChainLine &Line;
  FootprintSize Size;
};

// The places along the line from lo to hi to test: lo, hi, and each whole multiple of SAMPLE_STEP
// in each step's own stations between them, so a change to one side of the chain doesn't move
// where the others are tested.
std::vector<double> placesOf(const ChainLine &line, double lo, double hi) {
  std::vector<double> places{lo, hi};
  for (const StepSpan &span : line.spans()) {
    const double first = span.Station + (span.Sign * (span.Start - span.Body));
    const double last = span.Station + (span.Sign * (span.End - span.Body));
    const double low = std::min(first, last);
    const double high = std::max(first, last);
    for (auto k = static_cast<int64_t>(std::floor(low / SAMPLE_STEP)) + 1;
         static_cast<double>(k) * SAMPLE_STEP < high; ++k) {
      const double along =
          span.Body + (span.Sign * ((static_cast<double>(k) * SAMPLE_STEP) - span.Station));
      if (along > lo && along < hi) {
        places.push_back(along);
      }
    }
  }
  std::ranges::sort(places);
  places.erase(std::unique(places.begin(), places.end()), places.end());
  return places;
}

// The runs of places where a piece can stand, each from where it starts to where it ends. Each
// end of a run between a free place and a blocked one is found by halving between the two. On a
// closed line, a run through its end runs on into the run at its start.
std::vector<std::pair<double, double>> runsOf(const Placing &placing, Obstacles &blocking,
                                              const std::vector<double> &places, bool closed,
                                              double total) {
  std::vector<bool> free;
  free.reserve(places.size());
  for (const double place : places) {
    free.push_back(placing.standing(blocking, place).has_value());
  }
  const auto edgeBetween = [&](double freePlace, double blockedPlace) {
    for (int i = 0; i < REFINE_HALVINGS; ++i) {
      const double middle = (freePlace + blockedPlace) / 2.0;
      if (placing.standing(blocking, middle)) {
        freePlace = middle;
      } else {
        blockedPlace = middle;
      }
    }
    return freePlace;
  };
  std::vector<std::pair<double, double>> runs;
  for (size_t i = 0; i < places.size();) {
    if (!free[i]) {
      ++i;
      continue;
    }
    size_t last = i;
    while (last + 1 < places.size() && free[last + 1]) {
      ++last;
    }
    const double start = i == 0 ? places[i] : edgeBetween(places[i], places[i - 1]);
    const double end =
        last + 1 == places.size() ? places[last] : edgeBetween(places[last], places[last + 1]);
    runs.emplace_back(start, end);
    i = last + 1;
  }
  if (closed && runs.size() > 1 && free.front() && free.back()) {
    runs.back().second = total + runs.front().second;
    runs.erase(runs.begin());
  }
  return runs;
}

// A piece a spread puts down: where along the line it stands, its pose, and its grown footprints.
struct Placed {
  double Along = 0.0;
  Pose At;
  Outline Grown;
  Outline Near;
};

// The count of gaps that spreads the room nearest the step, as long as neighbours keep the least
// step apart: of the two counts around the room over the step, the one whose spacing lies nearer.
uint64_t gapsFor(double room, double step, double leastStep) {
  const auto fewer = static_cast<uint64_t>(std::floor(room / step));
  const auto spacingOf = [room](uint64_t count) {
    return count == 0 ? room : room / static_cast<double>(count);
  };
  if (spacingOf(fewer + 1) >= leastStep &&
      std::abs(spacingOf(fewer + 1) - step) < std::abs(spacingOf(fewer) - step)) {
    return fewer + 1;
  }
  return fewer;
}

// Spreads pieces evenly over a run, into placed. An open run's pieces stand at both its ends, or
// one at its middle when there is no gap. A ring holds one piece per gap all the way around, from
// the run's start. Pieces are laid in order, each at an even share of the room left after the one
// before. Where a piece would touch the one before, as around a tight curve, it moves on by
// NUDGE_STEP at a time until it doesn't, and the pieces after it share what is left. If the last
// piece can't fit, the run takes one gap fewer and spreads again. A piece that something else
// blocks is left out.
void spreadRun(const Placing &placing, Obstacles &blocking, std::pair<double, double> run,
               bool ring, double step, std::vector<Placed> &placed) {
  const double leastStep = placing.size().Width + PIECE_GAP;
  const double room = run.second - run.first;
  uint64_t gaps = gapsFor(room, step, leastStep);
  // Whether a piece standing there touches the one before, or, last in a ring, the first.
  const auto touches = [&](const Standing &standing, bool last) {
    return (!placed.empty() && overlaps(placed.back().Near, standing.Near)) ||
           (ring && last && placed.size() > 1 && overlaps(placed.front().Near, standing.Near));
  };
  for (;; --gaps) {
    placed.clear();
    bool fits = true;
    const uint64_t count = ring ? std::max<uint64_t>(gaps, 1) : gaps + 1;
    double previous = run.first;
    for (uint64_t i = 0; i < count && fits; ++i) {
      double along = run.first;
      if (gaps == 0) {
        along = (run.first + run.second) / 2.0;
      } else if (i > 0) {
        along = previous + ((run.second - previous) / static_cast<double>(gaps - i + 1));
      }
      const double limit = ring ? run.second - leastStep : run.second;
      std::optional<Standing> standing = placing.standing(blocking, along);
      while (standing && touches(*standing, i + 1 == count) && along < limit) {
        along = std::min(along + NUDGE_STEP, limit);
        standing = placing.standing(blocking, along);
      }
      previous = along;
      if (!standing) {
        continue;
      }
      if (touches(*standing, i + 1 == count)) {
        fits = false;
        break;
      }
      placed.push_back(Placed{along, standing->At, standing->Grown, standing->Near});
    }
    if (fits || gaps == 0) {
      return;
    }
  }
}

// Lays the pieces of one chain: in each run where a piece can stand, evenly spread. Each piece
// belongs to the side it stands beside, and its key comes from that side and where it stands along
// that side's segment. Adds each piece's key to laid and to keys, and its grown footprint to the
// obstacles.
void layChain(World &world, Obstacles &blocking, const std::vector<Member> &members,
              const Chain &chain, const PavedSurface &surface, std::set<EntityKey> &laid,
              std::vector<EntityKey> &keys) {
  const SideFurnishing &furnishing = members[chain.Steps.front().Member].Furnishing;
  const FootprintSize size = furnitureSize(furnishing.Kind);
  const ChainLine line(members, chain, surface, VERGE_GAP + (size.Depth / 2.0));
  const Placing placing(line, furnishing.Kind);
  const double step = std::max(furnishing.Spacing, size.Width + PIECE_GAP);
  const double total = line.length();
  double lo = 0.0;
  double hi = total;
  if (!chain.Closed) {
    const Step &head = chain.Steps.front();
    const Step &tail = chain.Steps.back();
    const SegmentSide &first = *members[head.Member].Side;
    const SegmentSide &last = *members[tail.Member].Side;
    const bool startRunsOn =
        (head.Reversed ? first.RunsOn[1] : first.RunsOn[0]).Segment != NULL_KEY;
    const bool endRunsOn = (tail.Reversed ? last.RunsOn[0] : last.RunsOn[1]).Segment != NULL_KEY;
    lo = startRunsOn ? step / 2.0 : END_GAP + (size.Width / 2.0);
    hi = total - (endRunsOn ? step / 2.0 : END_GAP + (size.Width / 2.0));
    if (hi < lo) {
      return;
    }
  }
  std::vector<double> places = placesOf(line, lo, hi);
  if (chain.Closed && !places.empty() && places.back() >= total) {
    places.pop_back();
  }
  const std::vector<std::pair<double, double>> runs =
      runsOf(placing, blocking, places, chain.Closed, total);
  // A closed line free all the way around, or but for a place too short for a piece, is a ring
  // from where its one run starts.
  const bool ring = chain.Closed && runs.size() == 1 &&
                    total - (runs.front().second - runs.front().first) < size.Width + PIECE_GAP;
  std::vector<Placed> placed;
  for (const std::pair<double, double> &run : runs) {
    spreadRun(placing, blocking, ring ? std::pair{run.first, run.first + total} : run, ring, step,
              placed);
    for (const Placed &piece : placed) {
      const Member &member = members[chain.Steps[line.stepAt(piece.Along)].Member];
      const ParkPoint at = line.at(piece.Along);
      const double station = dotOf(minus(at, member.From), member.Along) + KEY_ORIGIN;
      uint64_t index =
          (member.Left ? 0ULL : (1ULL << 63)) |
          (static_cast<uint64_t>(std::max(std::floor(station * 100.0), 0.0)) & ((1ULL << 62) - 1));
      EntityKey key = world.createDerivedEntity(member.Segment, PIECE_PURPOSE, index);
      while (laid.contains(key)) {
        key = world.createDerivedEntity(member.Segment, PIECE_PURPOSE, ++index);
      }
      world.emplaceOrReplace<FurniturePieceState>(
          key, FurniturePieceState{member.Segment,
                                   member.Left ? FurnishedSide::Left : FurnishedSide::Right,
                                   furnishing.Kind, piece.At});
      blocking.add(piece.Grown, false);
      laid.insert(key);
      keys.push_back(key);
    }
  }
}

// The side of the segment the surface gives, or none.
const SegmentSide *sideOf(const PavedSurface &surface, EntityKey segment, bool left) {
  const auto first = std::ranges::lower_bound(surface.Sides, segment, {}, &SegmentSide::Segment);
  for (auto it = first; it != surface.Sides.end() && it->Segment == segment; ++it) {
    if (it->Left == left) {
      return &*it;
    }
  }
  return nullptr;
}

// Every furnished side of a guest segment that the surface gives, as a member, in key order of the
// segment, the left side first.
std::vector<Member> membersOf(const World &world, const std::vector<ParkSegment> &segments,
                              const PavedSurface &surface) {
  const std::vector<ParkPathPoint> points = parkPathPoints(world);
  const auto positionOf = [&points](EntityKey key) -> std::optional<ParkPoint> {
    const auto found = std::ranges::lower_bound(points, key, {}, &ParkPathPoint::Key);
    return found != points.end() && found->Key == key ? std::optional<ParkPoint>(found->At)
                                                      : std::nullopt;
  };
  std::vector<Member> members;
  for (const ParkSegment &segment : segments) {
    const std::optional<ParkPoint> from = positionOf(segment.From);
    const std::optional<ParkPoint> to = positionOf(segment.To);
    if (!isFurnished(segment) || !from || !to) {
      continue;
    }
    const ParkPoint span = minus(*to, *from);
    const double spanLength = lengthOf(span);
    if (!(spanLength > 0.0)) {
      continue;
    }
    for (const bool left : {true, false}) {
      const std::optional<SideFurnishing> &furnishing =
          left ? segment.Furnished.Left : segment.Furnished.Right;
      const SegmentSide *side = sideOf(surface, segment.Key, left);
      if (furnishing && side != nullptr) {
        members.push_back(
            Member{segment.Key, left, *furnishing, side, *from, scaled(span, 1.0 / spanLength)});
      }
    }
  }
  return members;
}

// Two hashes of everything laying the chain reads: its furnishing, each of its sides with its line
// and its segment's stations, and every obstacle near enough to its line to stand in a piece's way.
std::array<uint64_t, 2> readBy(const std::vector<Member> &members, const Chain &chain,
                               const PavedSurface &surface, Obstacles &blocking) {
  const SideFurnishing &furnishing = members[chain.Steps.front().Member].Furnishing;
  const FootprintSize size = furnitureSize(furnishing.Kind);
  // The farthest from a side's line any test of a piece reaches: its line's greatest offset, its
  // greatest setback, and its grown footprint around its center, with a margin.
  const double reach = (2.0 * (VERGE_GAP + (size.Depth / 2.0))) + MAX_SETBACK +
                       lengthOf(ParkPoint{size.Width / 2.0, size.Depth / 2.0}) + CLEARANCE +
                       SAMPLE_STEP;
  std::array<Hasher, 2> hashers;
  hashers[1].add(1);
  const auto add = [&hashers](uint64_t word) {
    for (Hasher &hasher : hashers) {
      hasher.add(word);
    }
  };
  const auto addPoint = [&add](ParkPoint point) {
    add(std::bit_cast<uint64_t>(point.X));
    add(std::bit_cast<uint64_t>(point.Z));
  };
  add(static_cast<uint64_t>(furnishing.Kind));
  add(std::bit_cast<uint64_t>(furnishing.Spacing));
  add(chain.Closed ? 1U : 0U);
  std::vector<Obstacles::Rect> near;
  for (const Step &step : chain.Steps) {
    const Member &member = members[step.Member];
    const SegmentSide &side = *member.Side;
    add(static_cast<uint64_t>(member.Segment));
    add(member.Left ? 1U : 0U);
    add(step.Reversed ? 1U : 0U);
    addPoint(member.From);
    addPoint(member.Along);
    add(side.Body);
    add(side.RunsOn[0].Segment != NULL_KEY ? 1U : 0U);
    add(side.RunsOn[1].Segment != NULL_KEY ? 1U : 0U);
    const std::span<const ParkPoint> points = sidePoints(surface, side);
    add(points.size());
    for (size_t k = 0; k < points.size(); ++k) {
      addPoint(points[k]);
      const ParkPoint previous = points[k > 0 ? k - 1 : k];
      near.push_back(Obstacles::Rect{
          std::min(previous.X, points[k].X) - reach, std::min(previous.Z, points[k].Z) - reach,
          std::max(previous.X, points[k].X) + reach, std::max(previous.Z, points[k].Z) + reach});
    }
  }
  for (Hasher &hasher : hashers) {
    blocking.hashNear(hasher, near);
  }
  return {hashers[0].value(), hashers[1].value()};
}

// The chain the last resolution laid with the same reads, when all its pieces are still there.
const LaidChain *keptChain(const World &world, const std::vector<const LaidChain *> &kept,
                           const std::array<uint64_t, 2> &read) {
  const auto found = std::ranges::lower_bound(kept, read, {}, [](const LaidChain *chain) {
    return std::array<uint64_t, 2>{chain->Read, chain->ReadCheck};
  });
  if (found == kept.end() || (*found)->Read != read[0] || (*found)->ReadCheck != read[1]) {
    return nullptr;
  }
  const bool whole = std::ranges::all_of(
      (*found)->Pieces, [&world](EntityKey key) { return world.has<FurniturePieceState>(key); });
  return whole ? *found : nullptr;
}

// Publishes each piece's seat place from its segment, at the place on the segment's guest carrier
// nearest the piece, each segment's entries in key order of their pieces.
void publishSeatPlaces(World &world) {
  const Network &network = parkNetwork(world, PathKind::Guest);
  std::vector<std::pair<EntityKey, PlacedEntry<SeatPlace>>> placed;
  for (const FurniturePiece &piece : parkFurniture(world)) {
    const std::optional<Place> at =
        network.nearestPlaceOn(piece.Segment, GroundPoint{piece.At.X, piece.At.Z});
    if (!at) {
      continue;
    }
    placed.emplace_back(
        piece.Segment,
        PlacedEntry<SeatPlace>{*at, SeatPlace{piece.Key, piece.At, piece.Kind,
                                              furnitureSeats(piece.Kind), piece.Segment}});
  }
  std::ranges::stable_sort(placed, {}, &std::pair<EntityKey, PlacedEntry<SeatPlace>>::first);
  for (size_t i = 0; i < placed.size();) {
    const EntityKey segment = placed[i].first;
    std::vector<PlacedEntry<SeatPlace>> &entries = publishResolved<SeatPlaces>(world, segment);
    for (; i < placed.size() && placed[i].first == segment; ++i) {
      entries.push_back(placed[i].second);
    }
  }
}

// Lays the pieces each furnished guest segment asks for, chain by chain, and removes every piece
// laid before that this resolution doesn't lay. A chain whose reads are the same as when the last
// resolution laid it keeps its pieces, which are what laying it again would give.
void layFurniture(World &world, TickBuffers & /*buffers*/) {
  const std::vector<EntityKey> before = world.keysWith<FurniturePieceState>();
  const EntityKey laidKey = deriveKey(NULL_KEY, LAID_PURPOSE, 0);
  std::vector<const LaidChain *> kept;
  if (const LaidChains *held = world.tryGet<LaidChains>(laidKey)) {
    for (const LaidChain &chain : held->Chains) {
      kept.push_back(&chain);
    }
  }
  std::ranges::sort(kept, {}, [](const LaidChain *chain) {
    return std::array<uint64_t, 2>{chain->Read, chain->ReadCheck};
  });
  LaidChains now;
  std::set<EntityKey> laid;
  const std::vector<ParkSegment> segments = parkSegments(world);
  if (std::ranges::any_of(segments, isFurnished)) {
    Surroundings around = surroundingsOf(world);
    const std::vector<Member> members = membersOf(world, segments, *around.Surface);
    for (const Chain &chain : chainsOf(members)) {
      const std::array<uint64_t, 2> read = readBy(members, chain, *around.Surface, around.Blocking);
      if (const LaidChain *same = keptChain(world, kept, read)) {
        for (const EntityKey key : same->Pieces) {
          const FurniturePieceState &piece = world.get<FurniturePieceState>(key);
          if (const std::optional<Footprint> footprint =
                  footprintOf(piece.At, furnitureSize(piece.Kind))) {
            around.Blocking.add(grownOf(*footprint, CLEARANCE), false);
          }
          laid.insert(key);
        }
        now.Chains.push_back(*same);
        continue;
      }
      LaidChain made{read[0], read[1], {}};
      layChain(world, around.Blocking, members, chain, *around.Surface, laid, made.Pieces);
      now.Chains.push_back(std::move(made));
    }
  }
  for (const EntityKey key : before) {
    if (!laid.contains(key)) {
      world.destroyEntity(key);
    }
  }
  if (now.Chains.empty()) {
    world.destroyEntity(laidKey);
  } else {
    world.emplaceOrReplace<LaidChains>(world.createDerivedEntity(NULL_KEY, LAID_PURPOSE, 0),
                                       std::move(now));
  }
  publishSeatPlaces(world);
}

// Meters a seat lies from a piece's middle along it, and a table's from its middle across it.
constexpr double SEAT_ALONG = 0.45;
constexpr double TABLE_SEAT_ACROSS = 0.5;

} // namespace

Pose seatPose(FurnitureKind kind, const Pose &piece, uint32_t seat) {
  if (seat >= furnitureSeats(kind)) {
    throw std::out_of_range("seatPose: the piece has no seat " + std::to_string(seat));
  }
  const double length =
      std::sqrt((piece.FacingX * piece.FacingX) + (piece.FacingZ * piece.FacingZ));
  // Toward the paving, and the piece's left seen from the paving: leftOf the way a guest on the
  // paving looks at it.
  const double towardX = length > 0.0 ? piece.FacingX / length : 0.0;
  const double towardZ = length > 0.0 ? piece.FacingZ / length : 0.0;
  const double leftX = -towardZ;
  const double leftZ = towardX;
  const double along = seat % 2 == 0 ? SEAT_ALONG : -SEAT_ALONG;
  double across = 0.0;
  double facing = 1.0;
  if (kind == FurnitureKind::Table) {
    // The paving side's pair faces away from the paving, toward the table's middle.
    across = seat < 2 ? TABLE_SEAT_ACROSS : -TABLE_SEAT_ACROSS;
    facing = seat < 2 ? -1.0 : 1.0;
  }
  return {piece.X + (leftX * along) + (towardX * across),
          piece.Z + (leftZ * along) + (towardZ * across), towardX * facing, towardZ * facing};
}

void addFurniture(WorldSchema &schema) {
  schema.addComponent<FurniturePieceState>("furniture-piece", DataKind::Derived);
  schema.addComponent<LaidChains>("laid-furniture", DataKind::Derived);
  addField<SeatPlaces>(schema);
  schema.addResolver("furniture", layFurniture, {"walked-surface"});
}

std::vector<FurniturePiece> parkFurniture(const World &world) {
  std::vector<FurniturePiece> pieces;
  parkFurniture(world, pieces);
  return pieces;
}

void parkFurniture(const World &world, std::vector<FurniturePiece> &pieces) {
  pieces.clear();
  world.forEachWith<FurniturePieceState>(
      [&pieces](EntityKey key, const FurniturePieceState &piece) {
        pieces.push_back(FurniturePiece{key, piece.Segment, piece.Side, piece.Kind, piece.At});
      });
}

std::optional<FurniturePiece> furniturePiece(const World &world, EntityKey key) {
  const FurniturePieceState *piece = world.tryGet<FurniturePieceState>(key);
  if (piece == nullptr) {
    return std::nullopt;
  }
  return FurniturePiece{key, piece->Segment, piece->Side, piece->Kind, piece->At};
}

} // namespace tpj
