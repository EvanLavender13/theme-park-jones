#ifndef TPJ_TESTS_SIM_SUPPORT_LINE_PARK_H
#define TPJ_TESTS_SIM_SUPPORT_LINE_PARK_H

#include "support/connected_parks.h"
#include "support/park_worlds.h"

#include "sim/entity_key.h"
#include "sim/park/intent.h"
#include "sim/world.h"

#include <utility>
#include <vector>

// The line park: one straight backstage path along z = 0 from x = -64 to x = 64, with boxes
// beside it, each door that serves the backstage network off the line and within reach of it. A
// shop at x lies on the -z side facing -z, so its back door is toward the line, at (x, -g) for g
// that door's distance from the line. A depot at x lies on the +z side facing -z, so its front door
// is at (x, g), or, across the line, on the -z side facing +z, with its front door at (x, -g). A
// supply route between boxes at x and y is about g + |x - y| + g, but the line's ground line is
// sampled, so exact lengths come from sampling route distance.
namespace tpj::test {

inline constexpr EntityKey LINE{1};
inline constexpr EntityKey SHOP{2};
inline constexpr EntityKey FAR_DEPOT{3};
inline constexpr EntityKey DEPOT{4};
inline constexpr EntityKey TWIN_DEPOT{5};
inline constexpr EntityKey OTHER_SHOP{6};
// A shop and a depot far from any line, with no backstage connector.
inline constexpr EntityKey LOOSE_SHOP{7};
inline constexpr EntityKey LOOSE_DEPOT{8};
// A key no entity holds, as when a box has been deleted.
inline constexpr EntityKey GONE{40};

// The distance from the line's middle of each door that connects to it.
inline double doorGap() { return gapWithinReach(PathKind::Backstage); }

inline ParkBox shopAt(EntityKey key, double x) {
  const double z = -(doorGap() + (boxSize(BoxKind::Shop).Depth / 2.0));
  return ParkBox{.Key = key, .Kind = BoxKind::Shop, .At = Pose{x, z, 0.0, -1.0}};
}

inline ParkBox depotAt(EntityKey key, double x) {
  const double z = doorGap() + (boxSize(BoxKind::Depot).Depth / 2.0);
  return ParkBox{.Key = key, .Kind = BoxKind::Depot, .At = Pose{x, z, 0.0, -1.0}};
}

// A depot across the line from depotAt's, whose front door meets the line at the same place.
inline ParkBox depotAcross(EntityKey key, double x) {
  const double z = -(doorGap() + (boxSize(BoxKind::Depot).Depth / 2.0));
  return ParkBox{.Key = key, .Kind = BoxKind::Depot, .At = Pose{x, z, 0.0, 1.0}};
}

inline ParkBox looseShop() {
  return ParkBox{.Key = LOOSE_SHOP, .Kind = BoxKind::Shop, .At = Pose{0.0, -60.0, 0.0, -1.0}};
}

inline ParkBox looseDepot() {
  return ParkBox{.Key = LOOSE_DEPOT, .Kind = BoxKind::Depot, .At = Pose{0.0, 60.0, 0.0, -1.0}};
}

inline ParkPath linePath() {
  return ParkPath{.Key = LINE, .Kind = PathKind::Backstage, .Points = {{-64.0, 0.0}, {64.0, 0.0}}};
}

// The line park with the boxes, resolved, at tick 0 with empty ledgers.
inline World lineWorld(std::vector<ParkBox> boxes) {
  World world =
      worldOf(ParkIntent{.Entrances = {}, .Paths = {linePath()}, .Boxes = std::move(boxes)});
  resolveWorld(world);
  return world;
}

} // namespace tpj::test

#endif
