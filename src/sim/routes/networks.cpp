#include "sim/routes/networks.h"

#include "sim/routes/internal/connections.h"
#include "sim/routes/internal/walked_surface.h"
#include "sim/routes/route_distance.h"
#include "sim/schema.h"
#include "sim/units.h"
#include "sim/world.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <optional>
#include <span>
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
    groupStarts(distances[c], starts[c]);
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

// A connection of the kind's network, and the list holding its connector's points.
struct KindConnection {
  const DoorConnectionList *List = nullptr;
  const DoorConnection *Connection = nullptr;
};

// The kind's network: a carrier for each segment of the kind with a ground line, and a connector
// for each connection, in the order given, each meeting the path its connection meets.
Network deriveNetwork(const std::vector<ParkSegment> &segments,
                      std::span<const KindConnection> connections, PathKind kind) {
  PathCarriers paths = pathCarriers(segments, kind);
  std::vector<Meeting> meetings = pointMeetings(paths);
  std::vector<Carrier> carriers = std::move(paths.Carriers);
  const size_t pathCount = carriers.size();
  for (const KindConnection &kept : connections) {
    const DoorConnection &connection = *kept.Connection;
    const std::span<const CarrierPoint> points = connectorPoints(*kept.List, connection);
    const auto path = std::ranges::lower_bound(
        carriers.begin(), carriers.begin() + static_cast<std::ptrdiff_t>(pathCount),
        connection.Path, {}, &Carrier::Key);
    meetings.push_back({.First = static_cast<size_t>(path - carriers.begin()),
                        .FirstDistance = connection.PathDistance,
                        .Second = carriers.size(),
                        .SecondDistance = points.back().Distance});
    carriers.push_back(Carrier{.Key = connectorKey(connection.Door.Entity, connection.Door.Side),
                               .Width = connection.Width,
                               .Points = {points.begin(), points.end()},
                               .Stops = {}});
  }

  const uint32_t nodeCount = addStops(carriers, meetings);

  // Each connector's door is its first stop, a node nothing else meets, anchored to its entity.
  std::vector<NodeAnchor> anchors;
  for (size_t k = 0; k < connections.size(); ++k) {
    anchors.push_back({.Node = carriers[pathCount + k].Stops.front().Node,
                       .Entity = connections[k].Connection->Door.Entity});
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

// The segments, entrances, boxes, and one kind's connections resolvePathNetworks reads, kept from
// one resolution to the next.
struct NetworkLists {
  std::vector<ParkSegment> Segments;
  std::vector<ParkEntrance> Entrances;
  std::vector<ParkBox> Boxes;
  std::vector<KindConnection> Connections;
};

// Puts in connections, clearing it first, each connection of the kind, in the order doorsServing
// lists their doors: the entrances' first, then the boxes', each in key order of its entity.
void kindConnections(const World &world, const NetworkLists &lists, PathKind kind,
                     std::vector<KindConnection> &connections) {
  connections.clear();
  const auto add = [&](EntityKey entity) {
    if (const DoorConnectionList *list = unitValue<DoorConnections>(world, entity)) {
      for (const DoorConnection &connection : list->Doors) {
        if (connection.Kind == kind) {
          connections.push_back({list, &connection});
        }
      }
    }
  };
  for (const ParkEntrance &entrance : lists.Entrances) {
    add(entrance.Key);
  }
  for (const ParkBox &box : lists.Boxes) {
    add(box.Key);
  }
}

void resolvePathNetworks(World &world, TickBuffers &buffers) {
  NetworkLists &lists = buffers.get<NetworkLists>();
  parkSegments(world, lists.Segments, buffers);
  parkEntrances(world, lists.Entrances);
  parkBoxes(world, lists.Boxes);
  for (const PathKind kind : {PathKind::Guest, PathKind::Backstage}) {
    kindConnections(world, lists, kind, lists.Connections);
    Network network = deriveNetwork(lists.Segments, lists.Connections, kind);
    const EntityKey key =
        world.createDerivedEntity(NULL_KEY, NETWORK_PURPOSE, static_cast<uint64_t>(kind));
    if (Network *held = world.tryGet<Network>(key)) {
      world.emplaceOrReplace<PreviousNetwork>(key, PreviousNetwork{std::move(*held)});
    }
    world.emplaceOrReplace<Network>(key, std::move(network));
  }
}

// The entrances and boxes the doors are read from.
struct DoorLists {
  std::vector<ParkEntrance> Entrances;
  std::vector<ParkBox> Boxes;
};

// Removes both kinds' previous networks, so no resolution ends holding one.
void dropPreviousNetworks(World &world, TickBuffers & /*buffers*/) {
  for (const PathKind kind : {PathKind::Guest, PathKind::Backstage}) {
    world.remove<PreviousNetwork>(networkKey(kind));
  }
}

} // namespace

const Network *previousNetwork(const World &world, PathKind kind) {
  const PreviousNetwork *previous = world.tryGet<PreviousNetwork>(networkKey(kind));
  return previous != nullptr ? &previous->Held : nullptr;
}

std::vector<ParkDoor> parkDoors(const World &world, PathKind kind) {
  std::vector<ParkDoor> doors;
  TickBuffers buffers;
  parkDoors(world, kind, doors, buffers);
  return doors;
}

void parkDoors(const World &world, PathKind kind, std::vector<ParkDoor> &doors,
               TickBuffers &buffers) {
  DoorLists &lists = buffers.get<DoorLists>();
  parkEntrances(world, lists.Entrances);
  parkBoxes(world, lists.Boxes);
  doorsServing(lists.Entrances, lists.Boxes, kind, doors);
}

namespace {

// The entrances and boxes the walkways are read from, kept in the buffers.
struct WalkwayEntities {
  std::vector<ParkEntrance> Entrances;
  std::vector<ParkBox> Boxes;
};

// Appends the entity's walkways of the kind.
void appendWalkways(const World &world, EntityKey entity, PathKind kind,
                    std::vector<KeyedWalkway> &walkways) {
  if (const DoorWalkwayList *list = unitValue<DoorWalkways>(world, entity)) {
    for (const DoorWalkway &walkway : list->Ways) {
      if (walkway.Way.Kind == kind) {
        walkways.push_back(KeyedWalkway{.Connector = walkway.Connector, .Way = walkway.Way});
      }
    }
  }
}

} // namespace

void keyedWalkways(const World &world, PathKind kind, std::vector<KeyedWalkway> &walkways,
                   TickBuffers &buffers) {
  WalkwayEntities &lists = buffers.get<WalkwayEntities>();
  parkEntrances(world, lists.Entrances);
  parkBoxes(world, lists.Boxes);
  const size_t first = walkways.size();
  for (const ParkEntrance &entrance : lists.Entrances) {
    appendWalkways(world, entrance.Key, kind, walkways);
  }
  for (const ParkBox &box : lists.Boxes) {
    appendWalkways(world, box.Key, kind, walkways);
  }
  std::sort(walkways.begin() + static_cast<std::ptrdiff_t>(first), walkways.end(),
            [](const KeyedWalkway &left, const KeyedWalkway &right) {
              return left.Connector < right.Connector;
            });
}

std::vector<KeyedWalkway> keyedWalkways(const World &world, PathKind kind) {
  std::vector<KeyedWalkway> walkways;
  TickBuffers buffers;
  keyedWalkways(world, kind, walkways, buffers);
  return walkways;
}

namespace {

// The keyed walkways connectorWalkways reads, kept in the buffers.
struct KeyedWalkwayList {
  std::vector<KeyedWalkway> Keyed;
};

} // namespace

void connectorWalkways(const World &world, PathKind kind, std::vector<Walkway> &walkways,
                       TickBuffers &buffers) {
  std::vector<KeyedWalkway> &keyed = buffers.get<KeyedWalkwayList>().Keyed;
  keyed.clear();
  keyedWalkways(world, kind, keyed, buffers);
  for (const KeyedWalkway &walkway : keyed) {
    walkways.push_back(walkway.Way);
  }
}

std::vector<Walkway> connectorWalkways(const World &world, PathKind kind) {
  const std::vector<KeyedWalkway> keyed = keyedWalkways(world, kind);
  std::vector<Walkway> walkways;
  walkways.reserve(keyed.size());
  for (const KeyedWalkway &walkway : keyed) {
    walkways.push_back(walkway.Way);
  }
  return walkways;
}

const Network &parkNetwork(const World &world, PathKind kind) {
  static const Network EMPTY;
  const Network *network = world.tryGet<Network>(networkKey(kind));
  return network != nullptr ? *network : EMPTY;
}

void addRoutes(WorldSchema &schema) {
  schema.addComponent<PreviousNetwork>("previous-network", DataKind::Derived);
  addConnectionUnits(schema);
  schema.addResolver("path-networks", &resolvePathNetworks, {"paved-surface"});
  addWalkedSurface(schema);
  addRouteDistance(schema);
}

void addDropPreviousNetworks(WorldSchema &schema) { schema.addFinisher(&dropPreviousNetworks); }

} // namespace tpj
