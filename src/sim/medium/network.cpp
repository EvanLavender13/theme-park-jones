#include "sim/medium/network.h"

#include "sim/schema.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace tpj {

namespace {

std::string carrierName(EntityKey key) {
  return "carrier " + std::to_string(static_cast<uint64_t>(key));
}

bool isFinite(double value) { return std::isfinite(value); }

void requireValidPoints(const Carrier &carrier) {
  const std::string name = carrierName(carrier.Key);
  if (carrier.Points.size() < 2) {
    throw std::invalid_argument(name + " has fewer than two points");
  }
  for (const CarrierPoint &point : carrier.Points) {
    if (!isFinite(point.X) || !isFinite(point.Z) || !isFinite(point.Distance)) {
      throw std::invalid_argument(name + " has a coordinate or distance that is not finite");
    }
  }
  if (carrier.Points.front().Distance != 0.0) {
    throw std::invalid_argument(name + " does not start at distance 0");
  }
  for (size_t i = 1; i < carrier.Points.size(); ++i) {
    if (carrier.Points[i].Distance <= carrier.Points[i - 1].Distance) {
      throw std::invalid_argument(name + " has point distances that do not strictly increase");
    }
  }
}

void requireValidStops(const Carrier &carrier, uint32_t nodeCount) {
  const std::string name = carrierName(carrier.Key);
  if (carrier.Stops.size() < 2) {
    throw std::invalid_argument(name + " has fewer than two stops");
  }
  for (const CarrierStop &stop : carrier.Stops) {
    if (!isFinite(stop.Distance)) {
      throw std::invalid_argument(name + " has a stop distance that is not finite");
    }
    if (stop.Node >= nodeCount) {
      throw std::invalid_argument(name + " stops at node " + std::to_string(stop.Node) +
                                  ", which is not below the node count");
    }
  }
  if (carrier.Stops.front().Distance != 0.0) {
    throw std::invalid_argument(name + " has its first stop away from 0");
  }
  if (carrier.Stops.back().Distance != carrier.Points.back().Distance) {
    throw std::invalid_argument(name + " has its last stop away from its length");
  }
  for (size_t i = 1; i < carrier.Stops.size(); ++i) {
    if (carrier.Stops[i].Distance <= carrier.Stops[i - 1].Distance) {
      throw std::invalid_argument(name + " has stop distances that do not strictly increase");
    }
  }
}

void requireValidAnchors(const std::vector<NodeAnchor> &anchors, uint32_t nodeCount) {
  for (size_t i = 0; i < anchors.size(); ++i) {
    const NodeAnchor &anchor = anchors[i];
    const std::string name = "node " + std::to_string(anchor.Node);
    if (anchor.Node >= nodeCount) {
      throw std::invalid_argument("an anchor names " + name +
                                  ", which is not below the node count");
    }
    if (anchor.Entity == NULL_KEY) {
      throw std::invalid_argument("an anchor on " + name + " names the null key");
    }
    if (i > 0 && anchors[i - 1].Node == anchor.Node) {
      throw std::invalid_argument(name + " is anchored twice");
    }
  }
}

} // namespace

Network::Network(std::vector<Carrier> carriers, uint32_t nodeCount, std::vector<NodeAnchor> anchors)
    : Carriers(std::move(carriers)), NodeCount(nodeCount), Anchors(std::move(anchors)) {
  std::ranges::sort(Carriers, {}, &Carrier::Key);
  std::ranges::sort(Anchors, {}, &NodeAnchor::Node);
  for (size_t i = 0; i < Carriers.size(); ++i) {
    if (Carriers[i].Key == NULL_KEY) {
      throw std::invalid_argument("a carrier has the null key");
    }
    if (i > 0 && Carriers[i - 1].Key == Carriers[i].Key) {
      throw std::invalid_argument(carrierName(Carriers[i].Key) + " is repeated");
    }
    requireValidPoints(Carriers[i]);
    requireValidStops(Carriers[i], NodeCount);
  }
  requireValidAnchors(Anchors, NodeCount);

  std::vector<std::optional<Place>> places(NodeCount);
  for (const Carrier &carrier : Carriers) {
    FirstEdges.push_back(static_cast<uint32_t>(Edges.size()));
    for (size_t i = 0; i < carrier.Stops.size(); ++i) {
      const CarrierStop &stop = carrier.Stops[i];
      if (!places[stop.Node]) {
        places[stop.Node] = Place{carrier.Key, stop.Distance};
      }
      if (i + 1 < carrier.Stops.size()) {
        const CarrierStop &next = carrier.Stops[i + 1];
        Edges.push_back({carrier.Key, stop.Node, next.Node, stop.Distance, next.Distance});
      }
    }
  }
  NodePlaces.reserve(NodeCount);
  for (uint32_t node = 0; node < NodeCount; ++node) {
    const std::optional<Place> &place = places[node];
    if (!place) {
      throw std::invalid_argument("node " + std::to_string(node) +
                                  " has no carrier stopping at it");
    }
    NodePlaces.push_back(*place);
  }
}

