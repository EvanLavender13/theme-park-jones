#include "sim/furniture/furniture.h"

#include "sim/kept_map.h"
#include "sim/medium/field.h"
#include "sim/medium/network.h"
#include "sim/mix.h"
#include "sim/park/edits.h"
#include "sim/park/geometry.h"
#include "sim/park/surface.h"
#include "sim/park/unit_reads.h"
#include "sim/routes/networks.h"
#include "sim/tick_buffers.h"
#include "sim/units.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <compare>
#include <iterator>
#include <optional>
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
constexpr uint64_t CHAIN_PURPOSE = hashName("furniture-chain");

// One side of a segment: its segment, and whether it is the left side. Sides are ordered by
// segment key, the left side first.
struct SideKey {
  EntityKey Segment = NULL_KEY;
  bool Left = false;

  bool operator==(const SideKey &) const = default;
  std::strong_ordering operator<=>(const SideKey &other) const {
    if (const std::strong_ordering order = Segment <=> other.Segment; order != 0) {
      return order;
    }
    if (Left == other.Left) {
      return std::strong_ordering::equal;
    }
    return Left ? std::strong_ordering::less : std::strong_ordering::greater;
  }
};

// The key a chain is kept under: derived from its lowest side.
EntityKey chainKey(SideKey lowest) {
  return deriveKey(lowest.Segment, CHAIN_PURPOSE, lowest.Left ? 0 : 1);
}

// A rectangle on the ground, from its least to its greatest coordinates.
struct GroundRect {
  double MinX = 0.0;
  double MinZ = 0.0;
  double MaxX = 0.0;
  double MaxZ = 0.0;
};

// One chain furniture laid: its lowest side, which orders it among the chains, its sides in the
// order it walks them, the keys of the pieces it laid, the cells its laying reads, ascending, the
// rectangles its laying reads, a box around each side's rectangles, two hashes of the obstacles
// other than furniture whose bounds meet a rectangle, and two hashes of everything its laying
// read.
struct LaidChain {
  SideKey Order;
  std::vector<SideKey> Members;
  std::vector<EntityKey> Pieces;
  std::vector<EntityKey> Cells;
  std::vector<GroundRect> Rects;
  std::vector<GroundRect> Boxes;
  uint64_t Read = 0;
  uint64_t ReadCheck = 0;
  uint64_t Whole = 0;
  uint64_t WholeCheck = 0;
};

// Derived, on its own entity: every chain laid, by its key, the chains whose laying reads each
// cell, and the chains holding each segment's sides, each list ascending. A resolution lays again
// only the chains an edit reaches, and keeps the rest as they were. Its fields are bookkeeping
// derived from the pieces, so the walk doesn't visit them, and walks, saves, hashes, and
// comparisons never see them.
struct LaidChains {
  KeptMap<LaidChain> Chains;
  KeptMap<std::vector<EntityKey>> ByCell;
  KeptMap<std::vector<EntityKey>> BySegment;
};

