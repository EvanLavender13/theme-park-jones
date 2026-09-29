#include "sim/routes/networks.h"

#include "sim/park/geometry.h"
#include "sim/routes/route_distance.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <optional>
#include <stddef.h>
#include <stdint.h>
#include <utility>
#include <vector>

namespace tpj {

namespace {

// One carrier for each path of the kind whose ground line is not empty, with no stops yet.
// parkPaths gives paths in ascending key order, so the carriers are in it too.
std::vector<Carrier> pathCarriers(const std::vector<ParkPath> &paths, PathKind kind) {
  std::vector<Carrier> carriers;
  for (const ParkPath &path : paths) {
    if (path.Kind != kind) {
      continue;
    }
    std::vector<CarrierPoint> line = groundLine(path.Points);
    if (line.empty()) {
      continue;
    }
    carriers.push_back(Carrier{.Key = path.Key, .Points = std::move(line), .Stops = {}});
  }
  return carriers;
}

// A meeting's distance on each of two carriers, named by their indices in the network's carriers.
struct Meeting {
  size_t First = 0;
  double FirstDistance = 0.0;
  size_t Second = 0;
  double SecondDistance = 0.0;
};

// The side of p from the segment from a to b: positive to its left, negative to its right.
double sideOf(const CarrierPoint &a, const CarrierPoint &b, const CarrierPoint &p) {
  return (b.X - a.X) * (p.Z - a.Z) - (b.Z - a.Z) * (p.X - a.X);
}

bool oppositeSigns(double first, double second) {
  return (first < 0.0 && second > 0.0) || (first > 0.0 && second < 0.0);
}

// The segment's distance at the fraction along it, clamped to its two distances.
double distanceAt(const CarrierPoint &a, const CarrierPoint &b, double fraction) {
  return std::clamp(a.Distance + fraction * (b.Distance - a.Distance), a.Distance, b.Distance);
}

// The distance of p's projection onto the segment from a to b, when p lies within
// JUNCTION_TOLERANCE of it.
std::optional<double> projectionWithin(const CarrierPoint &a, const CarrierPoint &b,
                                       const CarrierPoint &p) {
  const double dx = b.X - a.X;
  const double dz = b.Z - a.Z;
  const double t =
      std::clamp(((p.X - a.X) * dx + (p.Z - a.Z) * dz) / (dx * dx + dz * dz), 0.0, 1.0);
  CarrierPoint projected{.X = a.X + t * dx, .Z = a.Z + t * dz, .Distance = distanceAt(a, b, t)};
  if (t == 0.0) {
    projected = a;
  } else if (t == 1.0) {
    projected = b;
  }
  const double offX = p.X - projected.X;
  const double offZ = p.Z - projected.Z;
  if (offX * offX + offZ * offZ > JUNCTION_TOLERANCE * JUNCTION_TOLERANCE) {
    return std::nullopt;
  }
  return projected.Distance;
}

// Segments whose bounding boxes lie farther apart than this cannot meet. It is twice the tolerance,
// so rounding never rejects a pair the meeting test would accept.
constexpr double MEETING_MARGIN = 2.0 * JUNCTION_TOLERANCE;

// Whether the segments' bounding boxes lie too far apart for them to meet.
bool farApart(const CarrierPoint &a, const CarrierPoint &b, const CarrierPoint &c,
              const CarrierPoint &d) {
  return std::max(a.X, b.X) + MEETING_MARGIN < std::min(c.X, d.X) ||
         std::max(c.X, d.X) + MEETING_MARGIN < std::min(a.X, b.X) ||
         std::max(a.Z, b.Z) + MEETING_MARGIN < std::min(c.Z, d.Z) ||
         std::max(c.Z, d.Z) + MEETING_MARGIN < std::min(a.Z, b.Z);
}

// Adds the meetings of segment i of the first carrier and segment j of the second.
void addMeetings(const std::vector<Carrier> &carriers, size_t first, size_t i, size_t second,
                 size_t j, std::vector<Meeting> &meetings) {
  const CarrierPoint &a = carriers[first].Points[i];
  const CarrierPoint &b = carriers[first].Points[i + 1];
  const CarrierPoint &c = carriers[second].Points[j];
  const CarrierPoint &d = carriers[second].Points[j + 1];
  if (farApart(a, b, c, d)) {
    return;
  }
  const double sideC = sideOf(a, b, c);
  const double sideD = sideOf(a, b, d);
  const double sideA = sideOf(c, d, a);
  const double sideB = sideOf(c, d, b);
  if (oppositeSigns(sideC, sideD) && oppositeSigns(sideA, sideB)) {
    meetings.push_back({.First = first,
                        .FirstDistance = distanceAt(a, b, sideA / (sideA - sideB)),
                        .Second = second,
                        .SecondDistance = distanceAt(c, d, sideC / (sideC - sideD))});
  }
  // Ends are tested even for crossing segments: nearly collinear segments that rounding leaves
  // crossing at a shallow angle still meet at their ends.
  if (const auto on = projectionWithin(c, d, a)) {
    meetings.push_back({first, a.Distance, second, *on});
  }
  if (const auto on = projectionWithin(c, d, b)) {
    meetings.push_back({first, b.Distance, second, *on});
  }
  if (const auto on = projectionWithin(a, b, c)) {
    meetings.push_back({first, *on, second, c.Distance});
  }
  if (const auto on = projectionWithin(a, b, d)) {
    meetings.push_back({first, *on, second, d.Distance});
  }
}

// A segment's place in the network, with the span of its x coordinates.
struct SegmentSpan {
  size_t Carrier = 0;
  size_t Index = 0;
  double MinX = 0.0;
  double MaxX = 0.0;
};

// The meetings of every pair of segments on different carriers, or on one carrier and not
// neighbors along it. Segments are swept in order of their least x, so only pairs whose x spans
// come within farApart's margin are tested. farApart rejects every other pair, and nothing
// downstream depends on the order meetings are found in.
std::vector<Meeting> findMeetings(const std::vector<Carrier> &carriers) {
  std::vector<SegmentSpan> spans;
  for (size_t c = 0; c < carriers.size(); ++c) {
    const std::vector<CarrierPoint> &points = carriers[c].Points;
    for (size_t i = 0; i + 1 < points.size(); ++i) {
      spans.push_back({.Carrier = c,
                       .Index = i,
                       .MinX = std::min(points[i].X, points[i + 1].X),
                       .MaxX = std::max(points[i].X, points[i + 1].X)});
    }
  }
  std::ranges::sort(spans, [](const SegmentSpan &left, const SegmentSpan &right) {
    return left.MinX < right.MinX;
  });

  std::vector<Meeting> meetings;
  for (size_t p = 0; p < spans.size(); ++p) {
    for (size_t q = p + 1; q < spans.size() && spans[q].MinX <= spans[p].MaxX + MEETING_MARGIN;
         ++q) {
      // The pair is tested with the lower carrier, or the lower index on one carrier, first.
      const bool inOrder =
          spans[p].Carrier < spans[q].Carrier ||
          (spans[p].Carrier == spans[q].Carrier && spans[p].Index < spans[q].Index);
      const SegmentSpan &first = inOrder ? spans[p] : spans[q];
      const SegmentSpan &second = inOrder ? spans[q] : spans[p];
      if (first.Carrier == second.Carrier && second.Index < first.Index + 2) {
        continue;
      }
      addMeetings(carriers, first.Carrier, first.Index, second.Carrier, second.Index, meetings);
    }
  }
  return meetings;
}

// The first distances of a carrier's stop groups: sorted, each group taking every distance at most
// JUNCTION_TOLERANCE above its first.
std::vector<double> groupStarts(std::vector<double> distances) {
  std::ranges::sort(distances);
  std::vector<double> starts;
  for (const double distance : distances) {
    if (starts.empty() || distance - starts.back() > JUNCTION_TOLERANCE) {
      starts.push_back(distance);
    }
  }
  return starts;
}

// The index of the group holding the distance, which is at least the first start, 0.
size_t groupHolding(const std::vector<double> &starts, double distance) {
  return static_cast<size_t>(std::ranges::upper_bound(starts, distance) - starts.begin()) - 1;
}

// Stops joined into nodes, by union-find.
class StopJoins {
public:
  explicit StopJoins(size_t count) : Parents(count) {
    std::iota(Parents.begin(), Parents.end(), size_t{0});
  }

