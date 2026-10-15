#include "sim/routes/internal/connections.h"

#include "sim/park/geometry.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <span>
#include <stddef.h>
#include <utility>
#include <vector>

namespace tpj {

namespace {

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

} // namespace

void doorsServing(std::span<const ParkEntrance> entrances, std::span<const ParkBox> boxes,
                  PathKind kind, std::vector<ParkDoor> &doors) {
  doors.clear();
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
}

void groupStarts(std::vector<double> &distances, std::vector<double> &starts) {
  std::ranges::sort(distances);
  starts.clear();
  for (const double distance : distances) {
    if (starts.empty() || distance - starts.back() > JUNCTION_TOLERANCE) {
      starts.push_back(distance);
    }
  }
}

size_t groupHolding(std::span<const double> starts, double distance) {
  return static_cast<size_t>(std::ranges::upper_bound(starts, distance) - starts.begin()) - 1;
}

namespace {

// Where a door meets the line of the path it connects to: the place, its ground point, and its
// distance from the door.
struct LineMeeting {
  Place At;
  GroundPoint Point;
  double Reach = 0.0;
};

// A path that may take a door's connection: its segment, and how far the door lies from its paving,
// or none.
struct Candidate {
  const ParkSegment *Segment = nullptr;
  std::optional<double> Paving;
};

// The lists deriving a door's connections reuses: the entity's doors, the pieces near a door, each
// owner's least distance from it, the candidate paths, and the connector's points.
struct ConnectionLists {
  std::vector<ParkDoor> Doors;
  std::vector<const SurfacePiece *> Pieces;
  std::vector<std::pair<EntityKey, double>> Owners;
  std::vector<Candidate> Candidates;
  std::vector<CarrierPoint> Points;
};

// How far along the ray from the point in the unit direction it enters the piece, within the
// limit, or none. 0 when the point lies on it.
std::optional<double> pieceEntry(const SurfacePiece &piece, ParkPoint from, ParkPoint direction,
                                 double limit) {
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
  if (missed) {
    return std::nullopt;
  }
  return enter;
}

// How far along the ray from the point in the unit direction it first meets one of the pieces,
// within the limit, or none. 0 when the point lies on one.
std::optional<double> rayEntry(std::span<const SurfacePiece *const> pieces, ParkPoint from,
                               ParkPoint direction, double limit) {
  std::optional<double> least;
  for (const SurfacePiece *piece : pieces) {
    const std::optional<double> enter = pieceEntry(*piece, from, direction, limit);
    if (enter && (!least || *enter < *least)) {
      least = enter;
    }
  }
  return least;
}

// Where the point meets the line of the segment nearest it: the place, its ground point, and its
// distance from the point. None when the line gives none.
std::optional<LineMeeting> meetingOn(const ParkSegment &segment, GroundPoint at) {
  const std::optional<Place> on = nearestPlaceOnLine(segment.Key, segment.Line, at);
  const std::optional<GroundPoint> ground =
      on ? groundPointOnLine(segment.Line, on->Distance) : std::nullopt;
  if (!on || !ground) {
    return std::nullopt;
  }
  const double dx = ground->X - at.X;
  const double dz = ground->Z - at.Z;
  return LineMeeting{*on, *ground, std::sqrt((dx * dx) + (dz * dz))};
}

// Puts in points the connector's points from the door to where it meets the path, and returns the
// distance along the path there. It runs straight out of the door's face to the paving of the
// kind, then to the path's line, unless the meeting already lies straight out of the face, the ray
// meets no paving within reach, or the door stands on paving. The pieces are those of the kind near
// the door.
double connectorRoute(const ParkSegment &path, std::span<const SurfacePiece *const> pieces,
                      const ParkDoor &door, const LineMeeting &meeting,
                      std::vector<CarrierPoint> &points) {
  points.clear();
  points.push_back({.X = door.At.X, .Z = door.At.Z, .Distance = 0.0});
  points.push_back({.X = meeting.Point.X, .Z = meeting.Point.Z, .Distance = meeting.Reach});
  const double ahead = (((meeting.Point.X - door.At.X) * door.Out.X) +
                        ((meeting.Point.Z - door.At.Z) * door.Out.Z)) /
                       meeting.Reach;
  if (ahead >= 1.0 - 1e-9) {
    return meeting.At.Distance;
  }
  const std::optional<double> entry =
      rayEntry(pieces, door.At, door.Out, CONNECTION_REACH + JUNCTION_TOLERANCE);
  if (!entry || !(*entry > 0.0)) {
    return meeting.At.Distance;
  }
  const GroundPoint at{door.At.X + (door.Out.X * *entry), door.At.Z + (door.Out.Z * *entry)};
  const std::optional<Place> on = nearestPlaceOnLine(path.Key, path.Line, at);
  const std::optional<GroundPoint> ground =
      on ? groundPointOnLine(path.Line, on->Distance) : std::nullopt;
  if (!on || !ground) {
    return meeting.At.Distance;
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
  return on->Distance;
}

// The least distance the owners, ascending by key, give the key, or none when they don't hold it.
std::optional<double> distanceOf(const std::vector<std::pair<EntityKey, double>> &owners,
                                 EntityKey key) {
  const auto found =
      std::ranges::lower_bound(owners, key, {}, &std::pair<EntityKey, double>::first);
  if (found == owners.end() || found->first != key) {
    return std::nullopt;
  }
  return found->second;
}

// Adds the segment the key holds as a candidate, when it is of the kind and has a ground line.
void addCandidate(UnitReads &reads, EntityKey key, PathKind kind,
                  std::vector<Candidate> &candidates) {
  const ParkSegment *segment = groundSegmentOf(reads, key);
  if (segment != nullptr && segment->Kind == kind && !segment->Line.empty()) {
    candidates.push_back(Candidate{segment, std::nullopt});
  }
}

// Puts in lists.Pieces the pieces of the door's kind near it, in lists.Owners each of their owners
// with its least distance from the door, ascending by owner, and in lists.Candidates the segments
// of the kind with ground lines whose own or whose points' pieces are among them, ascending by key,
// each with its paving distance. Returns the least paving distance, or none.
std::optional<double> nearPaving(UnitReads &reads, const ParkDoor &door, PathKind kind,
                                 ConnectionLists &lists) {
  constexpr double REACH = CONNECTION_REACH + JUNCTION_TOLERANCE;
  pavingNear(reads, {door.At.X - REACH, door.At.Z - REACH}, {door.At.X + REACH, door.At.Z + REACH},
             lists.Pieces);
  std::erase_if(lists.Pieces, [kind](const SurfacePiece *piece) { return piece->Kind != kind; });
  lists.Owners.clear();
  for (const SurfacePiece *piece : lists.Pieces) {
    lists.Owners.emplace_back(piece->Owner, pieceDistance(*piece, door.At));
  }
  std::ranges::sort(lists.Owners);
  // Sorting puts each owner's least distance first.
  const auto repeats = std::ranges::unique(lists.Owners, {}, &std::pair<EntityKey, double>::first);
  lists.Owners.erase(repeats.begin(), repeats.end());

  lists.Candidates.clear();
  for (const auto &[owner, distance] : lists.Owners) {
    if (groundSegmentOf(reads, owner) != nullptr) {
      addCandidate(reads, owner, kind, lists.Candidates);
      continue;
    }
    for (const EntityKey segment : segmentsAt(reads, owner)) {
      addCandidate(reads, segment, kind, lists.Candidates);
    }
  }
  std::ranges::sort(lists.Candidates, {}, [](const Candidate &c) { return c.Segment->Key; });
  const auto same =
      std::ranges::unique(lists.Candidates, {}, [](const Candidate &c) { return c.Segment->Key; });
  lists.Candidates.erase(same.begin(), same.end());

  std::optional<double> least;
  for (Candidate &candidate : lists.Candidates) {
    const ParkSegment &segment = *candidate.Segment;
    for (const EntityKey owner : {segment.Key, segment.From, segment.To}) {
      const std::optional<double> distance = distanceOf(lists.Owners, owner);
      if (distance && (!candidate.Paving || *distance < *candidate.Paving)) {
        candidate.Paving = distance;
      }
    }
    if (candidate.Paving && (!least || *candidate.Paving < *least)) {
      least = candidate.Paving;
    }
  }
  return least;
}

// The door's connection, from the paving near it, or none: to the candidate whose paving lies
// nearest within CONNECTION_REACH, the one whose line lies nearest winning among those equally
// near, within JUNCTION_TOLERANCE, then the lower key.
void connectDoor(UnitReads &reads, const ParkDoor &door, PathKind kind, ConnectionLists &lists,
                 DoorConnectionList &connections) {
  const std::optional<double> least = nearPaving(reads, door, kind, lists);
  if (!least || *least > CONNECTION_REACH) {
    return;
  }
  const GroundPoint at{door.At.X, door.At.Z};
  const ParkSegment *path = nullptr;
  std::optional<LineMeeting> meeting;
  for (const Candidate &candidate : lists.Candidates) {
    if (!candidate.Paving || *candidate.Paving > *least + JUNCTION_TOLERANCE) {
      continue;
    }
    const std::optional<LineMeeting> on = meetingOn(*candidate.Segment, at);
    if (on && (!meeting || on->Reach < meeting->Reach)) {
      meeting = on;
      path = candidate.Segment;
    }
  }
  if (!meeting || !(meeting->Reach > JUNCTION_TOLERANCE)) {
    return;
  }
  // A path keyed like the connector, which only a hand-written save holds, keeps carrier keys
  // unique by leaving the door unconnected.
  const ParkSegment *clash = groundSegmentOf(reads, connectorKey(door.Entity, door.Side));
  if (clash != nullptr && clash->Kind == kind && !clash->Line.empty()) {
    return;
  }
  const double pathDistance = connectorRoute(*path, lists.Pieces, door, *meeting, lists.Points);
  connections.Doors.push_back(DoorConnection{.Door = door,
                                             .Kind = kind,
                                             .First = connections.Points.size(),
                                             .Count = lists.Points.size(),
                                             .Width = std::min(pathWidth(kind), door.Width),
                                             .Path = path->Key,
                                             .PathDistance = pathDistance});
  connections.Points.insert(connections.Points.end(), lists.Points.begin(), lists.Points.end());
}

// The entity a key holds as an entrance list and a box list of at most one each.
struct EntityLists {
  std::vector<ParkEntrance> Entrances;
  std::vector<ParkBox> Boxes;
};

} // namespace

void DoorConnections::derive(UnitReads &reads, EntityKey key, DoorConnectionList &connections) {
  ConnectionLists &lists = reads.scratch().get<ConnectionLists>();
  EntityLists &entity = reads.scratch().get<EntityLists>();
  entity.Entrances.clear();
  entity.Boxes.clear();
  if (const ParkEntrance *entrance = entranceOf(reads, key)) {
    entity.Entrances.push_back(*entrance);
  }
  if (const ParkBox *box = boxOf(reads, key)) {
    entity.Boxes.push_back(*box);
  }
  connections.Doors.clear();
  connections.Points.clear();
  for (const PathKind kind : {PathKind::Guest, PathKind::Backstage}) {
    doorsServing(entity.Entrances, entity.Boxes, kind, lists.Doors);
    for (const ParkDoor &door : lists.Doors) {
      connectDoor(reads, door, kind, lists, connections);
    }
  }
}

namespace {

// Files each entity under the paths its connections meet.
struct ConnectionPaths {
  static constexpr std::string_view Name = "connection-paths";
  using Of = DoorConnections;

