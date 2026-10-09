#ifndef TPJ_SIM_PARK_INTERNAL_COMPONENTS_H
#define TPJ_SIM_PARK_INTERNAL_COMPONENTS_H

#include "sim/entity_key.h"
#include "sim/park/intent.h"

#include <vector>

namespace tpj {

// A point that path segments share: its ground position, the radius of the corner it makes when
// exactly two segments meet there, and the length over which the narrower of two segments curves
// out to the wider one's side where they meet there, or 0 for its natural length.
struct PathPointIntent {
  ParkPoint At;
  double Radius = DEFAULT_CORNER_RADIUS;
  double Taper = DEFAULT_TAPER_LENGTH;

  bool operator==(const PathPointIntent &) const = default;
};

template <typename Visitor> void visitFields(Visitor &visitor, PathPointIntent &point) {
  visitor.field("x", point.At.X);
  visitor.field("z", point.At.Z);
  visitor.field("radius", point.Radius);
  visitor.field("taper", point.Taper);
}

// A straight piece of path between two path points, and the radius of the edge that starts from its
// left side at each end, seen walking away from that end's point.
struct PathSegmentIntent {
  PathKind Kind = PathKind::Guest;
  EntityKey From = NULL_KEY;
  EntityKey To = NULL_KEY;
  double Width = 0.0;
  double FromEdge = DEFAULT_EDGE_RADIUS;
  double ToEdge = DEFAULT_EDGE_RADIUS;

  bool operator==(const PathSegmentIntent &) const = default;
};

template <typename Visitor> void visitFields(Visitor &visitor, PathSegmentIntent &segment) {
  visitor.field("kind", segment.Kind);
  visitor.field("from", segment.From);
  visitor.field("to", segment.To);
  visitor.field("width", segment.Width);
  visitor.field("from-edge-radius", segment.FromEdge);
  visitor.field("to-edge-radius", segment.ToEdge);
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

// A shop's stock, held only when it is not food.
struct ShopStockIntent {
  ShopStock Stock = ShopStock::Food;

  bool operator==(const ShopStockIntent &) const = default;
};

template <typename Visitor> void visitFields(Visitor &visitor, ShopStockIntent &stock) {
  visitor.field("stock", stock.Stock);
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