  size_t root(size_t stop) {
    while (Parents[stop] != stop) {
      Parents[stop] = Parents[Parents[stop]];
      stop = Parents[stop];
    }
    return stop;
  }

  void join(size_t first, size_t second) {
    const size_t firstRoot = root(first);
    const size_t secondRoot = root(second);
    Parents[std::max(firstRoot, secondRoot)] = std::min(firstRoot, secondRoot);
  }

private:
  std::vector<size_t> Parents;
};

// The midpoint of an entity's face, serving one network kind.
struct Door {
  EntityKey Entity = NULL_KEY;
  Face Side = Face::Front;
  GroundPoint At;
};

// Adds the door at the midpoint of the face of the pose's footprint for the size: the first two
// corners for the front, the last two for the back. Nothing when the pose has no footprint.
void addDoor(std::vector<Door> &doors, EntityKey entity, const Pose &pose, FootprintSize size,
             Face face) {
  const std::optional<Footprint> footprint = footprintOf(pose, size);
  if (!footprint) {
    return;
  }
  const size_t first = face == Face::Front ? 0 : 2;
  const ParkPoint &a = footprint->Corners[first];
  const ParkPoint &b = footprint->Corners[first + 1];
  doors.push_back({.Entity = entity, .Side = face, .At = {(a.X + b.X) / 2.0, (a.Z + b.Z) / 2.0}});
}

// The doors serving the kind: an entrance's front serves guests, a shop's front guests and its back
// backstage, and a depot's front backstage.
std::vector<Door> doorsServing(const std::vector<ParkEntrance> &entrances,
                               const std::vector<ParkBox> &boxes, PathKind kind) {
  std::vector<Door> doors;
  if (kind == PathKind::Guest) {
    for (const ParkEntrance &entrance : entrances) {
      addDoor(doors, entrance.Key, entrance.At, ENTRANCE_SIZE, Face::Front);
    }
  }
  for (const ParkBox &box : boxes) {
    const bool shop = box.Kind == BoxKind::Shop;
    if (kind == PathKind::Guest && shop) {
      addDoor(doors, box.Key, box.At, boxSize(box.Kind), Face::Front);
    } else if (kind == PathKind::Backstage) {
      addDoor(doors, box.Key, box.At, boxSize(box.Kind), shop ? Face::Back : Face::Front);
    }
  }
  return doors;
}

// The path carriers, each stopping only at its two ends, as a network nearestPlace can search.
Network linesOnly(const std::vector<Carrier> &carriers) {
  std::vector<Carrier> lines = carriers;
  uint32_t nodeCount = 0;
  for (Carrier &line : lines) {
    line.Stops = {{.Distance = 0.0, .Node = nodeCount},
                  {.Distance = line.Points.back().Distance, .Node = nodeCount + 1}};
    nodeCount += 2;
  }
  return Network(std::move(lines), nodeCount, {});
}

// A connector, its door's entity, and the index of the path carrier it meets and the distance
// there.
struct Connection {
  Carrier Connector;
  EntityKey Entity = NULL_KEY;
  size_t Path = 0;
  double PathDistance = 0.0;
};

// The connectors of the doors whose nearest point on the path carriers lies within reach. The
// carriers are in key order.
std::vector<Connection> findConnections(const std::vector<Carrier> &carriers,
                                        const std::vector<Door> &doors) {
  std::vector<Connection> connections;
  if (carriers.empty()) {
    return connections;
  }
  const Network lines = linesOnly(carriers);
  for (const Door &door : doors) {
    const std::optional<Place> place = lines.nearestPlace(door.At);
    if (!place) {
      continue;
    }
    const std::optional<GroundPoint> point = lines.groundPoint(*place);
    if (!point) {
      continue;
    }
    const double dx = point->X - door.At.X;
    const double dz = point->Z - door.At.Z;
    const double reach = std::sqrt((dx * dx) + (dz * dz));
    if (reach > CONNECTION_REACH || !(reach > JUNCTION_TOLERANCE)) {
      continue;
    }
    // A path keyed like the connector, which only a hand-written save holds, keeps carrier keys
    // unique by leaving the door unconnected.
    const EntityKey key = connectorKey(door.Entity, door.Side);
    if (std::ranges::binary_search(carriers, key, {}, &Carrier::Key)) {
      continue;
    }
    const auto path = std::ranges::lower_bound(carriers, place->Carrier, {}, &Carrier::Key);
    connections.push_back(
        {.Connector = {.Key = key,
                       .Points = {{.X = door.At.X, .Z = door.At.Z, .Distance = 0.0},
                                  {.X = point->X, .Z = point->Z, .Distance = reach}},
                       .Stops = {}},
         .Entity = door.Entity,
         .Path = static_cast<size_t>(path - carriers.begin()),
         .PathDistance = place->Distance});
  }
  return connections;
}

// Gives each carrier its stops, grouping each carrier's distances and joining the stops each
// meeting names into nodes, and returns the node count.
uint32_t addStops(std::vector<Carrier> &carriers, const std::vector<Meeting> &meetings) {
  std::vector<std::vector<double>> distances(carriers.size());
  for (size_t c = 0; c < carriers.size(); ++c) {
    distances[c] = {0.0, carriers[c].Points.back().Distance};
  }
  for (const Meeting &meeting : meetings) {
    distances[meeting.First].push_back(meeting.FirstDistance);
    distances[meeting.Second].push_back(meeting.SecondDistance);
  }

  // Every carrier's stops, numbered in one sequence so a meeting can join stops of two carriers.
  std::vector<std::vector<double>> starts(carriers.size());
  std::vector<size_t> firstStops(carriers.size());
  size_t stopCount = 0;
  for (size_t c = 0; c < carriers.size(); ++c) {
    starts[c] = groupStarts(std::move(distances[c]));
    firstStops[c] = stopCount;
    stopCount += starts[c].size();
  }
  StopJoins joins(stopCount);
  for (const Meeting &meeting : meetings) {
    joins.join(
        firstStops[meeting.First] + groupHolding(starts[meeting.First], meeting.FirstDistance),
        firstStops[meeting.Second] + groupHolding(starts[meeting.Second], meeting.SecondDistance));
  }

  // Nodes are numbered in order of first stop, carriers in key order and stops by distance. Paths
  // and connectors are walked together, since a hand-written save can give a path a key above a
  // connector's.
  std::vector<size_t> keyOrder(carriers.size());
  std::iota(keyOrder.begin(), keyOrder.end(), size_t{0});
  std::ranges::sort(keyOrder, {}, [&carriers](size_t c) { return carriers[c].Key; });
  constexpr uint32_t UNNUMBERED = UINT32_MAX;
  std::vector<uint32_t> numbers(stopCount, UNNUMBERED);
  uint32_t nodeCount = 0;
  for (const size_t c : keyOrder) {
    const double length = carriers[c].Points.back().Distance;
    for (size_t k = 0; k < starts[c].size(); ++k) {
      const size_t root = joins.root(firstStops[c] + k);
      if (numbers[root] == UNNUMBERED) {
        numbers[root] = nodeCount;
        ++nodeCount;
      }
      const bool last = k + 1 == starts[c].size();
      carriers[c].Stops.push_back(
          {.Distance = last ? length : starts[c][k], .Node = numbers[root]});
    }
  }
  return nodeCount;
}

Network deriveNetwork(const std::vector<ParkPath> &paths, const std::vector<Door> &doors,
                      PathKind kind) {
  std::vector<Carrier> carriers = pathCarriers(paths, kind);
  std::vector<Meeting> meetings = findMeetings(carriers);

  // Each connector meets its path at its connection.
  std::vector<Connection> connections = findConnections(carriers, doors);
  const size_t pathCount = carriers.size();
  for (Connection &connection : connections) {
    meetings.push_back({.First = connection.Path,
                        .FirstDistance = connection.PathDistance,
                        .Second = carriers.size(),
                        .SecondDistance = connection.Connector.Points.back().Distance});
    carriers.push_back(std::move(connection.Connector));
  }

  const uint32_t nodeCount = addStops(carriers, meetings);

  // Each connector's door is its first stop, a node nothing else meets, anchored to its entity.
  std::vector<NodeAnchor> anchors;
  for (size_t k = 0; k < connections.size(); ++k) {
    anchors.push_back(
        {.Node = carriers[pathCount + k].Stops.front().Node, .Entity = connections[k].Entity});
  }
  return Network(std::move(carriers), nodeCount, std::move(anchors));
}

void resolvePathNetworks(World &world) {
  const std::vector<ParkPath> paths = parkPaths(world);
  const std::vector<ParkEntrance> entrances = parkEntrances(world);
  const std::vector<ParkBox> boxes = parkBoxes(world);
  for (const PathKind kind : {PathKind::Guest, PathKind::Backstage}) {
    Network network = deriveNetwork(paths, doorsServing(entrances, boxes, kind), kind);
    const EntityKey key =
        world.createDerivedEntity(NULL_KEY, NETWORK_PURPOSE, static_cast<uint64_t>(kind));
    world.Registry.emplace_or_replace<Network>(world.findEntity(key), std::move(network));
  }
}

} // namespace

const Network &parkNetwork(const World &world, PathKind kind) {
  static const Network EMPTY;
  const entt::entity entity = world.findEntity(networkKey(kind));
  if (entity == entt::null) {
    return EMPTY;
  }
  const Network *network = world.Registry.try_get<Network>(entity);
  return network != nullptr ? *network : EMPTY;
}

void addRoutes(WorldSchema &schema) {
  schema.addResolver("path-networks", &resolvePathNetworks);
  addRouteDistance(schema);
}

} // namespace tpj
