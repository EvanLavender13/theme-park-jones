#ifndef TPJ_SIM_GUESTS_INTERNAL_ROUTE_AHEAD_H
#define TPJ_SIM_GUESTS_INTERNAL_ROUTE_AHEAD_H

#include "sim/entity_key.h"
#include "sim/medium/network.h"
#include "sim/routes/route_distance.h"

#include <optional>
#include <span>
#include <stddef.h>
#include <stdint.h>
#include <vector>

namespace tpj {

class GuestGround;
class World;

// A point of a guest's route ahead: where it is on the ground, the carrier of the stretch of route
// that ends at it, the distances along that carrier where the stretch starts and ends, and whether
// the point is a node. For a node, NodeIndex is its index in the network, and Shaping is whether
// it is a shaping point.
struct RoutePoint {
  GroundPoint At;
  EntityKey Carrier = NULL_KEY;
  double From = 0.0;
  double To = 0.0;
  bool Node = false;
  uint32_t NodeIndex = 0;
  bool Shaping = false;
};

// A guest's route ahead of its place: its points, the first at the place, and whether the last is
// where the route ends rather than where looking ahead stops.
struct RouteAhead {
  std::vector<RoutePoint> Points;
  bool Ends = false;
};

// A point along a route ahead: the index of the point its stretch ends at, and how far along that
// stretch it lies, from 0 to 1. Index 0, with Along 1, is the route's first point.
struct RouteSpot {
  size_t Point = 0;
  double Along = 1.0;
};

// The least entry at the place among the sources' route distance entries on the guest network,
// ties to the lower key, or none. Sources are in ascending key order.
std::optional<RouteEntry> leastEntryAt(const World &world, const Network &network,
                                       const Place &place, std::span<const EntityKey> sources);
// Whether the node is a shaping point: exactly two edge ends meet it, and nothing is anchored to
// it.
bool isShapingPoint(const Network &network, uint32_t node);
// Whether the step leaves the place back along the place's carrier, against the way forward gives:
// the way back for a guest arriving at the place walking that way.
bool isWayBack(const RouteStep &step, const Place &at, bool forward);
// The step along the end's edge, walked away from the node at that end.
RouteStep stepAway(const Network &network, const EdgeEnd &end);
// The first step out of the node, in edgeEnds order, that isn't the way back for a guest arriving
// at the place walking the way forward gives, each walked along its edge away from the node; none
// when every step is the way back. Allocates nothing.
std::optional<RouteStep> onwardStep(const Network &network, uint32_t node, const Place &at,
                                    bool forward);
// Whether the step's carrier isn't NULL_KEY and its From resolves to the node.
bool startsAt(const Network &network, const RouteStep &step, uint32_t node);
// Fills the route from the place along the first step, until AIM_LOOKAHEAD along it. With sources,
// it goes on from each node it reaches by the least next step among the sources' entries there.
// With none, the guest is wandering: the route goes on through shaping points, follows the turn out
// of the first other node when the turn starts there and isn't the way back, and ends at the next
// node that isn't a shaping point, or at that first one when it doesn't follow the turn. If the
// first step's carrier is NULL_KEY, the route is the place alone, and it ends there. Allocates
// nothing once the route's list has grown to its largest.
void traceRoute(const World &world, const Network &network, const Place &place,
                const RouteStep &first, std::span<const EntityKey> sources, const RouteStep &turn,
                RouteAhead &route);
// The first spot of the route at the place, or none when the route doesn't pass it.
std::optional<RouteSpot> spotOf(const RouteAhead &route, const Place &place);
// The spot's place on the network.
Place placeAt(const RouteAhead &route, RouteSpot spot);
// The spot's ground position.
GroundPoint pointAt(const RouteAhead &route, RouteSpot spot);
// The index of the first point of the route that is a node other than a shaping point, when it lies
// within TURN_LOOKAHEAD along the route from its first point, and isn't the last point of a route
// that ends there when headedForEnd is true; otherwise none.
std::optional<size_t> turnNode(const RouteAhead &route, bool headedForEnd);
// Whether the spot lies past the route's point at the index, and not on it.
bool passedPoint(const RouteAhead &route, RouteSpot spot, size_t index);
// Whether a node of the route lies after the spot and before the route's last point.
bool nodeAhead(const RouteAhead &route, RouteSpot spot);
// Moves the aim forward along the route to the farthest spot a straight line from the point
// reaches on the group's paving: each later route point in turn while the line to it is clear, and
// then, within the stretch to the first that isn't, as far as halving finds clear.
RouteSpot advanceAim(const GuestGround &ground, uint32_t group, GroundPoint from,
                     const RouteAhead &route, RouteSpot aim);
// The spot of the route, from its start up to the aim, nearest the point, ties to the earlier.
RouteSpot nearestSpot(const RouteAhead &route, GroundPoint point, RouteSpot aim);

} // namespace tpj

#endif