std::optional<size_t> Network::findCarrierOf(const Place &place) const {
  const auto found = std::ranges::lower_bound(Carriers, place.Carrier, {}, &Carrier::Key);
  if (found == Carriers.end() || found->Key != place.Carrier) {
    return std::nullopt;
  }
  if (std::isnan(place.Distance) || place.Distance < 0.0 ||
      place.Distance > found->Points.back().Distance) {
    return std::nullopt;
  }
  return static_cast<size_t>(found - Carriers.begin());
}

std::optional<NetworkPosition> Network::resolve(const Place &place) const {
  const std::optional<size_t> index = findCarrierOf(place);
  if (!index) {
    return std::nullopt;
  }
  const Carrier &carrier = Carriers[*index];
  // The first stop at or beyond the distance. It exists because the last stop is at the length.
  const auto stop =
      std::ranges::lower_bound(carrier.Stops, place.Distance, {}, &CarrierStop::Distance);
  if (stop->Distance == place.Distance) {
    return NodePosition{stop->Node};
  }
  // The distance is above the first stop, at 0, so the stop before this one exists.
  const auto edge = FirstEdges[*index] + static_cast<uint32_t>(stop - carrier.Stops.begin()) - 1;
  return EdgePosition{edge, place.Distance - Edges[edge].FromDistance,
                      Edges[edge].ToDistance - place.Distance};
}

Place Network::nodePlace(uint32_t node) const { return NodePlaces.at(node); }

EntityKey Network::nodeAnchor(uint32_t node) const {
  if (node >= NodeCount) {
    throw std::out_of_range("node " + std::to_string(node) + " is not below the node count");
  }
  const auto found = std::ranges::lower_bound(Anchors, node, {}, &NodeAnchor::Node);
  return found != Anchors.end() && found->Node == node ? found->Entity : NULL_KEY;
}

std::vector<uint32_t> Network::anchoredNodes(EntityKey entity) const {
  std::vector<uint32_t> nodes;
  for (const NodeAnchor &anchor : Anchors) {
    if (anchor.Entity == entity) {
      nodes.push_back(anchor.Node);
    }
  }
  return nodes;
}

std::optional<GroundPoint> Network::groundPoint(const Place &place) const {
  const std::optional<size_t> index = findCarrierOf(place);
  if (!index) {
    return std::nullopt;
  }
  const std::vector<CarrierPoint> &points = Carriers[*index].Points;
  // The first point at or beyond the distance. It exists because the last point is at the length.
  const auto after = std::ranges::lower_bound(points, place.Distance, {}, &CarrierPoint::Distance);
  if (after->Distance == place.Distance) {
    return GroundPoint{after->X, after->Z};
  }
  const CarrierPoint &a = *(after - 1);
  const CarrierPoint &b = *after;
  const double t = (place.Distance - a.Distance) / (b.Distance - a.Distance);
  return GroundPoint{a.X + t * (b.X - a.X), a.Z + t * (b.Z - a.Z)};
}

std::optional<Place> Network::nearestPlace(GroundPoint point) const {
  if (!isFinite(point.X) || !isFinite(point.Z)) {
    return std::nullopt;
  }
  std::optional<Place> best;
  double bestSquared = 0.0;
  for (const Carrier &carrier : Carriers) {
    for (size_t i = 0; i + 1 < carrier.Points.size(); ++i) {
      const CarrierPoint &a = carrier.Points[i];
      const CarrierPoint &b = carrier.Points[i + 1];
      const double dx = b.X - a.X;
      const double dz = b.Z - a.Z;
      const double lengthSquared = dx * dx + dz * dz;
      double t = 0.0;
      if (lengthSquared > 0.0) {
        t = std::clamp(((point.X - a.X) * dx + (point.Z - a.Z) * dz) / lengthSquared, 0.0, 1.0);
      }
      // A segment's ends are exact, so a junction is equally near on every carrier stopping there
      // and the tie rule decides between them.
      CarrierPoint projected{a.X + t * dx, a.Z + t * dz,
                             a.Distance + t * (b.Distance - a.Distance)};
      if (t == 0.0) {
        projected = a;
      } else if (t == 1.0) {
        projected = b;
      }
      const double offX = point.X - projected.X;
      const double offZ = point.Z - projected.Z;
      const double squared = offX * offX + offZ * offZ;
      if (!best || squared < bestSquared) {
        best = Place{carrier.Key, projected.Distance};
        bestSquared = squared;
      }
    }
  }
  return best;
}

void addNetworkComponent(WorldSchema &schema) {
  schema.addComponent<Network>("network", DataKind::Derived);
}

} // namespace tpj
