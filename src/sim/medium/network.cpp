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

void requireValidWidth(const Carrier &carrier) {
  if (!isFinite(carrier.Width) || !(carrier.Width > 0.0)) {
    throw std::invalid_argument(carrierName(carrier.Key) +
                                " has a width that is not finite and above zero");
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

// The carrier with the key, or null when there is none.
const Carrier *findCarrier(const std::vector<Carrier> &carriers, EntityKey key) {
  const auto found = std::ranges::lower_bound(carriers, key, {}, &Carrier::Key);
  return found != carriers.end() && found->Key == key ? &*found : nullptr;
}

// Projects the point onto each segment of the carrier, replacing best only with a strictly nearer
// projection, so earlier carriers and then lower distances win ties.
void projectOntoLine(EntityKey key, std::span<const CarrierPoint> points, GroundPoint point,
                     std::optional<Place> &best, double &bestSquared) {
  for (size_t i = 0; i + 1 < points.size(); ++i) {
    const CarrierPoint &a = points[i];
    const CarrierPoint &b = points[i + 1];
    const double dx = b.X - a.X;
    const double dz = b.Z - a.Z;
    const double lengthSquared = dx * dx + dz * dz;
    double t = 0.0;
    if (lengthSquared > 0.0) {
      t = std::clamp(((point.X - a.X) * dx + (point.Z - a.Z) * dz) / lengthSquared, 0.0, 1.0);
    }
    // A segment's ends are exact, so a junction is equally near on every carrier stopping there
    // and the tie rule decides between them.
    CarrierPoint projected{a.X + t * dx, a.Z + t * dz, a.Distance + t * (b.Distance - a.Distance)};
    if (t == 0.0) {
      projected = a;
    } else if (t == 1.0) {
      projected = b;
    }
    const double offX = point.X - projected.X;
    const double offZ = point.Z - projected.Z;
    const double squared = offX * offX + offZ * offZ;
    if (!best || squared < bestSquared) {
      best = Place{key, projected.Distance};
      bestSquared = squared;
    }
  }
}

void projectOnto(const Carrier &carrier, GroundPoint point, std::optional<Place> &best,
                 double &bestSquared) {
  projectOntoLine(carrier.Key, carrier.Points, point, best, bestSquared);
}

// The snapped place, when its ground position lies within ON_GROUND_TOLERANCE of the point.
std::optional<Place> onGround(const Network &network, const std::optional<Place> &snapped,
                              GroundPoint point) {
  const std::optional<GroundPoint> at = snapped ? network.groundPoint(*snapped) : std::nullopt;
  if (!at) {
    return std::nullopt;
  }
  const double dx = at->X - point.X;
  const double dz = at->Z - point.Z;
  if ((dx * dx) + (dz * dz) > ON_GROUND_TOLERANCE * ON_GROUND_TOLERANCE) {
    return std::nullopt;
  }
  return snapped;
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
    requireValidWidth(Carriers[i]);
    requireValidPoints(Carriers[i]);
    requireValidStops(Carriers[i], NodeCount);
  }
  requireValidAnchors(Anchors, NodeCount);

  // Counts each node's stops into the start after it, while building the edges.
  StopStarts.assign(static_cast<size_t>(NodeCount) + 1, 0);
  for (const Carrier &carrier : Carriers) {
    FirstEdges.push_back(static_cast<uint32_t>(Edges.size()));
    for (size_t i = 0; i < carrier.Stops.size(); ++i) {
      const CarrierStop &stop = carrier.Stops[i];
      ++StopStarts[static_cast<size_t>(stop.Node) + 1];
      if (i + 1 < carrier.Stops.size()) {
        const CarrierStop &next = carrier.Stops[i + 1];
        Edges.push_back({carrier.Key, stop.Node, next.Node, stop.Distance, next.Distance});
      }
    }
  }
  for (size_t node = 0; node < NodeCount; ++node) {
    if (StopStarts[node + 1] == 0) {
      throw std::invalid_argument("node " + std::to_string(node) +
                                  " has no carrier stopping at it");
    }
    StopStarts[node + 1] += StopStarts[node];
  }
  // Carriers in key order and stops in distance order leave each node's run in that order.
  StopPlaces.resize(StopStarts.back());
  std::vector<size_t> filled(StopStarts.begin(), StopStarts.end() - 1);
  for (const Carrier &carrier : Carriers) {
    for (const CarrierStop &stop : carrier.Stops) {
      StopPlaces[filled[stop.Node]] = Place{carrier.Key, stop.Distance};
      ++filled[stop.Node];
    }
  }
  buildEdgeEnds();
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

void Network::buildEdgeEnds() {
  // Edges in index order leave each node's run in ascending edge index, From before To.
  EdgeEndStarts.assign(static_cast<size_t>(NodeCount) + 1, 0);
  for (const NetworkEdge &edge : Edges) {
    ++EdgeEndStarts[static_cast<size_t>(edge.From) + 1];
    ++EdgeEndStarts[static_cast<size_t>(edge.To) + 1];
  }
  for (size_t node = 0; node < NodeCount; ++node) {
    EdgeEndStarts[node + 1] += EdgeEndStarts[node];
  }
  EdgeEnds.resize(EdgeEndStarts.back());
  std::vector<size_t> endsFilled(EdgeEndStarts.begin(), EdgeEndStarts.end() - 1);
  for (size_t i = 0; i < Edges.size(); ++i) {
    const auto index = static_cast<uint32_t>(i);
    EdgeEnds[endsFilled[Edges[i].From]++] = EdgeEnd{.Edge = index, .AtFrom = true};
    EdgeEnds[endsFilled[Edges[i].To]++] = EdgeEnd{.Edge = index, .AtFrom = false};
  }
}

Place Network::nodePlace(uint32_t node) const { return stopPlaces(node).front(); }

std::span<const Place> Network::stopPlaces(uint32_t node) const {
  if (node >= NodeCount) {
    throw std::out_of_range("node " + std::to_string(node) + " is not below the node count");
  }
  return std::span<const Place>(StopPlaces)
      .subspan(StopStarts[node], StopStarts[node + 1] - StopStarts[node]);
}

std::span<const EdgeEnd> Network::edgeEnds(uint32_t node) const {
  if (node >= NodeCount) {
    throw std::out_of_range("node " + std::to_string(node) + " is not below the node count");
  }
  return std::span<const EdgeEnd>(EdgeEnds).subspan(EdgeEndStarts[node],
                                                    EdgeEndStarts[node + 1] - EdgeEndStarts[node]);
}

EntityKey Network::nodeAnchor(uint32_t node) const {
  if (node >= NodeCount) {
    throw std::out_of_range("node " + std::to_string(node) + " is not below the node count");
  }
  const auto found = std::ranges::lower_bound(Anchors, node, {}, &NodeAnchor::Node);
  return found != Anchors.end() && found->Node == node ? found->Entity : NULL_KEY;
}

std::vector<uint32_t> Network::anchoredNodes(EntityKey entity) const {
  std::vector<uint32_t> nodes;
  anchoredNodes(entity, nodes);
  return nodes;
}

void Network::anchoredNodes(EntityKey entity, std::vector<uint32_t> &nodes) const {
  nodes.clear();
  for (const NodeAnchor &anchor : Anchors) {
    if (anchor.Entity == entity) {
      nodes.push_back(anchor.Node);
    }
  }
}

std::optional<uint32_t> Network::firstAnchoredNode(EntityKey entity) const {
  std::optional<uint32_t> first;
  for (const NodeAnchor &anchor : Anchors) {
    if (anchor.Entity == entity && (!first || anchor.Node < *first)) {
      first = anchor.Node;
    }
  }
  return first;
}

std::optional<GroundPoint> Network::groundPoint(const Place &place) const {
  const std::optional<size_t> index = findCarrierOf(place);
  if (!index) {
    return std::nullopt;
  }
  return groundPointOnLine(Carriers[*index].Points, place.Distance);
}

std::optional<double> Network::widthAt(const Place &place) const {
  const std::optional<size_t> index = findCarrierOf(place);
  if (!index) {
    return std::nullopt;
  }
  return Carriers[*index].Width;
}

std::optional<GroundDirection> Network::directionAt(const Place &place) const {
  const std::optional<size_t> index = findCarrierOf(place);
  if (!index) {
    return std::nullopt;
  }
  const std::vector<CarrierPoint> &points = Carriers[*index].Points;
  // The first point beyond the distance, so a place at a point takes the piece ahead of it. The
  // first point is at 0 and the place is at or above 0, so it is never the first point. Beyond
  // the end there is none, and the last piece is used.
  auto after = std::ranges::upper_bound(points, place.Distance, {}, &CarrierPoint::Distance);
  if (after == points.end()) {
    after = points.end() - 1;
  }
  const CarrierPoint &a = *(after - 1);
  const CarrierPoint &b = *after;
  const double dx = b.X - a.X;
  const double dz = b.Z - a.Z;
  const double length = std::sqrt((dx * dx) + (dz * dz));
  if (!(length > 0.0)) {
    return std::nullopt;
  }
  return GroundDirection{dx / length, dz / length};
}

std::optional<Place> Network::nearestPlace(GroundPoint point) const {
  if (!isFinite(point.X) || !isFinite(point.Z)) {
    return std::nullopt;
  }
  std::optional<Place> best;
  double bestSquared = 0.0;
  for (const Carrier &carrier : Carriers) {
    projectOnto(carrier, point, best, bestSquared);
  }
  return best;
}

std::optional<Place> Network::nearestPlaceOn(EntityKey carrier, GroundPoint point) const {
  const Carrier *found = findCarrier(Carriers, carrier);
  if (found == nullptr || !isFinite(point.X) || !isFinite(point.Z)) {
    return std::nullopt;
  }
  return nearestPlaceOnLine(found->Key, found->Points, point);
}

std::optional<Place> nearestPlaceOnLine(EntityKey carrier, std::span<const CarrierPoint> points,
                                        GroundPoint point) {
  if (points.empty() || !isFinite(point.X) || !isFinite(point.Z)) {
    return std::nullopt;
  }
  std::optional<Place> best;
  double bestSquared = 0.0;
  projectOntoLine(carrier, points, point, best, bestSquared);
  return best;
}

std::optional<GroundPoint> groundPointOnLine(std::span<const CarrierPoint> points,
                                             double distance) {
  if (points.empty() || std::isnan(distance) || distance < 0.0 ||
      distance > points.back().Distance) {
    return std::nullopt;
  }
  // The first point at or beyond the distance. It exists because the last point is at the length.
  const auto after = std::ranges::lower_bound(points, distance, {}, &CarrierPoint::Distance);
  if (after->Distance == distance) {
    return GroundPoint{after->X, after->Z};
  }
  const CarrierPoint &a = *(after - 1);
  const CarrierPoint &b = *after;
  const double t = (distance - a.Distance) / (b.Distance - a.Distance);
  return GroundPoint{a.X + t * (b.X - a.X), a.Z + t * (b.Z - a.Z)};
}

std::optional<Place> carryOver(const Place &place, const Network &before, const Network &after) {
  const std::optional<GroundPoint> ground = before.groundPoint(place);
  if (!ground) {
    return std::nullopt;
  }
  // The place resolves before, so its carrier is there. Stops are not geometry, so a split that
  // keeps the carrier's key keeps the place.
  const Carrier *was = findCarrier(before.carriers(), place.Carrier);
  const Carrier *now = findCarrier(after.carriers(), place.Carrier);
  if (was != nullptr && now != nullptr && was->Points == now->Points) {
    return place;
  }
  // None when the carrier is gone.
  const std::optional<Place> own = after.nearestPlaceOn(place.Carrier, *ground);
  if (const std::optional<Place> stays = onGround(after, own, *ground)) {
    return stays;
  }
  if (const std::optional<Place> stays = onGround(after, after.nearestPlace(*ground), *ground)) {
    return stays;
  }
  return own;
}

void addNetworkComponent(WorldSchema &schema) {
  schema.addComponent<Network>("network", DataKind::Derived);
}

} // namespace tpj
