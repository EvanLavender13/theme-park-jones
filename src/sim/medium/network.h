#ifndef TPJ_SIM_MEDIUM_NETWORK_H
#define TPJ_SIM_MEDIUM_NETWORK_H

#include "sim/entity_key.h"

#include <optional>
#include <span>
#include <stddef.h>
#include <stdint.h>
#include <variant>
#include <vector>

namespace tpj {

class WorldSchema;

// A point of a carrier's ground line: its position on the ground, and its arc length from the
// carrier's start as the carrier's producer measured it.
struct CarrierPoint {
  double X = 0.0;
  double Z = 0.0;
  double Distance = 0.0;

  bool operator==(const CarrierPoint &) const = default;
};

template <typename Visitor> void visitFields(Visitor &visitor, CarrierPoint &point) {
  visitor.field("x", point.X);
  visitor.field("z", point.Z);
  visitor.field("distance", point.Distance);
}

// A distance along a carrier where it meets a node.
struct CarrierStop {
  double Distance = 0.0;
  uint32_t Node = 0;
};

template <typename Visitor> void visitFields(Visitor &visitor, CarrierStop &stop) {
  visitor.field("distance", stop.Distance);
  visitor.field("node", stop.Node);
}

// A line with a stable key that edges are stretches of, such as a drawn path or a box's connector.
// Its stops cut it into edges, the first at 0 and the last at its length.
struct Carrier {
  EntityKey Key = NULL_KEY;
  std::vector<CarrierPoint> Points;
  std::vector<CarrierStop> Stops;
};

template <typename Visitor> void visitFields(Visitor &visitor, Carrier &carrier) {
  visitor.field("key", carrier.Key);
  visitor.field("points", carrier.Points);
  visitor.field("stops", carrier.Stops);
}

// A node tied to an entity, so the entity finds where it meets the network.
struct NodeAnchor {
  uint32_t Node = 0;
  EntityKey Entity = NULL_KEY;
};

template <typename Visitor> void visitFields(Visitor &visitor, NodeAnchor &anchor) {
  visitor.field("node", anchor.Node);
  visitor.field("entity", anchor.Entity);
}

// The stretch of a carrier between two consecutive stops.
struct NetworkEdge {
  EntityKey Carrier = NULL_KEY;
  uint32_t From = 0;
  uint32_t To = 0;
  double FromDistance = 0.0;
  double ToDistance = 0.0;

  [[nodiscard]] double length() const { return ToDistance - FromDistance; }
};

// A carrier and a distance along it. It names the same ground position however the carrier is cut
// into edges, so holders keep places, never edges.
struct Place {
  EntityKey Carrier = NULL_KEY;
  double Distance = 0.0;

  bool operator==(const Place &) const = default;
};

template <typename Visitor> void visitFields(Visitor &visitor, Place &place) {
  visitor.field("carrier", place.Carrier);
  visitor.field("distance", place.Distance);
}

struct GroundPoint {
  double X = 0.0;
  double Z = 0.0;

  bool operator==(const GroundPoint &) const = default;
};

// A place at a node.
struct NodePosition {
  uint32_t Node = 0;
};

// A place strictly inside an edge, with its distances along the carrier to the edge's two ends.
struct EdgePosition {
  uint32_t Edge = 0;
  double FromOffset = 0.0;
  double ToOffset = 0.0;
};

using NetworkPosition = std::variant<NodePosition, EdgePosition>;

// Carriers, nodes, and anchors, as a value. Derived data: producers build networks in resolution.
class Network {
public:
  Network() = default;
  // Sorts carriers by key and anchors by node. Throws std::invalid_argument for a repeated or null
  // carrier key, a carrier's points or stops out of order, missing, or not finite, a stop's node
  // not below nodeCount, a node no carrier stops at, or an anchor on a missing, null, or already
  // anchored node.
  Network(std::vector<Carrier> carriers, uint32_t nodeCount, std::vector<NodeAnchor> anchors);

  // In ascending key order.
  [[nodiscard]] const std::vector<Carrier> &carriers() const { return Carriers; }
  [[nodiscard]] uint32_t nodeCount() const { return NodeCount; }
  // Carriers in key order, and each carrier's edges in order of distance.
  [[nodiscard]] const std::vector<NetworkEdge> &edges() const { return Edges; }

  // The node at exactly the place's distance, or the edge enclosing it. None when the carrier is
  // not in the network, or the distance is NaN, below 0, or above the carrier's length.
  [[nodiscard]] std::optional<NetworkPosition> resolve(const Place &place) const;
  // The node's stop on its lowest-keyed carrier, at the lowest distance there. Throws
  // std::out_of_range for a node not below nodeCount().
  [[nodiscard]] Place nodePlace(uint32_t node) const;
  // The place of each stop at the node, in ascending carrier key and then distance: exactly the
  // places resolve gives the node for. Throws std::out_of_range as nodePlace does.
  [[nodiscard]] std::span<const Place> stopPlaces(uint32_t node) const;
  // The node's anchored entity, or NULL_KEY. Throws std::out_of_range as nodePlace does.
  [[nodiscard]] EntityKey nodeAnchor(uint32_t node) const;
  // The nodes anchored to the entity, ascending.
  [[nodiscard]] std::vector<uint32_t> anchoredNodes(EntityKey entity) const;
  // The place's ground position, interpolated by distance between carrier points. None where
  // resolve gives none.
  [[nodiscard]] std::optional<GroundPoint> groundPoint(const Place &place) const;
  // The place whose ground position is nearest the point, ties to the lower carrier key and then
  // the lower distance. None for an empty network or a point that is not finite.
  [[nodiscard]] std::optional<Place> nearestPlace(GroundPoint point) const;
  // The place on the carrier whose ground position is nearest the point, ties to the lower
  // distance. None when the carrier is not in the network or the point is not finite.
  [[nodiscard]] std::optional<Place> nearestPlaceOn(EntityKey carrier, GroundPoint point) const;

  // Lists the inputs. The edges and stop places are rebuilt from them only by the constructor, so
  // Network is never loaded: it is derived, and saves never hold it.
  template <typename Visitor> friend void visitFields(Visitor &visitor, Network &network) {
    visitor.field("carriers", network.Carriers);
    visitor.field("nodes", network.NodeCount);
    visitor.field("anchors", network.Anchors);
  }

private:
  // The index in Carriers of the carrier holding the place, when the place lies on it.
  [[nodiscard]] std::optional<size_t> findCarrierOf(const Place &place) const;

  std::vector<Carrier> Carriers;
  uint32_t NodeCount = 0;
  std::vector<NodeAnchor> Anchors;

  // Built by the constructor from the members above.
  std::vector<NetworkEdge> Edges;
  // Parallel to Carriers: the index in Edges of each carrier's first edge.
  std::vector<uint32_t> FirstEdges;
  // Each node's stop places, node by node: node n's run from StopStarts[n] up to StopStarts[n + 1].
  std::vector<Place> StopPlaces;
  std::vector<size_t> StopStarts;
};

// Moves a place held across a re-derivation from the network before to the network after: kept
// when its carrier's points are unchanged, moved to the carrier's nearest point to its old ground
// position when they changed, and none when it did not resolve before or its carrier is gone.
[[nodiscard]] std::optional<Place> carryOver(const Place &place, const Network &before,
                                             const Network &after);

// Registers Network as the derived component type network.
void addNetworkComponent(WorldSchema &schema);

} // namespace tpj

#endif
