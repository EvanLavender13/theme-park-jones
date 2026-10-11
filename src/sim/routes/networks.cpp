#include "sim/routes/networks.h"

#include "sim/park/geometry.h"
#include "sim/routes/route_distance.h"
#include "sim/schema.h"
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
// direction for the front and against it for the back. The door is as wide as the size. Nothing
// when the pose has no footprint.
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
                   .Out = {footprint->Forward.X * out, footprint->Forward.Z * out},
                   .Width = size.Width});
}

// The doors serving the kind: an entrance's front, a shop's, a bench's, and a toilet block's front
// serve guests, and a shop's back and a depot's front serve backstage.
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
    const bool facility = box.Kind == BoxKind::Bench || box.Kind == BoxKind::ToiletBlock;
    if (kind == PathKind::Guest && (shop || facility)) {
      addDoor(doors, box.Key, box.At, boxSize(box.Kind), Face::Front);
    } else if (kind == PathKind::Backstage && (shop || box.Kind == BoxKind::Depot)) {
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

// Each owner of the kind's pieces with the least distance from the point to any of its pieces, in
// ascending key order of the owner. The pieces are in ascending key order of their owner.
void ownerDistances(std::vector<std::pair<EntityKey, double>> &out,
                    const std::vector<SurfacePiece> &pieces, PathKind kind, ParkPoint point) {
  out.clear();
  for (const SurfacePiece &piece : pieces) {
    if (piece.Kind != kind) {
      continue;
    }
    const double distance = pieceDistance(piece, point);
    if (!out.empty() && out.back().first == piece.Owner) {
      out.back().second = std::min(out.back().second, distance);
    } else {
      out.emplace_back(piece.Owner, distance);
    }
  }
}

// The least distance the owners give the key, or none when they don't hold it.
std::optional<double> distanceOf(const std::vector<std::pair<EntityKey, double>> &owners,
                                 EntityKey key) {
  const auto found =
      std::ranges::lower_bound(owners, key, {}, &std::pair<EntityKey, double>::first);
  if (found == owners.end() || found->first != key) {
    return std::nullopt;
  }
  return found->second;
}

// Sets each carrier's paving distance from the owners' distances: the least that its own key and
// the keys of the points it ends at give, or none. Returns the least over every carrier, or none.
std::optional<double> pavingDistances(std::vector<std::optional<double>> &paving,
                                      const PathCarriers &paths,
                                      const std::vector<std::pair<EntityKey, double>> &owners) {
  std::optional<double> least;
  for (size_t i = 0; i < paths.Carriers.size(); ++i) {
    std::optional<double> own;
    for (const EntityKey owner :
         {paths.Carriers[i].Key, paths.Ends[i].first, paths.Ends[i].second}) {
      const std::optional<double> distance = distanceOf(owners, owner);
      if (distance && (!own || *distance < *own)) {
        own = distance;
      }
    }
    if (own && (!least || *own < *least)) {
      least = own;
    }
    paving[i] = own;
  }
  return least;
}

// Where a door meets the line of the carrier it connects to: the place, its ground point, and its
// distance from the door.
struct LineMeeting {
  Place At;
  GroundPoint Point;
  double Reach = 0.0;
};

// Of the carriers whose paving lies within JUNCTION_TOLERANCE of the least, where the door meets
// the line that lies nearest it, the first in key order winning a tie.
std::optional<LineMeeting> nearestLine(const Network &lines, const std::vector<Carrier> &carriers,
                                       const std::vector<std::optional<double>> &paving,
                                       double least, ParkPoint door) {
  const GroundPoint at{door.X, door.Z};
  std::optional<LineMeeting> nearest;
  for (size_t i = 0; i < carriers.size(); ++i) {
    const std::optional<double> &own = paving[i];
    if (!own || *own > least + JUNCTION_TOLERANCE) {
      continue;
    }
    const std::optional<Place> on = lines.nearestPlaceOn(carriers[i].Key, at);
    const std::optional<GroundPoint> ground = on ? lines.groundPoint(*on) : std::nullopt;
    if (!on || !ground) {
      continue;
    }
    const double dx = ground->X - at.X;
    const double dz = ground->Z - at.Z;
    const double distance = std::sqrt((dx * dx) + (dz * dz));
    if (!nearest || distance < nearest->Reach) {
      nearest = LineMeeting{*on, *ground, distance};
    }
  }
  return nearest;
}

// How far along the ray from the point in the unit direction it first meets a piece of the kind,
// within the limit, or none. 0 when the point lies on one.
std::optional<double> rayEntry(const std::vector<SurfacePiece> &pieces, PathKind kind,
                               ParkPoint from, ParkPoint direction, double limit) {
  std::optional<double> least;
  for (const SurfacePiece &piece : pieces) {
    if (piece.Kind != kind) {
      continue;
    }
    const std::span<const ParkPoint> corners = pieceCorners(piece);
    double enter = 0.0;
    double leave = limit;
    bool missed = corners.empty();
    for (size_t i = 0; !missed && i < corners.size(); ++i) {
      const ParkPoint &corner = corners[i];
      const ParkPoint &next = corners[(i + 1) % corners.size()];
      const double sideX = next.X - corner.X;
      const double sideZ = next.Z - corner.Z;
      const double length = std::sqrt((sideX * sideX) + (sideZ * sideZ));
      if (length < 1e-9) {
        continue;
      }
      // The inside of a counter-clockwise piece lies to the left of each side.
      const double inwardX = sideZ / length;
      const double inwardZ = -sideX / length;
      const double start = ((from.X - corner.X) * inwardX) + ((from.Z - corner.Z) * inwardZ);
      const double change = (direction.X * inwardX) + (direction.Z * inwardZ);
      if (change == 0.0) {
        missed = start < 0.0;
      } else if (change > 0.0) {
        enter = std::max(enter, -start / change);
      } else {
        leave = std::min(leave, -start / change);
      }
      missed = missed || enter > leave;
    }
    if (!missed && (!least || enter < *least)) {
      least = enter;
    }
  }
  return least;
}

// The connector's points from the door to where it meets the carrier, and the distance along the
// carrier there. It runs straight out of the door's face to the paving of the kind, then to the
// carrier's line, unless the meeting already lies straight out of the face, the ray meets no
// paving within reach, or the door stands on paving.
std::pair<std::vector<CarrierPoint>, double> connectorRoute(const Network &lines,
                                                            const std::vector<SurfacePiece> &pieces,
                                                            PathKind kind, const ParkDoor &door,
                                                            const LineMeeting &meeting) {
  std::vector<CarrierPoint> points{
      {.X = door.At.X, .Z = door.At.Z, .Distance = 0.0},
      {.X = meeting.Point.X, .Z = meeting.Point.Z, .Distance = meeting.Reach}};
  const double ahead = (((meeting.Point.X - door.At.X) * door.Out.X) +
                        ((meeting.Point.Z - door.At.Z) * door.Out.Z)) /
                       meeting.Reach;
  if (ahead >= 1.0 - 1e-9) {
    return {points, meeting.At.Distance};
  }
  const std::optional<double> entry =
      rayEntry(pieces, kind, door.At, door.Out, CONNECTION_REACH + JUNCTION_TOLERANCE);
  if (!entry || !(*entry > 0.0)) {
    return {points, meeting.At.Distance};
  }
  const GroundPoint at{door.At.X + (door.Out.X * *entry), door.At.Z + (door.Out.Z * *entry)};
  const std::optional<Place> on = lines.nearestPlaceOn(meeting.At.Carrier, at);
  const std::optional<GroundPoint> ground = on ? lines.groundPoint(*on) : std::nullopt;
  if (!on || !ground) {
    return {points, meeting.At.Distance};
  }
  const double dx = ground->X - at.X;
  const double dz = ground->Z - at.Z;
  const double leg = std::sqrt((dx * dx) + (dz * dz));
  points[1] = {.X = at.X, .Z = at.Z, .Distance = *entry};
  if (leg <= JUNCTION_TOLERANCE) {
    points[1] = {.X = ground->X, .Z = ground->Z, .Distance = *entry};
  } else {
    points.push_back({.X = ground->X, .Z = ground->Z, .Distance = *entry + leg});
  }
  return {points, on->Distance};
}

// The connectors of the doors, each as wide as the kind's path or its door, whichever is narrower.
// A door connects to the path carrier whose paving lies nearest, when that paving lies within
// reach. A carrier's paving is the pieces its segment owns and those of the points it ends at. Of
// carriers whose paving lies equally near, the one whose line lies nearest wins, then the lower
// key.
std::vector<Connection> findConnections(const PathCarriers &paths,
                                        const std::vector<SurfacePiece> &pieces, PathKind kind,
                                        const std::vector<ParkDoor> &doors) {
  const std::vector<Carrier> &carriers = paths.Carriers;
  std::vector<Connection> connections;
  if (carriers.empty()) {
    return connections;
  }
  const Network lines = linesOnly(carriers);
  std::vector<std::pair<EntityKey, double>> owners;
  std::vector<std::optional<double>> paving(carriers.size());
  for (const ParkDoor &door : doors) {
    ownerDistances(owners, pieces, kind, door.At);
    const std::optional<double> least = pavingDistances(paving, paths, owners);
    if (!least || *least > CONNECTION_REACH) {
      continue;
    }
    const std::optional<LineMeeting> meeting =
        nearestLine(lines, carriers, paving, *least, door.At);
    if (!meeting || !(meeting->Reach > JUNCTION_TOLERANCE)) {
      continue;
    }
    // A path keyed like the connector, which only a hand-written save holds, keeps carrier keys
    // unique by leaving the door unconnected.
    const EntityKey key = connectorKey(door.Entity, door.Side);
    if (std::ranges::binary_search(carriers, key, {}, &Carrier::Key)) {
      continue;
    }
    const auto path = std::ranges::lower_bound(carriers, meeting->At.Carrier, {}, &Carrier::Key);
    auto [points, pathDistance] = connectorRoute(lines, pieces, kind, door, *meeting);
    connections.push_back({.Connector = {.Key = key,
                                         .Width = std::min(pathWidth(kind), door.Width),
                                         .Points = std::move(points),
                                         .Stops = {}},
                           .Entity = door.Entity,
                           .Path = static_cast<size_t>(path - carriers.begin()),
                           .PathDistance = pathDistance});
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

Network deriveNetwork(const std::vector<ParkSegment> &segments,
                      const std::vector<SurfacePiece> &pieces, const std::vector<ParkDoor> &doors,
                      PathKind kind) {
  PathCarriers paths = pathCarriers(segments, kind);
  std::vector<Meeting> meetings = pointMeetings(paths);
  // Each connector meets its path at its connection.
  std::vector<Connection> connections = findConnections(paths, pieces, kind, doors);
  std::vector<Carrier> carriers = std::move(paths.Carriers);
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
  const PavedSurface &surface = resolvedSurface(world);
  for (const PathKind kind : {PathKind::Guest, PathKind::Backstage}) {
    Network network =
        deriveNetwork(segments, surface.Pieces, doorsServing(entrances, boxes, kind), kind);
    const EntityKey key =
        world.createDerivedEntity(NULL_KEY, NETWORK_PURPOSE, static_cast<uint64_t>(kind));
    if (Network *held = world.tryGet<Network>(key)) {
      world.emplaceOrReplace<PreviousNetwork>(key, PreviousNetwork{std::move(*held)});
    }
    world.emplaceOrReplace<Network>(key, std::move(network));
  }
}

// Derived, on its own entity: the park's paved surface with every connector's walkway.
struct WalkedSurface {
  PavedSurface Held;
};

template <typename Visitor> void visitFields(Visitor &visitor, WalkedSurface &walked) {
  visitor.field("surface", walked.Held);
}

inline constexpr uint64_t WALKED_SURFACE_PURPOSE = hashName("walked-surface");

void resolveWalkedSurface(World &world) {
  std::vector<Walkway> walkways = connectorWalkways(world, PathKind::Guest);
  for (const Walkway &walkway : connectorWalkways(world, PathKind::Backstage)) {
    walkways.push_back(walkway);
  }
  const EntityKey key = world.createDerivedEntity(NULL_KEY, WALKED_SURFACE_PURPOSE, 0);
  world.emplaceOrReplace<WalkedSurface>(key, WalkedSurface{parkSurface(world, walkways)});
}

// Removes both kinds' previous networks, so no resolution ends holding one.
void dropPreviousNetworks(World &world) {
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
  return doorsServing(parkEntrances(world), parkBoxes(world), kind);
}

std::vector<KeyedWalkway> keyedWalkways(const World &world, PathKind kind) {
  const Network &network = parkNetwork(world, kind);
  const std::vector<ParkDoor> doors = parkDoors(world, kind);
  // Only connectors' doors are anchored, so a carrier whose first stop is anchored is a connector.
  const auto isConnector = [&network](const Carrier &carrier) {
    return !carrier.Stops.empty() && network.nodeAnchor(carrier.Stops.front().Node) != NULL_KEY;
  };
  std::vector<KeyedWalkway> walkways;
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
    // A connector that turns at the paving gives a walkway along its first part only, reaching
    // half its width into the paving, or to the line if that is nearer.
    ParkPoint meets{carrier.Points.back().X, carrier.Points.back().Z};
    if (carrier.Points.size() > 2) {
      const CarrierPoint &entry = carrier.Points[1];
      const double into =
          std::min(0.5 * carrier.Width, carrier.Points.back().Distance - entry.Distance);
      meets = {entry.X + (door->Out.X * into), entry.Z + (door->Out.Z * into)};
    }
    walkways.push_back(KeyedWalkway{.Connector = carrier.Key,
                                    .Way = Walkway{.Kind = kind,
                                                   .Joins = segment,
                                                   .Door = door->At,
                                                   .Out = door->Out,
                                                   .Meets = meets,
                                                   .Width = carrier.Width}});
  }
  return walkways;
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

const PavedSurface &walkedSurface(const World &world) {
  static const PavedSurface EMPTY;
  const WalkedSurface *walked =
      world.tryGet<WalkedSurface>(deriveKey(NULL_KEY, WALKED_SURFACE_PURPOSE, 0));
  return walked != nullptr ? walked->Held : EMPTY;
}

const Network &parkNetwork(const World &world, PathKind kind) {
  static const Network EMPTY;
  const Network *network = world.tryGet<Network>(networkKey(kind));
  return network != nullptr ? *network : EMPTY;
}

void addRoutes(WorldSchema &schema) {
  schema.addComponent<PreviousNetwork>("previous-network", DataKind::Derived);
  schema.addComponent<WalkedSurface>("walked-surface", DataKind::Derived);
  schema.addResolver("path-networks", &resolvePathNetworks, {"paved-surface"});
  schema.addResolver("walked-surface", &resolveWalkedSurface, {"path-networks"});
  addRouteDistance(schema);
}

void addDropPreviousNetworks(WorldSchema &schema) { schema.addFinisher(&dropPreviousNetworks); }

} // namespace tpj