  static void keysOf(const DoorConnectionList &connections, std::vector<EntityKey> &keys) {
    keys.clear();
    for (const DoorConnection &connection : connections.Doors) {
      keys.push_back(connection.Path);
    }
  }
};

// The lists deriving walkways reuses: a path's stop distances and their groups' starts, the
// segments at the node's points and the points still to visit.
struct WalkwayLists {
  std::vector<double> Distances;
  std::vector<double> Starts;
  std::vector<EntityKey> Points;
  std::vector<EntityKey> Visit;
};

// Adds the point to visit, if it hasn't been.
void visitPoint(EntityKey point, WalkwayLists &lists) {
  if (std::ranges::find(lists.Points, point) == lists.Points.end()) {
    lists.Points.push_back(point);
    lists.Visit.push_back(point);
  }
}

// The lowest key among the path and the segments at the node where the connection meets it: the
// path's From point's segments when its stop group is the path's first, and its To point's when it
// is the last, each of the kind with a ground line. A segment no longer than JUNCTION_TOLERANCE
// stops at one node at both ends, so the segments at its far point are there too.
EntityKey joinedSegment(UnitReads &reads, const DoorConnection &connection, const ParkSegment &path,
                        WalkwayLists &lists) {
  lists.Distances.clear();
  lists.Distances.push_back(0.0);
  lists.Distances.push_back(path.Line.back().Distance);
  for (const EntityKey entity : reads.filed<ConnectionPaths>(path.Key)) {
    for (const DoorConnection &other : reads.unit<DoorConnections>(entity)->Doors) {
      if (other.Path == path.Key) {
        lists.Distances.push_back(other.PathDistance);
      }
    }
  }
  groupStarts(lists.Distances, lists.Starts);
  const size_t group = groupHolding(lists.Starts, connection.PathDistance);
  lists.Points.clear();
  lists.Visit.clear();
  if (group == 0) {
    visitPoint(path.From, lists);
  }
  if (group + 1 == lists.Starts.size()) {
    visitPoint(path.To, lists);
  }
  EntityKey joins = path.Key;
  while (!lists.Visit.empty()) {
    const EntityKey point = lists.Visit.back();
    lists.Visit.pop_back();
    for (const EntityKey key : segmentsAt(reads, point)) {
      const ParkSegment *segment = groundSegmentOf(reads, key);
      if (segment == nullptr || segment->Kind != connection.Kind || segment->Line.empty()) {
        continue;
      }
      joins = std::min(joins, key);
      if (segment->Line.back().Distance <= JUNCTION_TOLERANCE) {
        visitPoint(segment->From == point ? segment->To : segment->From, lists);
      }
    }
  }
  return joins;
}

} // namespace

void DoorWalkways::derive(UnitReads &reads, EntityKey key, DoorWalkwayList &walkways) {
  WalkwayLists &lists = reads.scratch().get<WalkwayLists>();
  const DoorConnectionList &connections = *reads.unit<DoorConnections>(key);
  walkways.Ways.resize(connections.Doors.size());
  for (size_t i = 0; i < connections.Doors.size(); ++i) {
    const DoorConnection &connection = connections.Doors[i];
    const ParkSegment &path = *groundSegmentOf(reads, connection.Path);
    const EntityKey joins = joinedSegment(reads, connection, path, lists);
    const std::span<const CarrierPoint> points = connectorPoints(connections, connection);
    // A connector that turns at the paving gives a walkway along its first part only, reaching
    // half its width into the paving, or to the line if that is nearer.
    ParkPoint meets{points.back().X, points.back().Z};
    if (points.size() > 2) {
      const CarrierPoint &entry = points[1];
      const double into = std::min(0.5 * connection.Width, points.back().Distance - entry.Distance);
      meets = {entry.X + (connection.Door.Out.X * into), entry.Z + (connection.Door.Out.Z * into)};
    }
    DoorWalkway &walkway = walkways.Ways[i];
    walkway.Connector = connectorKey(connection.Door.Entity, connection.Door.Side);
    walkway.Way = Walkway{.Kind = connection.Kind,
                          .Joins = joins,
                          .Door = connection.Door.At,
                          .Out = connection.Door.Out,
                          .Meets = meets,
                          .Width = connection.Width};
    const ParkSegment &joined = *groundSegmentOf(reads, joins);
    const std::optional<SurfacePiece> piece =
        walkwayPiece(std::span<const ParkSegment>(&joined, 1), walkway.Way);
    walkway.HasPiece = piece.has_value();
    walkway.Piece = piece.value_or(SurfacePiece{});
  }
}

void WalkwayCells::keysOf(const DoorWalkwayList &walkways, std::vector<EntityKey> &keys) {
  keys.clear();
  for (const DoorWalkway &walkway : walkways.Ways) {
    if (!walkway.HasPiece) {
      continue;
    }
    const std::span<const ParkPoint> corners = pieceCorners(walkway.Piece);
    ParkPoint least = corners.front();
    ParkPoint most = corners.front();
    for (const ParkPoint &corner : corners) {
      least = {std::min(least.X, corner.X), std::min(least.Z, corner.Z)};
      most = {std::max(most.X, corner.X), std::max(most.Z, corner.Z)};
    }
    pavingCells(least, most, keys);
  }
}

void WalkwayJoins::keysOf(const DoorWalkwayList &walkways, std::vector<EntityKey> &keys) {
  keys.clear();
  for (const DoorWalkway &walkway : walkways.Ways) {
    if (walkway.HasPiece) {
      keys.push_back(walkway.Way.Joins);
    }
  }
}

void addConnectionUnits(WorldSchema &schema) {
  addUnits<DoorConnections>(schema);
  addUnitIndex<ConnectionPaths>(schema);
  addUnits<DoorWalkways>(schema);
  addUnitIndex<WalkwayCells>(schema);
  addUnitIndex<WalkwayJoins>(schema);
}

} // namespace tpj
