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

// The path carriers of a kind, and the keys of the points each one's segment runs from and to.
struct PathCarriers {
  std::vector<Carrier> Carriers;
  std::vector<std::pair<EntityKey, EntityKey>> Ends;
};

// One carrier for each segment of the kind with a ground line, with its width and no stops yet.
// parkSegments gives segments in ascending key order, so the carriers are in it too.
PathCarriers pathCarriers(const std::vector<ParkSegment> &segments, PathKind kind) {
  PathCarriers paths;
  for (const ParkSegment &segment : segments) {
    if (segment.Kind != kind || segment.Line.empty()) {
      continue;
    }
    paths.Carriers.push_back(
        Carrier{.Key = segment.Key, .Width = segment.Width, .Points = segment.Line, .Stops = {}});
    paths.Ends.emplace_back(segment.From, segment.To);
  }
  return paths;
}

// A meeting's distance on each of two carriers, named by their indices in the network's carriers.
struct Meeting {
  size_t First = 0;
  double FirstDistance = 0.0;
  size_t Second = 0;
  double SecondDistance = 0.0;
};

// A carrier's end at a path point: the point's key, the carrier's index, and its distance there.
struct PointEnd {
  EntityKey Point = NULL_KEY;
  size_t Carrier = 0;
  double Distance = 0.0;
};

