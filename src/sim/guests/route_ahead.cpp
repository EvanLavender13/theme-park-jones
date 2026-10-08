#include "sim/guests/internal/route_ahead.h"

#include "sim/guests/guests.h"
#include "sim/guests/internal/ground.h"
#include "sim/park/intent.h"
#include "sim/routes/route_distance.h"
#include "sim/world.h"

#include <algorithm>
#include <cmath>
#include <variant>

namespace tpj {

namespace {

// How many times advanceAim halves the stretch it can't see all of.
constexpr int AIM_HALVINGS = 8;

GroundPoint between(GroundPoint a, GroundPoint b, double along) {
  return {a.X + ((b.X - a.X) * along), a.Z + ((b.Z - a.Z) * along)};
}

double squaredDistance(GroundPoint a, GroundPoint b) {
  const double x = b.X - a.X;
  const double z = b.Z - a.Z;
  return (x * x) + (z * z);
}

// The share of the way from a to b of the point of that line nearest the point, from 0 up to the
// limit.
double nearestAlong(GroundPoint a, GroundPoint b, GroundPoint point, double limit) {
  const double x = b.X - a.X;
  const double z = b.Z - a.Z;
  const double length = (x * x) + (z * z);
  if (length == 0.0) {
    return 0.0;
  }
  return std::clamp((((point.X - a.X) * x) + ((point.Z - a.Z) * z)) / length, 0.0, limit);
}

// Marks the point as the node at the place, with its index and whether it is a shaping point, when
// the place resolves to a node.
void markNode(const Network &network, const Place &place, RoutePoint &point) {
  const std::optional<NetworkPosition> position = network.resolve(place);
  const auto *node = position ? std::get_if<NodePosition>(&*position) : nullptr;
  point.Node = node != nullptr;
  if (node != nullptr) {
    point.NodeIndex = node->Node;
    point.Shaping = isShapingPoint(network, node->Node);
  }
}

// Adds the points of the step to the route, from the distance along its carrier to its To, the
// last a node, and adds what it walks to walked. If a point lies beyond AIM_LOOKAHEAD, it adds the
// point AIM_LOOKAHEAD along instead and returns false, as it does when the carrier is missing.
bool walkStep(const Network &network, const RouteStep &step, double from, double &walked,
              RouteAhead &route) {
  const auto carrier =
      std::ranges::lower_bound(network.carriers(), step.Carrier, {}, &Carrier::Key);
  if (carrier == network.carriers().end() || carrier->Key != step.Carrier) {
    return false;
  }
  const bool forward = step.To > from;
  const auto add = [&](double distance, GroundPoint at, bool node) {
    const double length = std::abs(distance - from);
    if (walked + length > AIM_LOOKAHEAD) {
      const double left = AIM_LOOKAHEAD - walked;
      const double cut = forward ? from + left : from - left;
      if (const std::optional<GroundPoint> cutAt = network.groundPoint({step.Carrier, cut})) {
        route.Points.push_back({*cutAt, step.Carrier, from, cut, false});
      }
      return false;
    }
    route.Points.push_back({at, step.Carrier, from, distance, node});
    walked += length;
    from = distance;
    return true;
  };
  const auto strictlyBetween = [&](double distance) {
    return forward ? distance > from && distance < step.To : distance < from && distance > step.To;
  };
  const std::vector<CarrierPoint> &points = carrier->Points;
  for (size_t i = 0; i < points.size(); ++i) {
    const CarrierPoint &point = forward ? points[i] : points[points.size() - 1 - i];
    if (strictlyBetween(point.Distance) && !add(point.Distance, {point.X, point.Z}, false)) {
      return false;
    }
  }
  const std::optional<GroundPoint> end = network.groundPoint({step.Carrier, step.To});
  if (!end || !add(step.To, *end, true)) {
    return false;
  }
  markNode(network, {step.Carrier, step.To}, route.Points.back());
  return true;
}

// The step a route toward the sources takes on from the end of the step: the least next step among
// their entries there. None where it stops, and Ends set where it has arrived.
std::optional<RouteStep> routeOn(const World &world, const Network &network, const RouteStep &step,
                                 std::span<const EntityKey> sources, RouteAhead &route) {
  const std::optional<RouteEntry> entry =
      leastEntryAt(world, network, {step.Carrier, step.To}, sources);
  if (!entry) {
    return std::nullopt;
  }
  if (entry->Next.Carrier == NULL_KEY) {
    route.Ends = true;
    return std::nullopt;
  }
  return entry->Next;
}

// The step a wandering route takes on from the end of the step: on through a shaping point, and out
// of the first other node along the turn when it starts there and isn't the way back. None where
// the route ends, which sets Ends. A route that ran on back the way it came would let the aim reach
// behind the guest, so where the turn goes back, the route stops at the node.
std::optional<RouteStep> wanderOn(const Network &network, const RouteStep &step,
                                  const RouteStep &turn, bool &turned, RouteAhead &route) {
  const RoutePoint &end = route.Points.back();
  const Place at{step.Carrier, step.To};
  const bool forward = step.To > step.From;
  std::optional<RouteStep> next;
  if (end.Node && end.Shaping) {
    next = onwardStep(network, end.NodeIndex, at, forward);
  } else if (end.Node && !turned && startsAt(network, turn, end.NodeIndex) &&
             !isWayBack(turn, at, forward)) {
    turned = true;
    next = turn;
  }
  route.Ends = !next;
  return next;
}

} // namespace

std::optional<RouteEntry> leastEntryAt(const World &world, const Network &network,
                                       const Place &place, std::span<const EntityKey> sources) {
  std::optional<RouteEntry> least;
  // Sources come in ascending key order, so keeping only a strictly less distance breaks ties to
  // the lower key.
  for (const EntityKey source : sources) {
    const std::optional<RouteEntry> entry =
        routeEntryAt(world, PathKind::Guest, network, place, source);
    if (entry && (!least || entry->Distance < least->Distance)) {
      least = entry;
    }
  }
  return least;
}

bool isShapingPoint(const Network &network, uint32_t node) {
  return network.edgeEnds(node).size() == 2 && network.nodeAnchor(node) == NULL_KEY;
}

bool isWayBack(const RouteStep &step, const Place &at, bool forward) {
  return step.Carrier == at.Carrier && step.From == at.Distance && (step.To > step.From) != forward;
}

RouteStep stepAway(const Network &network, const EdgeEnd &end) {
  const NetworkEdge &edge = network.edges()[end.Edge];
  return end.AtFrom ? RouteStep{edge.Carrier, edge.FromDistance, edge.ToDistance}
                    : RouteStep{edge.Carrier, edge.ToDistance, edge.FromDistance};
}

std::optional<RouteStep> onwardStep(const Network &network, uint32_t node, const Place &at,
                                    bool forward) {
  for (const EdgeEnd &end : network.edgeEnds(node)) {
    const RouteStep step = stepAway(network, end);
    if (!isWayBack(step, at, forward)) {
      return step;
    }
  }
  return std::nullopt;
}

bool startsAt(const Network &network, const RouteStep &step, uint32_t node) {
  if (step.Carrier == NULL_KEY) {
    return false;
  }
  const std::optional<NetworkPosition> position = network.resolve({step.Carrier, step.From});
  const auto *at = position ? std::get_if<NodePosition>(&*position) : nullptr;
  return at != nullptr && at->Node == node;
}

void traceRoute(const World &world, const Network &network, const Place &place,
                const RouteStep &first, std::span<const EntityKey> sources, const RouteStep &turn,
                RouteAhead &route) {
  route.Points.clear();
  route.Ends = false;
  const std::optional<GroundPoint> start = network.groundPoint(place);
  if (!start) {
    return;
  }
  route.Points.push_back({*start, place.Carrier, place.Distance, place.Distance});
  markNode(network, place, route.Points.back());
  if (first.Carrier == NULL_KEY) {
    route.Ends = true;
    return;
  }
  double walked = 0.0;
  // Inside an edge a step names the whole edge, so the first stretch starts where the guest is.
  double from = first.Carrier == place.Carrier ? place.Distance : first.From;
  RouteStep step = first;
  bool turned = false;
  for (;;) {
    const double startedAt = walked;
    if (!walkStep(network, step, from, walked, route)) {
      return;
    }
    const std::optional<RouteStep> next = sources.empty()
                                              ? wanderOn(network, step, turn, turned, route)
                                              : routeOn(world, network, step, sources, route);
    // A step that walks nowhere would never end the loop.
    if (!next || (walked == startedAt && next->From == next->To)) {
      return;
    }
    step = *next;
    from = step.From;
  }
}

std::optional<RouteSpot> spotOf(const RouteAhead &route, const Place &place) {
  if (route.Points.empty()) {
    return std::nullopt;
  }
  const RoutePoint &start = route.Points.front();
  if (start.Carrier == place.Carrier && start.To == place.Distance) {
    return RouteSpot{0, 1.0};
  }
  for (size_t i = 1; i < route.Points.size(); ++i) {
    const RoutePoint &point = route.Points[i];
    if (point.Carrier != place.Carrier || place.Distance < std::min(point.From, point.To) ||
        place.Distance > std::max(point.From, point.To)) {
      continue;
    }
    if (place.Distance == point.To || point.From == point.To) {
      return RouteSpot{i, 1.0};
    }
    return RouteSpot{i, (place.Distance - point.From) / (point.To - point.From)};
  }
  return std::nullopt;
}

Place placeAt(const RouteAhead &route, RouteSpot spot) {
  const RoutePoint &point = route.Points[spot.Point];
  if (spot.Along == 1.0) {
    return {point.Carrier, point.To};
  }
  return {point.Carrier, point.From + ((point.To - point.From) * spot.Along)};
}

GroundPoint pointAt(const RouteAhead &route, RouteSpot spot) {
  const RoutePoint &point = route.Points[spot.Point];
  if (spot.Point == 0 || spot.Along == 1.0) {
    return point.At;
  }
  return between(route.Points[spot.Point - 1].At, point.At, spot.Along);
}

std::optional<size_t> turnNode(const RouteAhead &route, bool headedForEnd) {
  double along = 0.0;
  for (size_t i = 0; i < route.Points.size(); ++i) {
    const RoutePoint &point = route.Points[i];
    if (i > 0) {
      along += std::abs(point.To - point.From);
    }
    if (!point.Node || point.Shaping) {
      continue;
    }
    if (along > TURN_LOOKAHEAD || (headedForEnd && route.Ends && i + 1 == route.Points.size())) {
      return std::nullopt;
    }
    return i;
  }
  return std::nullopt;
}

bool passedPoint(const RouteAhead &route, RouteSpot spot, size_t index) {
  // A spot at the very start of the next stretch stands on the point.
  return index + 1 < route.Points.size() &&
         (spot.Point > index + 1 || (spot.Point == index + 1 && spot.Along > 0.0));
}

bool nodeAhead(const RouteAhead &route, RouteSpot spot) {
  // The spot's own stretch ends ahead of it unless the spot stands on that stretch's end.
  const size_t first = spot.Point == 0 || spot.Along == 1.0 ? spot.Point + 1 : spot.Point;
  for (size_t i = first; i + 1 < route.Points.size(); ++i) {
    if (route.Points[i].Node) {
      return true;
    }
  }
  return false;
}

RouteSpot advanceAim(const GuestGround &ground, uint32_t group, GroundPoint from,
                     const RouteAhead &route, RouteSpot aim) {
  for (;;) {
    RouteSpot candidate{aim.Point, 1.0};
    if (aim.Point == 0 || aim.Along == 1.0) {
      if (aim.Point + 1 >= route.Points.size()) {
        return aim;
      }
      candidate = {aim.Point + 1, 1.0};
    }
    if (ground.clearLine(group, from, pointAt(route, candidate))) {
      aim = candidate;
      continue;
    }
    // The aim and the candidate as shares of the candidate's stretch.
    double clear = candidate.Point == aim.Point ? aim.Along : 0.0;
    const double start = clear;
    double blocked = 1.0;
    for (int i = 0; i < AIM_HALVINGS; ++i) {
      const double middle = 0.5 * (clear + blocked);
      if (ground.clearLine(group, from, pointAt(route, {candidate.Point, middle}))) {
        clear = middle;
      } else {
        blocked = middle;
      }
    }
    return clear > start ? RouteSpot{candidate.Point, clear} : aim;
  }
}

RouteSpot nearestSpot(const RouteAhead &route, GroundPoint point, RouteSpot aim) {
  RouteSpot best{0, 1.0};
  double bestSquared = squaredDistance(route.Points.front().At, point);
  for (size_t i = 1; i <= aim.Point; ++i) {
    const GroundPoint a = route.Points[i - 1].At;
    const GroundPoint b = route.Points[i].At;
    const double along = nearestAlong(a, b, point, i == aim.Point ? aim.Along : 1.0);
    const double distance = squaredDistance(between(a, b, along), point);
    if (distance < bestSquared) {
      best = {i, along};
      bestSquared = distance;
    }
  }
  return best;
}

} // namespace tpj