template <typename Visitor> void visitFields(Visitor & /*visitor*/, LaidChains & /*laid*/) {}

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
    forEachCell(box, [this, index](size_t cell) {
      if (Cells[cell].empty()) {
        Touched.push_back(cell);
      }
      Cells[cell].push_back(index);
    });
  }

  // Removes every outline, emptying only the cells outlines were entered in.
  void clear() {
    for (const size_t cell : Touched) {
      Cells[cell].clear();
    }
    Touched.clear();
    Outlines.clear();
    Seen.clear();
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

  using Rect = GroundRect;

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

  // Calls visit with the index of each cell the rectangle reaches.
  template <typename Visit> void forEachCell(const Rect &box, Visit visit) {
    for (size_t z = cellOf(box.MinZ); z <= cellOf(box.MaxZ); ++z) {
      for (size_t x = cellOf(box.MinX); x <= cellOf(box.MaxX); ++x) {
        visit((z * CELLS) + x);
      }
    }
  }

  // Calls visit with each outline whose bounds reach the rectangle and that no call since the
  // stamp last changed has visited.
  template <typename Visit> void visitNear(const Rect &box, Visit visit) {
    forEachCell(box, [&](size_t cell) {
      for (const size_t index : Cells[cell]) {
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
  // The cells outlines were entered in since the last clear.
  std::vector<size_t> Touched;
};

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

// A chain formed in this resolution: its key, its lowest side, and its steps.
struct FormedChain {
  EntityKey Key = NULL_KEY;
  SideKey Order;
  Chain Walk;
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

// Joins the members into chains, in place of the chains formed before, keeping their lists in the
// spare list. Each chain is found from its lowest member not yet in a chain: back from it, and then
// forward from its first side. A closed chain starts at that member.
void chainsOf(const std::vector<Member> &members, std::vector<bool> &used,
              std::vector<FormedChain> &chains, std::vector<FormedChain> &spare) {
  resizeKeeping(chains, 0, spare);
  used.assign(members.size(), false);
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
    if (spare.empty()) {
      chains.emplace_back();
    } else {
      chains.push_back(std::move(spare.back()));
      spare.pop_back();
    }
    FormedChain &formed = chains.back();
    formed.Order = SideKey{members[i].Segment, members[i].Left};
    formed.Key = chainKey(formed.Order);
    Chain &chain = formed.Walk;
    chain.Steps.clear();
    chain.Steps.push_back(first);
    chain.Closed = closed;
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
  }
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

// The positions the side runs through, from its segment's paving in the surface, which holds it.
std::span<const ParkPoint> pointsOf(const PavedSurface &surface, const SegmentSide &side) {
  return sidePoints(*pavingOf(surface, side.Segment), side);
}

// The line a chain's pieces stand on, their centers' line: its sides' lines joined end to end and
// moved off the paving by the distance. Places on it are given by their length along it, which
// wraps around a closed chain. It keeps its lists from one chain to the next.
class ChainLine {
public:
  // Makes this the line of the chain, reusing its lists.
  void refill(const std::vector<Member> &members, const Chain &chain, const PavedSurface &surface,
              double distance) {
    Closed = chain.Closed;
    const Member &head = members[chain.Steps.front().Member];
    OutLeft = head.Left != chain.Steps.front().Reversed;
    std::vector<ParkPoint> &raw = Raw;
    std::vector<std::array<size_t, 3>> &rawSpans = RawSpans;
    raw.clear();
    rawSpans.clear();
    Points.clear();
    Lengths.clear();
    Spans.clear();
    for (const Step &step : chain.Steps) {
      const SegmentSide &side = *members[step.Member].Side;
      const std::span<const ParkPoint> points = pointsOf(surface, side);
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
    offsetLine(raw, distance);
    const std::vector<size_t> &kept = Kept;
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
  // that would step backward is left out. Puts in Kept, for each raw position, the index of the
  // position kept for it, or of the one kept before it.
  void offsetLine(const std::vector<ParkPoint> &raw, double distance) {
    const size_t n = raw.size();
    const auto outOf = [this](ParkPoint d) { return OutLeft ? leftOf(d) : rightOf(d); };
    std::vector<ParkPoint> &steps = Steps;
    steps.clear();
    for (size_t i = 1; i < n; ++i) {
      const ParkPoint step = minus(raw[i], raw[i - 1]);
      const double length = lengthOf(step);
      steps.push_back(length > 0.0 ? scaled(step, 1.0 / length) : ParkPoint{});
    }
    std::vector<size_t> &kept = Kept;
    kept.assign(n, 0);
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
  }

  std::vector<ParkPoint> Points;
  std::vector<double> Lengths;
  std::vector<StepSpan> Spans;
  bool Closed = false;
  bool OutLeft = false;
  // The lists refill works with: the sides' positions joined, where each step's span lies among
  // them, the unit direction of each stretch between them, and the position kept for each.
  std::vector<ParkPoint> Raw;
  std::vector<std::array<size_t, 3>> RawSpans;
  std::vector<ParkPoint> Steps;
  std::vector<size_t> Kept;
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
// where the others are tested. Puts them in places, clearing it first.
void placesOf(const ChainLine &line, double lo, double hi, std::vector<double> &places) {
  places.clear();
  places.push_back(lo);
  places.push_back(hi);
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
}

// The runs of places where a piece can stand, each from where it starts to where it ends. Each
// end of a run between a free place and a blocked one is found by halving between the two. On a
// closed line, a run through its end runs on into the run at its start. Puts them in runs,
// clearing it first, and keeps whether each place is free in free.
void runsOf(const Placing &placing, Obstacles &blocking, const std::vector<double> &places,
            bool closed, double total, std::vector<bool> &free,
            std::vector<std::pair<double, double>> &runs) {
  free.clear();
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
  runs.clear();
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

// A chain waiting to be laid: its lowest side, its key, and its place among the chains formed in
// this resolution, or NOT_FORMED when it must be formed from the chain laid before.
struct QueuedChain {
  SideKey Order;
  EntityKey Key = NULL_KEY;
  size_t Formed = 0;
};

constexpr size_t NOT_FORMED = static_cast<size_t>(-1);

// A side to walk to, and, when a linked side led there, the furnishing it must have to join.
struct SideVisit {
  SideKey Side;
  bool Linked = false;
  SideFurnishing Furnishing;
};

// A piece as it stood before this resolution laid its chain.
struct PieceBefore {
  EntityKey Key = NULL_KEY;
  FurniturePieceState State;
};

// An obstacle other than furniture near a chain: its outline, its bounds, and whether it is paving.
struct NearObstacle {
  Outline Shape;
  GroundRect Bounds;
  bool Paving = false;
};

// Every list layFurniture works with, kept from one resolution to the next so that it allocates
// nothing once they have grown.
struct FurnitureLists {
  // The segments read at a world's first resolution, and one segment read by key.
  std::vector<ParkSegment> Segments;
  ParkSegment Segment;
  // The walk over linked sides: the sides to visit and those visited, ascending.
  std::vector<SideVisit> Stack;
  std::vector<SideKey> Visited;
  // The members walked and the chains formed from them, and the same for one queued chain.
  std::vector<Member> Members;
  std::vector<bool> Used;
  std::vector<FormedChain> Formed;
  std::vector<Member> QueuedMembers;
  std::vector<FormedChain> QueuedFormed;
  std::vector<FormedChain> SpareFormed;
  // What changed: the segments and the cells, and the chains they make dirty, ascending.
  std::vector<EntityKey> ChangedSegments;
  std::vector<EntityKey> ChangedCells;
  std::vector<EntityKey> Dirty;
  std::vector<EntityKey> FormedKeys;
  std::vector<EntityKey> Removed;
  // The chains to lay, as a heap with the lowest first, the chains queued, and those laid.
  std::vector<QueuedChain> Heap;
  std::vector<EntityKey> Queued;
  std::vector<EntityKey> Done;
  // The pieces of the dirty chains before laying, ascending, and the pieces laid in this
  // resolution, ascending.
  std::vector<PieceBefore> Snapshot;
  std::vector<EntityKey> LaidPieces;
  std::vector<EntityKey> Merged;
  // One chain's laying: its reach rectangles and cells, its obstacles and what fills them, its
  // pieces before and after, and the cells a changed piece covers.
  std::vector<GroundRect> Rects;
  std::vector<GroundRect> StepBoxes;
  std::vector<NearObstacle> Near;
  std::vector<std::array<uint64_t, 2>> NearHashes;
  std::array<uint64_t, 2> NearRead{};
  std::vector<std::array<uint64_t, 2>> EarlierHashes;
  std::array<uint64_t, 2> WholeRead{};
  std::vector<EntityKey> Candidates;
  std::vector<EntityKey> ChainCells;
  Obstacles Blocking;
  std::vector<const SurfacePiece *> NearPaving;
  std::vector<const SurfacePiece *> Paving;
  std::vector<const SurfacePiece *> NearWalkways;
  std::vector<const SurfacePiece *> Walkways;
  std::vector<std::array<ParkPoint, 4>> NearStrips;
  std::vector<std::array<ParkPoint, 4>> Strips;
  std::vector<const ParkBox *> NearBoxes;
  std::vector<const ParkBox *> Boxes;
  std::vector<const ParkEntrance *> NearEntrances;
  std::vector<const ParkEntrance *> Entrances;
  std::vector<EntityKey> NearChains;
  std::vector<PieceBefore> Before;
  std::vector<EntityKey> Pieces;
  std::vector<EntityKey> SortedPieces;
  std::vector<EntityKey> PieceCells;
  // Laying one chain's line: the line, the places tested, whether each is free, the runs, and the
  // pieces a spread puts down.
  ChainLine Line;
  std::vector<double> Places;
  std::vector<bool> Free;
  std::vector<std::pair<double, double>> Runs;
  std::vector<Placed> Spread;
  // The segments whose seat places to publish again, and one segment's pieces and entries.
  std::vector<EntityKey> Republish;
  std::vector<EntityKey> SegmentPieces;
  std::vector<PlacedEntry<SeatPlace>> Entries;
  // The pieces a world held before its first resolution.
  std::vector<EntityKey> Stale;
};

bool holds(const std::vector<EntityKey> &sorted, EntityKey key) {
  return std::ranges::binary_search(sorted, key);
}

// Lays the pieces of one chain: in each run where a piece can stand, evenly spread. Each piece
// belongs to the side it stands beside, and its key comes from that side and where it stands along
// that side's segment. Puts each piece's key in keys, and adds its grown footprint to the
// obstacles. A key already laid in this resolution, by this chain or one before, moves on to the
// next.
void layChain(World &world, Obstacles &blocking, const std::vector<Member> &members,
              const Chain &chain, const PavedSurface &surface, FurnitureLists &lists,
              std::vector<EntityKey> &keys) {
  keys.clear();
  const SideFurnishing &furnishing = members[chain.Steps.front().Member].Furnishing;
  const FootprintSize size = furnitureSize(furnishing.Kind);
  ChainLine &line = lists.Line;
  line.refill(members, chain, surface, VERGE_GAP + (size.Depth / 2.0));
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
  std::vector<double> &places = lists.Places;
  placesOf(line, lo, hi, places);
  if (chain.Closed && !places.empty() && places.back() >= total) {
    places.pop_back();
  }
  runsOf(placing, blocking, places, chain.Closed, total, lists.Free, lists.Runs);
  const std::vector<std::pair<double, double>> &runs = lists.Runs;
  // A closed line free all the way around, or but for a place too short for a piece, is a ring
  // from where its one run starts.
  const bool ring = chain.Closed && runs.size() == 1 &&
                    total - (runs.front().second - runs.front().first) < size.Width + PIECE_GAP;
  std::vector<Placed> &placed = lists.Spread;
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
      while (holds(lists.LaidPieces, key) || std::ranges::find(keys, key) != keys.end()) {
        key = world.createDerivedEntity(member.Segment, PIECE_PURPOSE, ++index);
      }
      world.emplaceOrReplace<FurniturePieceState>(
          key, FurniturePieceState{member.Segment,
                                   member.Left ? FurnishedSide::Left : FurnishedSide::Right,
                                   furnishing.Kind, piece.At});
      blocking.add(piece.Grown, false);
      keys.push_back(key);
    }
  }
}

// The side of the segment the surface gives, or none.
const SegmentSide *sideOf(const PavedSurface &surface, EntityKey segment, bool left) {
  const OwnerPaving *paving = pavingOf(surface, segment);
  if (paving == nullptr) {
    return nullptr;
  }
  const auto found = std::ranges::find(paving->Sides, left, &SegmentSide::Left);
  return found != paving->Sides.end() ? &*found : nullptr;
}

// The side of the segment as a member, when the segment is a furnished guest segment between two
// points apart, the side is furnished, and the surface gives it.
std::optional<Member> memberOf(const World &world, const PavedSurface &surface,
                               const ParkSegment &segment, bool left) {
  if (!isFurnished(segment)) {
    return std::nullopt;
  }
  const std::optional<ParkPoint> from = pathPointAt(world, segment.From);
  const std::optional<ParkPoint> to = pathPointAt(world, segment.To);
  if (!from || !to) {
    return std::nullopt;
  }
  const ParkPoint span = minus(*to, *from);
  const double spanLength = lengthOf(span);
  if (!(spanLength > 0.0)) {
    return std::nullopt;
  }
  const std::optional<SideFurnishing> &furnishing =
      left ? segment.Furnished.Left : segment.Furnished.Right;
  const SegmentSide *side = sideOf(surface, segment.Key, left);
  if (!furnishing || side == nullptr) {
    return std::nullopt;
  }
  return Member{segment.Key, left, *furnishing, side, *from, scaled(span, 1.0 / spanLength)};
}

SideKey sideKeyOf(const Member &member) { return SideKey{member.Segment, member.Left}; }

// Adds the chain to the dirty chains, when it isn't one, and puts its sides on the walk.
void markDirty(const LaidChains &laid, EntityKey key, FurnitureLists &lists) {
  const auto at = std::ranges::lower_bound(lists.Dirty, key);
  if (at != lists.Dirty.end() && *at == key) {
    return;
  }
  lists.Dirty.insert(at, key);
  for (const SideKey &side : laid.Chains.find(key)->Members) {
    lists.Stack.push_back(SideVisit{side, false, {}});
  }
}

// Puts in members, clearing it first, every member joined to a side on the walk's stack through
// linked sides with the same furnishing, and each side on the stack that a link didn't lead to, in
// key order of their segment, the left side first. When laid is given, each chain holding a side
// walked is dirty, and its sides are walked too.
void walkMembers(const World &world, const PavedSurface &surface, FurnitureLists &lists,
                 const LaidChains *laid, std::vector<Member> &members) {
  members.clear();
  lists.Visited.clear();
  while (!lists.Stack.empty()) {
    const SideVisit visit = lists.Stack.back();
    lists.Stack.pop_back();
    const auto at = std::ranges::lower_bound(lists.Visited, visit.Side);
    if (at != lists.Visited.end() && *at == visit.Side) {
      continue;
    }
    Member walked;
    bool furnished = false;
    if (parkSegment(world, visit.Side.Segment, lists.Segment)) {
      if (const std::optional<Member> member =
              memberOf(world, surface, lists.Segment, visit.Side.Left)) {
        walked = *member;
        furnished = true;
      }
    }
    if (visit.Linked && (!furnished || !(walked.Furnishing == visit.Furnishing))) {
      continue;
    }
    lists.Visited.insert(at, visit.Side);
    if (laid != nullptr) {
      if (const std::vector<EntityKey> *chains = laid->BySegment.find(visit.Side.Segment)) {
        for (const EntityKey key : *chains) {
          const std::vector<SideKey> &sides = laid->Chains.find(key)->Members;
          if (std::ranges::find(sides, visit.Side) != sides.end()) {
            markDirty(*laid, key, lists);
          }
        }
      }
    }
    if (!furnished) {
      continue;
    }
    members.push_back(walked);
    for (const SideLink &link : walked.Side->RunsOn) {
      if (link.Segment != NULL_KEY) {
        lists.Stack.push_back(SideVisit{SideKey{link.Segment, link.Left}, true, walked.Furnishing});
      }
    }
  }
  std::ranges::sort(members, {}, sideKeyOf);
}

// The farthest from a side's line any test of a piece of the kind reaches: its line's greatest
// offset, its greatest setback, and its grown footprint around its center, with a margin.
double reachOf(FurnitureKind kind) {
  const FootprintSize size = furnitureSize(kind);
  return (2.0 * (VERGE_GAP + (size.Depth / 2.0))) + MAX_SETBACK +
         lengthOf(ParkPoint{size.Width / 2.0, size.Depth / 2.0}) + CLEARANCE + SAMPLE_STEP;
}

// Puts in the lists the rectangles around each stretch of the chain's sides that its laying reads,
// and the cells they cover, ascending.
void reachOfChain(const std::vector<Member> &members, const Chain &chain,
                  const PavedSurface &surface, FurnitureLists &lists) {
  const double reach = reachOf(members[chain.Steps.front().Member].Furnishing.Kind);
  lists.Rects.clear();
  lists.StepBoxes.clear();
  lists.ChainCells.clear();
  for (const Step &step : chain.Steps) {
    const std::span<const ParkPoint> points = pointsOf(surface, *members[step.Member].Side);
    for (size_t k = 0; k < points.size(); ++k) {
      const ParkPoint previous = points[k > 0 ? k - 1 : k];
      const GroundRect rect{
          std::min(previous.X, points[k].X) - reach, std::min(previous.Z, points[k].Z) - reach,
          std::max(previous.X, points[k].X) + reach, std::max(previous.Z, points[k].Z) + reach};
      lists.Rects.push_back(rect);
      pavingCells({rect.MinX, rect.MinZ}, {rect.MaxX, rect.MaxZ}, lists.ChainCells);
      if (k == 0) {
        lists.StepBoxes.push_back(rect);
      }
      GroundRect &box = lists.StepBoxes.back();
      box = GroundRect{std::min(box.MinX, rect.MinX), std::min(box.MinZ, rect.MinZ),
                       std::max(box.MaxX, rect.MaxX), std::max(box.MaxZ, rect.MaxZ)};
    }
  }
  sortUnique(lists.ChainCells);
}

// The piece's footprint grown by CLEARANCE, as layChain adds it to the obstacles, or none.
std::optional<Outline> grownPiece(const FurniturePieceState &piece) {
  const std::optional<Footprint> footprint = footprintOf(piece.At, furnitureSize(piece.Kind));
  if (!footprint) {
    return std::nullopt;
  }
  return grownOf(*footprint, CLEARANCE);
}

// Appends the near list to the list of all.
template <typename T> void appendTo(std::vector<T> &all, const std::vector<T> &near) {
  all.insert(all.end(), near.begin(), near.end());
}

// Sorts the list and leaves out repeats.
template <typename T> void uniqueSorted(std::vector<T> &list) {
  std::ranges::sort(list);
  list.erase(std::unique(list.begin(), list.end()), list.end());
}

// The rectangle that holds the outline.
GroundRect boundsOfOutline(const Outline &outline) {
  GroundRect box{outline.Corners[0].X, outline.Corners[0].Z, outline.Corners[0].X,
                 outline.Corners[0].Z};
  for (size_t i = 1; i < outline.Count; ++i) {
    box.MinX = std::min(box.MinX, outline.Corners[i].X);
    box.MaxX = std::max(box.MaxX, outline.Corners[i].X);
    box.MinZ = std::min(box.MinZ, outline.Corners[i].Z);
    box.MaxZ = std::max(box.MaxZ, outline.Corners[i].Z);
  }
  return box;
}

// Two hashes of an obstacle: whether it is paving, and its corners.
std::array<uint64_t, 2> outlineHash(const Outline &shape, bool paving) {
  std::array<Hasher, 2> hashers;
  hashers[1].add(1);
  for (Hasher &hasher : hashers) {
    hasher.add(paving ? 1U : 0U);
    hasher.add(shape.Count);
    for (size_t i = 0; i < shape.Count; ++i) {
      hasher.add(std::bit_cast<uint64_t>(shape.Corners[i].X));
      hasher.add(std::bit_cast<uint64_t>(shape.Corners[i].Z));
    }
  }
  return {hashers[0].value(), hashers[1].value()};
}

// Two hashes of a set of obstacles' hashes, which it sorts, so they don't depend on the order the
// obstacles were read in or where they lie in memory.
std::array<uint64_t, 2> setHash(std::vector<std::array<uint64_t, 2>> &hashes) {
  uniqueSorted(hashes);
  std::array<Hasher, 2> read;
  read[1].add(1);
  for (const std::array<uint64_t, 2> &hash : hashes) {
    read[0].add(hash[0]);
    read[1].add(hash[1]);
  }
  return {read[0].value(), read[1].value()};
}

// Whether the bounds meet one of the chain's rectangles.
bool meetsReach(const GroundRect &bounds, const FurnitureLists &lists) {
  return std::ranges::any_of(lists.Rects, [&bounds](const GroundRect &rect) {
    return bounds.MinX <= rect.MaxX && bounds.MaxX >= rect.MinX && bounds.MinZ <= rect.MaxZ &&
           bounds.MaxZ >= rect.MinZ;
  });
}

// Adds the outline to the near obstacles when its bounds meet one of the chain's rectangles.
void keepNear(const Outline &outline, bool paving, FurnitureLists &lists) {
  const GroundRect bounds = boundsOfOutline(outline);
  if (meetsReach(bounds, lists)) {
    lists.Near.push_back(NearObstacle{outline, bounds, paving});
  }
}

// Gathers the pointers the near read gives for each of the chain's boxes into the list of all,
// once each.
void gatherPointers(const World &world, FurnitureLists &lists, TickBuffers &buffers) {
  lists.Paving.clear();
  lists.Walkways.clear();
  lists.Strips.clear();
  lists.Boxes.clear();
  lists.Entrances.clear();
  for (const GroundRect &box : lists.StepBoxes) {
    const ParkPoint least{box.MinX, box.MinZ};
    const ParkPoint most{box.MaxX, box.MaxZ};
    pavingNear(world, least, most, lists.NearPaving, buffers);
    appendTo(lists.Paving, lists.NearPaving);
    walkwaysNear(world, least, most, lists.NearWalkways, lists.NearStrips, buffers);
    appendTo(lists.Walkways, lists.NearWalkways);
    appendTo(lists.Strips, lists.NearStrips);
    boxesNear(world, least, most, lists.NearBoxes, buffers);
    appendTo(lists.Boxes, lists.NearBoxes);
    entrancesNear(world, least, most, lists.NearEntrances, buffers);
    appendTo(lists.Entrances, lists.NearEntrances);
  }
  uniqueSorted(lists.Paving);
  uniqueSorted(lists.Walkways);
  uniqueSorted(lists.Boxes);
  uniqueSorted(lists.Entrances);
}

// Puts in the near obstacles the paving of either kind, walkways, frontage strips, boxes, and
// entrances whose bounds meet one of the chain's rectangles, and puts in NearRead two hashes of
// them that don't depend on where they lie in memory or the order they were read in.
void gatherNear(const World &world, FurnitureLists &lists, TickBuffers &buffers) {
  gatherPointers(world, lists, buffers);
  lists.Near.clear();
  for (const std::array<ParkPoint, 4> &strip : lists.Strips) {
    keepNear(Outline{strip, 4}, false, lists);
  }
  for (const SurfacePiece *piece : lists.Paving) {
    keepNear(Outline{piece->Corners, piece->Count}, true, lists);
  }
  for (const SurfacePiece *piece : lists.Walkways) {
    keepNear(Outline{piece->Corners, piece->Count}, true, lists);
  }
  for (const ParkBox *box : lists.Boxes) {
    if (const std::optional<Footprint> footprint = footprintOf(box->At, boxSize(box->Kind))) {
      keepNear(outlineOf(*footprint), false, lists);
    }
  }
  for (const ParkEntrance *entrance : lists.Entrances) {
    if (const std::optional<Footprint> footprint = footprintOf(entrance->At, ENTRANCE_SIZE)) {
      keepNear(outlineOf(*footprint), false, lists);
    }
  }
  lists.NearHashes.clear();
  for (const NearObstacle &near : lists.Near) {
    lists.NearHashes.push_back(outlineHash(near.Shape, near.Paving));
  }
  lists.NearRead = setHash(lists.NearHashes);
}

// Whether the obstacles other than furniture that the chain's rectangles reach differ from those
// it was laid against.
bool nearChanged(const World &world, const LaidChain &chain, FurnitureLists &lists,
                 TickBuffers &buffers) {
  copyGrowing(lists.Rects, std::span<const GroundRect>(chain.Rects));
  copyGrowing(lists.StepBoxes, std::span<const GroundRect>(chain.Boxes));
  gatherNear(world, lists, buffers);
  return lists.NearRead[0] != chain.Read || lists.NearRead[1] != chain.ReadCheck;
}

// Adds to the obstacles the pieces of the chains before the order filed in the chain's cells.
void addEarlierPieces(const World &world, const LaidChains &laid, SideKey order,
                      FurnitureLists &lists) {
  lists.EarlierHashes.clear();
  lists.NearChains.clear();
  for (const EntityKey cell : lists.ChainCells) {
    if (const std::vector<EntityKey> *chains = laid.ByCell.find(cell)) {
      appendTo(lists.NearChains, *chains);
    }
  }
  sortUnique(lists.NearChains);
  for (const EntityKey key : lists.NearChains) {
    const LaidChain &chain = *laid.Chains.find(key);
    if (!(chain.Order < order)) {
      continue;
    }
    for (const EntityKey piece : chain.Pieces) {
      if (const FurniturePieceState *state = world.tryGet<FurniturePieceState>(piece)) {
        if (const std::optional<Outline> grown = grownPiece(*state)) {
          lists.Blocking.add(*grown, false);
          if (meetsReach(boundsOfOutline(*grown), lists)) {
            lists.EarlierHashes.push_back(outlineHash(*grown, false));
          }
        }
      }
    }
  }
}

// Fills the obstacles for laying the chain from what its rectangles reach: paving of either kind,
// walkways, frontage strips, boxes, entrances, and the pieces of the chains before it filed in its
// cells.
void fillObstacles(const World &world, const LaidChains &laid, SideKey order, FurnitureLists &lists,
                   TickBuffers &buffers) {
  Obstacles &blocking = lists.Blocking;
  blocking.clear();
  gatherNear(world, lists, buffers);
  for (const NearObstacle &near : lists.Near) {
    blocking.add(near.Shape, near.Paving);
  }
  addEarlierPieces(world, laid, order, lists);
}

// Queues each chain after the order whose laying reads a cell the piece's grown footprint covers,
// unless it is queued, laid, or removed in this resolution.
void queueNear(const LaidChains &laid, const FurniturePieceState &piece, SideKey order,
               FurnitureLists &lists) {
  const std::optional<Outline> grown = grownPiece(piece);
  if (!grown) {
    return;
  }
  ParkPoint least = grown->Corners[0];
  ParkPoint most = grown->Corners[0];
  for (size_t i = 1; i < grown->Count; ++i) {
    least = {std::min(least.X, grown->Corners[i].X), std::min(least.Z, grown->Corners[i].Z)};
    most = {std::max(most.X, grown->Corners[i].X), std::max(most.Z, grown->Corners[i].Z)};
  }
  lists.PieceCells.clear();
  pavingCells(least, most, lists.PieceCells);
  const auto later = [](const QueuedChain &a, const QueuedChain &b) { return b.Order < a.Order; };
  for (const EntityKey cell : lists.PieceCells) {
    const std::vector<EntityKey> *chains = laid.ByCell.find(cell);
    if (chains == nullptr) {
      continue;
    }
    for (const EntityKey key : *chains) {
      const LaidChain &chain = *laid.Chains.find(key);
      if (!(order < chain.Order) || holds(lists.Done, key) || holds(lists.Removed, key)) {
        continue;
      }
      const auto at = std::ranges::lower_bound(lists.Queued, key);
      if (at != lists.Queued.end() && *at == key) {
        continue;
      }
      lists.Queued.insert(at, key);
      lists.Heap.push_back(QueuedChain{chain.Order, key, NOT_FORMED});
      std::ranges::push_heap(lists.Heap, later);
    }
  }
}

// Removes the key from each list the map files under the keys, and each list it empties.
void unfile(KeptMap<std::vector<EntityKey>> &map, std::span<const EntityKey> under, EntityKey key) {
  for (const EntityKey at : under) {
    if (std::vector<EntityKey> *filed = map.find(at)) {
      eraseSorted(*filed, key);
      if (filed->empty()) {
        map.erase(at);
      }
    }
  }
}

// Adds the key to the list the map files under each of the keys.
void file(KeptMap<std::vector<EntityKey>> &map, std::span<const EntityKey> under, EntityKey key) {
  for (const EntityKey at : under) {
    auto [filed, added] = map.getOrAdd(at);
    if (added) {
      filed.clear();
    }
    insertSorted(filed, key);
  }
}

// Removes the chain's entries from the cell and segment maps, and adds its segments to those whose
// seat places are published again.
void unfileChain(LaidChains &laid, const LaidChain &chain, EntityKey key, FurnitureLists &lists) {
  unfile(laid.ByCell, chain.Cells, key);
  for (const SideKey &side : chain.Members) {
    unfile(laid.BySegment, std::span<const EntityKey>(&side.Segment, 1), key);
    lists.Republish.push_back(side.Segment);
  }
}

// The piece's state before this resolution laid anything, when it stood then.
const FurniturePieceState *stateBefore(const World &world, const FurnitureLists &lists,
                                       EntityKey key) {
  const auto at = std::ranges::lower_bound(lists.Snapshot, key, {}, &PieceBefore::Key);
  if (at != lists.Snapshot.end() && at->Key == key) {
    return &at->State;
  }
  return world.tryGet<FurniturePieceState>(key);
}

// Keeps the chain as just laid, in place of how it was laid before, filed under its cells and its
// segments, whose seat places are published again.
void keepChain(LaidChains &laid, const FormedChain &formed, const std::vector<Member> &members,
               FurnitureLists &lists) {
  auto [chain, added] = laid.Chains.getOrAdd(formed.Key);
  if (!added) {
    unfileChain(laid, chain, formed.Key, lists);
  }
  chain.Order = formed.Order;
  chain.Members.clear();
  for (const Step &step : formed.Walk.Steps) {
    chain.Members.push_back(sideKeyOf(members[step.Member]));
  }
  copyGrowing(chain.Pieces, std::span<const EntityKey>(lists.Pieces));
  copyGrowing(chain.Cells, std::span<const EntityKey>(lists.ChainCells));
  copyGrowing(chain.Rects, std::span<const GroundRect>(lists.Rects));
  copyGrowing(chain.Boxes, std::span<const GroundRect>(lists.StepBoxes));
  chain.Read = lists.NearRead[0];
  chain.ReadCheck = lists.NearRead[1];
  chain.Whole = lists.WholeRead[0];
  chain.WholeCheck = lists.WholeRead[1];
  file(laid.ByCell, chain.Cells, formed.Key);
  for (const SideKey &side : chain.Members) {
    file(laid.BySegment, std::span<const EntityKey>(&side.Segment, 1), formed.Key);
    lists.Republish.push_back(side.Segment);
  }
}

// Two hashes of everything laying the chain reads: its furnishing, each of its sides with its line
// and its segment's stations, and the obstacles within reach, furniture laid before it included.
std::array<uint64_t, 2> wholeRead(const std::vector<Member> &members, const Chain &chain,
                                  const PavedSurface &surface, FurnitureLists &lists) {
  const SideFurnishing &furnishing = members[chain.Steps.front().Member].Furnishing;
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
    const std::span<const ParkPoint> points = pointsOf(surface, side);
    add(points.size());
    for (const ParkPoint point : points) {
      addPoint(point);
    }
  }
  add(lists.NearRead[0]);
  add(lists.NearRead[1]);
  const std::array<uint64_t, 2> earlier = setHash(lists.EarlierHashes);
  add(earlier[0]);
  add(earlier[1]);
  return {hashers[0].value(), hashers[1].value()};
}

// Whether the chain was laid before from the same reads and its pieces still stand as it laid
// them, which laying it again would give. Then its pieces count as laid in this resolution.
bool keptAsLaid(const World &world, const LaidChains &laid, const FormedChain &formed,
                FurnitureLists &lists) {
  const LaidChain *before = laid.Chains.find(formed.Key);
  if (before == nullptr || before->Whole != lists.WholeRead[0] ||
      before->WholeCheck != lists.WholeRead[1]) {
    return false;
  }
  const bool standing = std::ranges::all_of(before->Pieces, [&](EntityKey key) {
    return !holds(lists.LaidPieces, key) && world.has<FurniturePieceState>(key);
  });
  if (!standing) {
    return false;
  }
  copyGrowing(lists.Pieces, std::span<const EntityKey>(before->Pieces));
  copyGrowing(lists.SortedPieces, std::span<const EntityKey>(before->Pieces));
  std::ranges::sort(lists.SortedPieces);
  lists.Merged.clear();
  std::ranges::merge(lists.LaidPieces, lists.SortedPieces, std::back_inserter(lists.Merged));
  lists.LaidPieces.swap(lists.Merged);
  return true;
}

// Lays one chain against the obstacles its cells hold, keeps it, destroys the pieces it laid
// before and doesn't now, and queues the later chains that each changed piece reaches.
void layOne(World &world, LaidChains &laid, const FormedChain &formed,
            const std::vector<Member> &members, const PavedSurface &surface, FurnitureLists &lists,
            TickBuffers &buffers) {
  reachOfChain(members, formed.Walk, surface, lists);
  fillObstacles(world, laid, formed.Order, lists, buffers);
  lists.WholeRead = wholeRead(members, formed.Walk, surface, lists);
  if (keptAsLaid(world, laid, formed, lists)) {
    keepChain(laid, formed, members, lists);
    return;
  }
  lists.Before.clear();
  if (const LaidChain *before = laid.Chains.find(formed.Key)) {
    for (const EntityKey key : before->Pieces) {
      if (const FurniturePieceState *state = stateBefore(world, lists, key)) {
        lists.Before.push_back(PieceBefore{key, *state});
      }
    }
  }
  std::ranges::sort(lists.Before, {}, &PieceBefore::Key);
  layChain(world, lists.Blocking, members, formed.Walk, surface, lists, lists.Pieces);
  copyGrowing(lists.SortedPieces, std::span<const EntityKey>(lists.Pieces));
  std::ranges::sort(lists.SortedPieces);
  for (const EntityKey key : lists.Pieces) {
    const FurniturePieceState &state = world.get<FurniturePieceState>(key);
    const auto old = std::ranges::lower_bound(lists.Before, key, {}, &PieceBefore::Key);
    if (old == lists.Before.end() || old->Key != key || !(old->State == state)) {
      queueNear(laid, state, formed.Order, lists);
    }
  }
  for (const PieceBefore &old : lists.Before) {
    const bool kept = holds(lists.SortedPieces, old.Key);
    if (kept && world.get<FurniturePieceState>(old.Key) == old.State) {
      continue;
    }
    queueNear(laid, old.State, formed.Order, lists);
    if (!kept && !holds(lists.LaidPieces, old.Key)) {
      world.destroyEntity(old.Key);
    }
  }
  lists.Merged.clear();
  std::ranges::merge(lists.LaidPieces, lists.SortedPieces, std::back_inserter(lists.Merged));
  lists.LaidPieces.swap(lists.Merged);
  keepChain(laid, formed, members, lists);
}

// Forms a queued chain again from the sides it holds, which didn't change, into the queued lists.
const FormedChain &formQueued(const World &world, const LaidChains &laid, const QueuedChain &queued,
                              const PavedSurface &surface, FurnitureLists &lists) {
  for (const SideKey &side : laid.Chains.find(queued.Key)->Members) {
    lists.Stack.push_back(SideVisit{side, false, {}});
  }
  walkMembers(world, surface, lists, nullptr, lists.QueuedMembers);
  // Formed chains keep their lists in the shared spare list, so the queued chain's take it back.
  std::vector<FormedChain> &formed = lists.QueuedFormed;
  chainsOf(lists.QueuedMembers, lists.Used, formed, lists.SpareFormed);
  const auto found = std::ranges::find(formed, queued.Key, &FormedChain::Key);
  if (found == formed.end()) {
    throw std::logic_error("furniture: a chain an obstacle queued no longer forms");
  }
  return *found;
}

// Publishes again each segment's seat places, at the place on the segment's guest carrier nearest
// each piece, in key order of their pieces, or none when it has no such pieces.
void republishSeatPlaces(World &world, const LaidChains &laid, FurnitureLists &lists) {
  const Network &network = parkNetwork(world, PathKind::Guest);
  sortUnique(lists.Republish);
  for (const EntityKey segment : lists.Republish) {
    lists.SegmentPieces.clear();
    if (const std::vector<EntityKey> *chains = laid.BySegment.find(segment)) {
      for (const EntityKey key : *chains) {
        for (const EntityKey piece : laid.Chains.find(key)->Pieces) {
          const FurniturePieceState *state = world.tryGet<FurniturePieceState>(piece);
          if (state != nullptr && state->Segment == segment) {
            lists.SegmentPieces.push_back(piece);
          }
        }
      }
    }
    std::ranges::sort(lists.SegmentPieces);
    lists.Entries.clear();
    for (const EntityKey key : lists.SegmentPieces) {
      const FurniturePieceState &piece = world.get<FurniturePieceState>(key);
      const std::optional<Place> at =
          network.nearestPlaceOn(segment, GroundPoint{piece.At.X, piece.At.Z});
      if (at) {
        lists.Entries.push_back(PlacedEntry<SeatPlace>{
            *at, SeatPlace{key, piece.At, piece.Kind, furnitureSeats(piece.Kind), piece.Segment}});
      }
    }
    if (lists.Entries.empty()) {
      dropResolved<SeatPlaces>(world, segment);
      continue;
    }
    std::vector<PlacedEntry<SeatPlace>> &entries = editResolved<SeatPlaces>(world, segment);
    copyGrowing(entries, std::span<const PlacedEntry<SeatPlace>>(lists.Entries));
  }
}

// Finds the chains to lay at a world's first resolution: every furnished side.
void formEveryChain(const World &world, const PavedSurface &surface, FurnitureLists &lists,
                    TickBuffers &buffers) {
  parkSegments(world, lists.Segments, buffers);
  lists.Members.clear();
  for (const ParkSegment &segment : lists.Segments) {
    for (const bool left : {true, false}) {
      if (const std::optional<Member> member = memberOf(world, surface, segment, left)) {
        lists.Members.push_back(*member);
      }
    }
  }
}

// Finds the chains to lay after a world's first resolution: the chains holding a side of a segment
// that changed, the chains whose obstacles within reach changed, every chain holding a side joined
// to theirs, and the chains new furnishing forms.
void formDirtyChains(const World &world, const LaidChains &laid, const PavedSurface &surface,
                     FurnitureLists &lists, TickBuffers &buffers) {
  lists.ChangedCells.clear();
  changedParkCells(world, lists.ChangedCells, buffers);
  changedWalkwayCells(world, lists.ChangedCells, buffers);
  lists.Candidates.clear();
  for (const EntityKey cell : lists.ChangedCells) {
    if (const std::vector<EntityKey> *chains = laid.ByCell.find(cell)) {
      appendTo(lists.Candidates, *chains);
    }
  }
  sortUnique(lists.Candidates);
  // A cell is coarser than a chain's reach, so a chain in a changed cell is dirty only when the
  // obstacles it reaches changed.
  for (const EntityKey key : lists.Candidates) {
    if (nearChanged(world, *laid.Chains.find(key), lists, buffers)) {
      markDirty(laid, key, lists);
    }
  }
  for (const EntityKey segment : lists.ChangedSegments) {
    lists.Stack.push_back(SideVisit{SideKey{segment, true}, false, {}});
    lists.Stack.push_back(SideVisit{SideKey{segment, false}, false, {}});
  }
  walkMembers(world, surface, lists, &laid, lists.Members);
}

// Queues every formed chain, and empties the lists of chains laid and pieces laid.
void queueFormed(FurnitureLists &lists) {
  lists.Heap.clear();
  lists.Done.clear();
  lists.LaidPieces.clear();
  for (size_t i = 0; i < lists.Formed.size(); ++i) {
    lists.Heap.push_back(QueuedChain{lists.Formed[i].Order, lists.Formed[i].Key, i});
  }
  std::ranges::make_heap(
      lists.Heap, [](const QueuedChain &a, const QueuedChain &b) { return b.Order < a.Order; });
  copyGrowing(lists.Queued, std::span<const EntityKey>(lists.FormedKeys));
}

// Keeps each dirty chain's pieces as they stood, since a chain laid before it can take one of its
// sides and lay a piece with the same key.
void snapshotDirty(const World &world, const LaidChains &laid, FurnitureLists &lists) {
  lists.Snapshot.clear();
  for (const EntityKey key : lists.Dirty) {
    if (holds(lists.Removed, key)) {
      continue;
    }
    for (const EntityKey piece : laid.Chains.find(key)->Pieces) {
      if (const FurniturePieceState *state = world.tryGet<FurniturePieceState>(piece)) {
        lists.Snapshot.push_back(PieceBefore{piece, *state});
      }
    }
  }
  std::ranges::sort(lists.Snapshot, {}, &PieceBefore::Key);
}

// Removes each removed chain with its pieces. A removed chain is never laid, so the later chains
// its pieces reached are queued before any chain is laid.
void removeChains(World &world, LaidChains &laid, FurnitureLists &lists) {
  for (const EntityKey key : lists.Removed) {
    const LaidChain &chain = *laid.Chains.find(key);
    for (const EntityKey piece : chain.Pieces) {
      if (const FurniturePieceState *state = world.tryGet<FurniturePieceState>(piece)) {
        queueNear(laid, *state, chain.Order, lists);
      }
    }
  }
  for (const EntityKey key : lists.Removed) {
    const LaidChain &chain = *laid.Chains.find(key);
    for (const EntityKey piece : chain.Pieces) {
      world.destroyEntity(piece);
    }
    unfileChain(laid, chain, key, lists);
    laid.Chains.erase(key);
  }
}

// Lays the pieces each furnished guest segment asks for, chain by chain in order of their lowest
// sides, laying again only the chains an edit reaches: those whose sides changed, those whose
// obstacles within reach changed, and the later chains a changed piece reaches. The rest keep their
// pieces, which are what laying them again would give.
void layFurniture(World &world, TickBuffers &buffers) {
  FurnitureLists &lists = buffers.get<FurnitureLists>();
  const EntityKey laidKey = deriveKey(NULL_KEY, LAID_PURPOSE, 0);
  const bool first = !world.has<LaidChains>(laidKey);
  LaidChains &laid =
      world.getOrEmplace<LaidChains>(world.createDerivedEntity(NULL_KEY, LAID_PURPOSE, 0));
  const PavedSurface &surface = walkedSurface(world);
  lists.Stack.clear();
  lists.Dirty.clear();
  lists.ChangedSegments.clear();
  lists.Republish.clear();
  if (first) {
    world.keysWith<FurniturePieceState>(lists.Stale);
    formEveryChain(world, surface, lists, buffers);
  } else {
    changedSegments(world, lists.ChangedSegments);
    formDirtyChains(world, laid, surface, lists, buffers);
  }
  chainsOf(lists.Members, lists.Used, lists.Formed, lists.SpareFormed);
  lists.FormedKeys.clear();
  for (const FormedChain &formed : lists.Formed) {
    lists.FormedKeys.push_back(formed.Key);
  }
  std::ranges::sort(lists.FormedKeys);
  lists.Removed.clear();
  for (const EntityKey key : lists.Dirty) {
    if (!holds(lists.FormedKeys, key)) {
      lists.Removed.push_back(key);
    }
  }
  queueFormed(lists);
  snapshotDirty(world, laid, lists);
  removeChains(world, laid, lists);
  const auto later = [](const QueuedChain &a, const QueuedChain &b) { return b.Order < a.Order; };
  while (!lists.Heap.empty()) {
    std::ranges::pop_heap(lists.Heap, later);
    const QueuedChain queued = lists.Heap.back();
    lists.Heap.pop_back();
    insertSorted(lists.Done, queued.Key);
    if (queued.Formed != NOT_FORMED) {
      layOne(world, laid, lists.Formed[queued.Formed], lists.Members, surface, lists, buffers);
    } else {
      const FormedChain &formed = formQueued(world, laid, queued, surface, lists);
      layOne(world, laid, formed, lists.QueuedMembers, surface, lists, buffers);
    }
  }
  if (first) {
    for (const EntityKey key : lists.Stale) {
      if (!holds(lists.LaidPieces, key)) {
        world.destroyEntity(key);
      }
    }
  }
  appendTo(lists.Republish, lists.ChangedSegments);
  republishSeatPlaces(world, laid, lists);
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