// Carriers whose segments end at one point meet there: the first end at each point meets every
// other end at it.
std::vector<Meeting> pointMeetings(const PathCarriers &paths) {
  std::vector<PointEnd> ends;
  for (size_t c = 0; c < paths.Carriers.size(); ++c) {
    ends.push_back({.Point = paths.Ends[c].first, .Carrier = c, .Distance = 0.0});
    ends.push_back({.Point = paths.Ends[c].second,
                    .Carrier = c,
                    .Distance = paths.Carriers[c].Points.back().Distance});
  }
  std::ranges::sort(ends, [](const PointEnd &left, const PointEnd &right) {
    if (left.Point != right.Point) {
      return left.Point < right.Point;
    }
    if (left.Carrier != right.Carrier) {
      return left.Carrier < right.Carrier;
    }
    return left.Distance < right.Distance;
  });
  std::vector<Meeting> meetings;
  size_t first = 0;
  for (size_t e = 1; e < ends.size(); ++e) {
    if (ends[e].Point != ends[first].Point) {
      first = e;
      continue;
    }
    meetings.push_back({.First = ends[first].Carrier,
                        .FirstDistance = ends[first].Distance,
                        .Second = ends[e].Carrier,
                        .SecondDistance = ends[e].Distance});
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

// Adds the door at the midpoint of the face of the pose's footprint for the size: the first two
// corners for the front, the last two for the back. It faces out along the footprint's forward
// direction for the front and against it for the back. Nothing when the pose has no footprint.
void addDoor(std::vector<ParkDoor> &doors, EntityKey entity, const Pose &pose, FootprintSize size,
             Face face) {
  const std::optional<Footprint> footprint = footprintOf(pose, size);
  if (!footprint) {
    return;
  }
  const size_t first = face == Face::Front ? 0 : 2;
  const ParkPoint &a = footprint->Corners[first];
  const ParkPoint &b = footprint->Corners[first + 1];
  const double out = face == Face::Front ? 1.0 : -1.0;
  doors.push_back({.Entity = entity,
                   .Side = face,
                   .At = {(a.X + b.X) / 2.0, (a.Z + b.Z) / 2.0},
                   .Out = {footprint->Forward.X * out, footprint->Forward.Z * out}});
}

// The doors serving the kind: an entrance's front serves guests, a shop's front guests and its back
// backstage, and a depot's front backstage.
std::vector<ParkDoor> doorsServing(const std::vector<ParkEntrance> &entrances,
                                   const std::vector<ParkBox> &boxes, PathKind kind) {
  std::vector<ParkDoor> doors;
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

// The connectors of the doors, each the given width. A door connects to the path carrier whose
// edge, half its width out from its line, lies nearest, when that edge lies within reach. The
// carriers are in key order, so the lower key wins a tie.
std::vector<Connection> findConnections(const std::vector<Carrier> &carriers,
                                        const std::vector<ParkDoor> &doors, double width) {
  std::vector<Connection> connections;
  if (carriers.empty()) {
    return connections;
  }
  const Network lines = linesOnly(carriers);
  for (const ParkDoor &door : doors) {
    const GroundPoint at{door.At.X, door.At.Z};
    std::optional<Place> place;
    GroundPoint point;
    double reach = 0.0;
    double edge = 0.0;
    for (const Carrier &carrier : carriers) {
      const std::optional<Place> on = lines.nearestPlaceOn(carrier.Key, at);
      const std::optional<GroundPoint> ground = on ? lines.groundPoint(*on) : std::nullopt;
      if (!ground) {
        continue;
      }
      const double dx = ground->X - at.X;
      const double dz = ground->Z - at.Z;
      const double distance = std::sqrt((dx * dx) + (dz * dz));
      if (!place || distance - (0.5 * carrier.Width) < edge) {
        place = on;
        point = *ground;
        reach = distance;
        edge = distance - (0.5 * carrier.Width);
      }
    }
    if (!place || edge > CONNECTION_REACH || !(reach > JUNCTION_TOLERANCE)) {
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
                       .Width = width,
                       .Points = {{.X = door.At.X, .Z = door.At.Z, .Distance = 0.0},
                                  {.X = point.X, .Z = point.Z, .Distance = reach}},
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

Network deriveNetwork(const std::vector<ParkSegment> &segments, const std::vector<ParkDoor> &doors,
                      PathKind kind) {
  PathCarriers paths = pathCarriers(segments, kind);
  std::vector<Meeting> meetings = pointMeetings(paths);
  std::vector<Carrier> carriers = std::move(paths.Carriers);

  // Each connector meets its path at its connection.
  std::vector<Connection> connections = findConnections(carriers, doors, pathWidth(kind));
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

// Derived, on a network's entity from path-networks to the finisher that drops it: the network the
// previous resolution derived.
struct PreviousNetwork {
  Network Held;
};

template <typename Visitor> void visitFields(Visitor &visitor, PreviousNetwork &previous) {
  visitor.field("network", previous.Held);
}

void resolvePathNetworks(World &world) {
  const std::vector<ParkSegment> segments = parkSegments(world);
  const std::vector<ParkEntrance> entrances = parkEntrances(world);
  const std::vector<ParkBox> boxes = parkBoxes(world);
  for (const PathKind kind : {PathKind::Guest, PathKind::Backstage}) {
    Network network = deriveNetwork(segments, doorsServing(entrances, boxes, kind), kind);
    const EntityKey key =
        world.createDerivedEntity(NULL_KEY, NETWORK_PURPOSE, static_cast<uint64_t>(kind));
    const entt::entity entity = world.findEntity(key);
    if (Network *held = world.Registry.try_get<Network>(entity)) {
      world.Registry.emplace_or_replace<PreviousNetwork>(entity, PreviousNetwork{std::move(*held)});
    }
    world.Registry.emplace_or_replace<Network>(entity, std::move(network));
  }
}

// Removes both kinds' previous networks, so no resolution ends holding one.
void dropPreviousNetworks(World &world) {
  for (const PathKind kind : {PathKind::Guest, PathKind::Backstage}) {
    const entt::entity entity = world.findEntity(networkKey(kind));
    if (entity != entt::null) {
      world.Registry.remove<PreviousNetwork>(entity);
    }
  }
}

} // namespace

const Network *previousNetwork(const World &world, PathKind kind) {
  const entt::entity entity = world.findEntity(networkKey(kind));
  if (entity == entt::null) {
    return nullptr;
  }
  const PreviousNetwork *previous = world.Registry.try_get<PreviousNetwork>(entity);
  return previous != nullptr ? &previous->Held : nullptr;
}

std::vector<ParkDoor> parkDoors(const World &world, PathKind kind) {
  return doorsServing(parkEntrances(world), parkBoxes(world), kind);
}

std::vector<Walkway> connectorWalkways(const World &world, PathKind kind) {
  const Network &network = parkNetwork(world, kind);
  const std::vector<ParkDoor> doors = parkDoors(world, kind);
  // Only connectors' doors are anchored, so a carrier whose first stop is anchored is a connector.
  const auto isConnector = [&network](const Carrier &carrier) {
    return !carrier.Stops.empty() && network.nodeAnchor(carrier.Stops.front().Node) != NULL_KEY;
  };
  std::vector<Walkway> walkways;
  for (const Carrier &carrier : network.carriers()) {
    if (!isConnector(carrier)) {
      continue;
    }
    const EntityKey entity = network.nodeAnchor(carrier.Stops.front().Node);
    const Face face = carrier.Key == connectorKey(entity, Face::Front) ? Face::Front : Face::Back;
    const auto door = std::ranges::find_if(doors, [&](const ParkDoor &candidate) {
      return candidate.Entity == entity && candidate.Side == face;
    });
    if (door == doors.end()) {
      continue;
    }
    // stopPlaces lists places in ascending carrier key, so this is the lowest-keyed segment there.
    EntityKey segment = NULL_KEY;
    for (const Place &place : network.stopPlaces(carrier.Stops.back().Node)) {
      const auto other =
          std::ranges::lower_bound(network.carriers(), place.Carrier, {}, &Carrier::Key);
      if (place.Carrier != carrier.Key && other != network.carriers().end() &&
          other->Key == place.Carrier && !isConnector(*other)) {
        segment = place.Carrier;
        break;
      }
    }
    if (segment == NULL_KEY) {
      continue;
    }
    const CarrierPoint &meets = carrier.Points.back();
    walkways.push_back(Walkway{.Kind = kind,
                               .Joins = segment,
                               .Door = door->At,
                               .Out = door->Out,
                               .Meets = {meets.X, meets.Z},
                               .Width = carrier.Width});
  }
  return walkways;
}

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
  schema.addComponent<PreviousNetwork>("previous-network", DataKind::Derived);
  schema.addResolver("path-networks", &resolvePathNetworks);
  addRouteDistance(schema);
}

void addDropPreviousNetworks(WorldSchema &schema) { schema.addFinisher(&dropPreviousNetworks); }

} // namespace tpj
