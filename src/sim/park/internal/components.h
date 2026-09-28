#ifndef TPJ_SIM_PARK_INTERNAL_COMPONENTS_H
#define TPJ_SIM_PARK_INTERNAL_COMPONENTS_H

#include "sim/park/intent.h"

#include <vector>

namespace tpj {

// A path the player drew: its kind and the points they clicked, in order.
struct PathIntent {
  PathKind Kind = PathKind::Guest;
  std::vector<ParkPoint> Points;

  bool operator==(const PathIntent &) const = default;
};

template <typename Visitor> void visitFields(Visitor &visitor, PathIntent &path) {
  visitor.field("kind", path.Kind);
  visitor.field("points", path.Points);
}

// A box the player placed.
struct BoxIntent {
  BoxKind Kind = BoxKind::Shop;
  Pose At;

  bool operator==(const BoxIntent &) const = default;
};

template <typename Visitor> void visitFields(Visitor &visitor, BoxIntent &box) {
  visitor.field("kind", box.Kind);
  visitor.field("x", box.At.X);
  visitor.field("z", box.At.Z);
  visitor.field("facing-x", box.At.FacingX);
  visitor.field("facing-z", box.At.FacingZ);
}

// Where guests arrive and leave.
struct EntranceIntent {
  Pose At;

  bool operator==(const EntranceIntent &) const = default;
};

template <typename Visitor> void visitFields(Visitor &visitor, EntranceIntent &entrance) {
  visitor.field("x", entrance.At.X);
  visitor.field("z", entrance.At.Z);
  visitor.field("facing-x", entrance.At.FacingX);
  visitor.field("facing-z", entrance.At.FacingZ);
}

} // namespace tpj

#endif
