#ifndef TPJ_SIM_PARK_INTERNAL_GROUND_UNITS_H
#define TPJ_SIM_PARK_INTERNAL_GROUND_UNITS_H

#include "sim/entity_key.h"
#include "sim/park/intent.h"
#include "sim/park/internal/components.h"
#include "sim/park/internal/layout.h"
#include "sim/units.h"
#include "sim/world.h"

#include <array>
#include <span>
#include <string_view>
#include <tuple>
#include <vector>

namespace tpj {

// Files each segment under its two points, once when they are one point.
struct SegmentEnds {
  static constexpr std::string_view Name = "segment-ends";
  using Over = PathSegmentIntent;

  static void keysOf(const PathSegmentIntent &segment, std::vector<EntityKey> &keys) {
    keys.clear();
    keys.push_back(segment.From);
    if (segment.To != segment.From) {
      keys.push_back(segment.To);
    }
  }
};

// A segment's straight ground line.
struct StraightLine {
  std::vector<CarrierPoint> Points;
};

template <typename Visitor> void visitFields(Visitor &visitor, StraightLine &line) {
  visitor.field("points", line.Points);
}

struct StraightLines {
  static constexpr std::string_view Name = "straight-lines";
  using Over = PathSegmentIntent;
  using Value = StraightLine;
  using Reads = std::tuple<PathPointIntent>;

  static void derive(UnitReads &reads, EntityKey key, StraightLine &line) {
    const PathSegmentIntent &segment = *reads.component<PathSegmentIntent>(key);
    straightLineOf(segment, reads.component<PathPointIntent>(segment.From),
                   reads.component<PathPointIntent>(segment.To), line.Points);
  }
};

// The segment ends at one point, which deriving its arms reuses.
struct ArmInputs {
  std::vector<ArmInput> Ends;
};

struct PointArmsUnits {
  static constexpr std::string_view Name = "point-arms";
  using Over = PathPointIntent;
  using Value = PointArms;
  using Reads = std::tuple<PathSegmentIntent>;

  static void derive(UnitReads &reads, EntityKey key, PointArms &arms) {
    const PathPointIntent &point = *reads.component<PathPointIntent>(key);
    std::vector<ArmInput> &ends = reads.scratch().get<ArmInputs>().Ends;
    ends.clear();
    for (const EntityKey segmentKey : reads.filed<SegmentEnds>(key)) {
      const PathSegmentIntent &segment = *reads.component<PathSegmentIntent>(segmentKey);
      const StraightLine &straight = *reads.unit<StraightLines>(segmentKey);
      const EntityKey far = segment.From == key ? segment.To : segment.From;
      const ArmInput end{segmentKey, &segment, !straight.Points.empty(),
                         reads.component<PathPointIntent>(far)};
      // A segment with both ends at the point has two ends there.
      ends.push_back(end);
      if (segment.From == segment.To) {
        ends.push_back(end);
      }
    }
    armsAt(key, point, ends, reads.scratch(), arms);
  }
};

// The corner at a point, when it has one. Without one, Round holds no values.
struct Corner {
  bool Has = false;
  Rounding Round;
};

template <typename Visitor> void visitFields(Visitor &visitor, Corner &corner) {
  visitor.field("has", corner.Has);
  visitor.field("round", corner.Round);
}

struct Corners {
  static constexpr std::string_view Name = "corners";
  using Over = PathPointIntent;
  using Value = Corner;
  using Reads = std::tuple<>;

  static void derive(UnitReads &reads, EntityKey key, Corner &corner) {
    const PointArms &arms = *reads.unit<PointArmsUnits>(key);
    corner.Has = arms.HasBend;
    if (!corner.Has) {
      corner.Round.Corner = ParkCorner{};
      corner.Round.Segments = {};
      for (std::vector<ParkPoint> &half : corner.Round.Halves) {
        half.clear();
      }
      return;
    }
    std::array<double, 2> needed{};
    for (size_t k = 0; k < 2; ++k) {
      needed[k] =
          neededAt(*reads.unit<PointArmsUnits>(arms.Bent.FarKeys[k]), arms.Bent.Segments[k]);
    }
    cornerAt(arms.Bent, needed, corner.Round);
  }
};

// A segment with its ground line. Its furnishing is always empty.
struct GroundLine {
  ParkSegment Segment;
};

template <typename Visitor> void visitFields(Visitor &visitor, GroundLine &line) {
  ParkSegment &segment = line.Segment;
  visitor.field("key", segment.Key);
  visitor.field("kind", segment.Kind);
  visitor.field("from", segment.From);
  visitor.field("to", segment.To);
  visitor.field("width", segment.Width);
  visitor.field("line", segment.Line);
  visitor.field("from-edge", segment.FromEdge);
  visitor.field("to-edge", segment.ToEdge);
}

struct GroundLines {
  static constexpr std::string_view Name = "ground-lines";
  using Over = PathSegmentIntent;
  using Value = GroundLine;
  using Reads = std::tuple<>;

  static void derive(UnitReads &reads, EntityKey key, GroundLine &line) {
    const PathSegmentIntent &segment = *reads.component<PathSegmentIntent>(key);
    const StraightLine &straight = *reads.unit<StraightLines>(key);
    std::span<const ParkPoint> start;
    std::span<const ParkPoint> end;
    // A segment without a straight line rounds no corner.
    if (!straight.Points.empty()) {
      const Corner *from = reads.unit<Corners>(segment.From);
      const Corner *to = reads.unit<Corners>(segment.To);
      if (from != nullptr && from->Has) {
        start = halfOf(from->Round, key);
      }
      if (to != nullptr && to->Has) {
        end = halfOf(to->Round, key);
      }
    }
    groundLineOf(key, segment, straight.Points, start, end, line.Segment);
  }
};

// Puts the world's layout in lists.Layout and its ground in lists.Ground. When the ground units
// hold every change made to the world, it copies the ground from them. Otherwise it derives the
// whole layout's ground, as groundOf does. Both give the same ground. Allocates nothing once the
// lists have grown.
void worldGround(const World &world, GroundLists &lists);

} // namespace tpj

#endif
