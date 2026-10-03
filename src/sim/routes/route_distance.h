#ifndef TPJ_SIM_ROUTES_ROUTE_DISTANCE_H
#define TPJ_SIM_ROUTES_ROUTE_DISTANCE_H

#include "sim/entity_key.h"
#include "sim/medium/field.h"
#include "sim/medium/network.h"
#include "sim/park/intent.h"

#include <optional>
#include <string_view>
#include <vector>

namespace tpj {

class WorldSchema;

// An edge walked one way: along the carrier from its stop at From to its stop at To. The carrier
// is NULL_KEY at the source itself, where the route has arrived.
struct RouteStep {
  EntityKey Carrier = NULL_KEY;
  double From = 0.0;
  double To = 0.0;

  bool operator==(const RouteStep &) const = default;
};

template <typename Visitor> void visitFields(Visitor &visitor, RouteStep &step) {
  visitor.field("carrier", step.Carrier);
  visitor.field("from", step.From);
  visitor.field("to", step.To);
}

// A source's distance along the network from a place, and the first step of a shortest route.
struct RouteEntry {
  double Distance = 0.0;
  RouteStep Next;

  bool operator==(const RouteEntry &) const = default;
};

template <typename Visitor> void visitFields(Visitor &visitor, RouteEntry &entry) {
  visitor.field("distance", entry.Distance);
  visitor.field("next", entry.Next);
}

// At a place inside an edge, one entry for the source: the better of the edge's two ends, the
// place's offset to each added to that end's distance, ties to the From end. Entries inside the
// edge are ignored.
std::vector<RouteEntry> sampleRouteEdge(const EdgeSample<RouteEntry> &sample);

// The route distance field on the kind's network.
template <PathKind NetworkKind> struct RouteDistance {
  using Entry = RouteEntry;
  static constexpr std::string_view Name =
      NetworkKind == PathKind::Guest ? "guest-route-distance" : "backstage-route-distance";
  static constexpr FieldKind Kind = FieldKind::Entry;

  static std::vector<RouteEntry> sampleEdge(const EdgeSample<RouteEntry> &sample) {
    return sampleRouteEdge(sample);
  }
};

// The source's entry in the kind's route distance field at the place on the network: exactly the
// entry sampleField of that field gives the source there, or none. Reads only the source's
// entries and allocates nothing.
std::optional<RouteEntry> routeEntryAt(const World &world, PathKind kind, const Network &network,
                                       const Place &place, EntityKey source);

// The source's entry at each node of the network, indexed by node, none where no route joins the
// node to one the source anchors. Distances are least route lengths added from the source's end,
// and each next step is the lowest by carrier key, then toward lower distances, then From, of
// those that achieve its node's distance.
std::vector<std::optional<RouteEntry>> routeEntries(const Network &network, EntityKey source);

// Registers the guest and then the backstage route distance field, and then the resolver
// route-distance, which publishes every anchored entity's entries at its nodes. addRoutes calls
// it after registering path-networks.
void addRouteDistance(WorldSchema &schema);

} // namespace tpj

#endif
