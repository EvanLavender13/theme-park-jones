#ifndef TPJ_SIM_ROUTES_INTERNAL_CONNECTIONS_H
#define TPJ_SIM_ROUTES_INTERNAL_CONNECTIONS_H

#include "sim/entity_key.h"
#include "sim/medium/network.h"
#include "sim/park/intent.h"
#include "sim/park/surface.h"
#include "sim/park/unit_reads.h"
#include "sim/routes/networks.h"
#include "sim/schema.h"
#include "sim/units.h"

#include <array>
#include <span>
#include <string_view>
#include <tuple>
#include <vector>

namespace tpj {

// Puts the doors serving the kind in doors, clearing it first: an entrance's front, a shop's, a
// bench's, and a toilet block's front serve guests, and a shop's back and a depot's front serve
// backstage.
void doorsServing(std::span<const ParkEntrance> entrances, std::span<const ParkBox> boxes,
                  PathKind kind, std::vector<ParkDoor> &doors);

// Puts in starts, clearing it first, the first distances of a carrier's stop groups: the
// distances sorted, which it sorts in place, each group taking every distance at most
// JUNCTION_TOLERANCE above its first.
void groupStarts(std::vector<double> &distances, std::vector<double> &starts);
// The index of the group holding the distance, which is at least the first start, 0.
size_t groupHolding(std::span<const double> starts, double distance);

// A door's connection to a path of the kind it serves: the door, the kind, the Count connector
// points from First of its list's Points, from the door to where it meets the path, its width, the
// key of the path it meets, and the distance along the path there.
struct DoorConnection {
  ParkDoor Door;
  PathKind Kind = PathKind::Guest;
  size_t First = 0;
  size_t Count = 0;
  double Width = 0.0;
  EntityKey Path = NULL_KEY;
  double PathDistance = 0.0;
};

template <typename Visitor> void visitFields(Visitor &visitor, DoorConnection &connection) {
  ParkDoor &door = connection.Door;
  visitor.field("entity", door.Entity);
  visitor.field("face", door.Side);
  visitor.field("door-x", door.At.X);
  visitor.field("door-z", door.At.Z);
  visitor.field("out-x", door.Out.X);
  visitor.field("out-z", door.Out.Z);
  visitor.field("door-width", door.Width);
  visitor.field("kind", connection.Kind);
  visitor.field("first", connection.First);
  visitor.field("count", connection.Count);
  visitor.field("width", connection.Width);
  visitor.field("path", connection.Path);
  visitor.field("path-distance", connection.PathDistance);
}

// The connections of an entity's doors, the guest door's first, and the points their connectors
// run through.
struct DoorConnectionList {
  std::vector<DoorConnection> Doors;
  std::vector<CarrierPoint> Points;
};

template <typename Visitor> void visitFields(Visitor &visitor, DoorConnectionList &list) {
  visitor.field("doors", list.Doors);
  visitor.field("points", list.Points);
}

// The points the connection's connector runs through.
inline std::span<const CarrierPoint> connectorPoints(const DoorConnectionList &list,
                                                     const DoorConnection &connection) {
  return std::span<const CarrierPoint>(list.Points).subspan(connection.First, connection.Count);
}

// Each door of a box or an entrance that lies within CONNECTION_REACH of the paving of a segment of
// the kind it serves, connected as the routes spec says, from only the paving near the door.
struct DoorConnections {
  static constexpr std::string_view Name = "door-connections";
  using Over = UnitsOf<BoxUnits, EntranceUnits>;
  using Value = DoorConnectionList;
  using Reads = std::tuple<>;

  static void derive(UnitReads &reads, EntityKey key, DoorConnectionList &connections);
};

// A connector's walkway: the connector's key, the walkway, its piece, when it has one, and the
// width of its door.
struct DoorWalkway {
  EntityKey Connector = NULL_KEY;
  Walkway Way;
  bool HasPiece = false;
  SurfacePiece Piece;
  double DoorWidth = 0.0;
};

template <typename Visitor> void visitFields(Visitor &visitor, DoorWalkway &walkway) {
  Walkway &way = walkway.Way;
  visitor.field("connector", walkway.Connector);
  visitor.field("kind", way.Kind);
  visitor.field("joins", way.Joins);
  visitor.field("door", way.Door);
  visitor.field("out", way.Out);
  visitor.field("meets", way.Meets);
  visitor.field("width", way.Width);
  visitor.field("has-piece", walkway.HasPiece);
  visitor.field("piece", walkway.Piece);
  visitor.field("door-width", walkway.DoorWidth);
}

// Puts in corners the walkway's frontage strip: from its door's face, as wide as the door, straight
// out to where the walkway ends. False when the strip has no depth.
bool frontageStrip(const DoorWalkway &walkway, std::array<ParkPoint, 4> &corners);

// The walkways of an entity's connections, in their order.
struct DoorWalkwayList {
  std::vector<DoorWalkway> Ways;
};

template <typename Visitor> void visitFields(Visitor &visitor, DoorWalkwayList &list) {
  visitor.field("ways", list.Ways);
}

// The walkway of each of an entity's connections, joined to the lowest-keyed segment at the node
// where its connector meets its path.
struct DoorWalkways {
  static constexpr std::string_view Name = "door-walkways";
  using Over = UnitsOf<BoxUnits, EntranceUnits>;
  using Value = DoorWalkwayList;
  using Reads = std::tuple<>;

  static void derive(UnitReads &reads, EntityKey key, DoorWalkwayList &walkways);
};

// Files each entity under the cells its walkways' pieces cover.
struct WalkwayCells {
  static constexpr std::string_view Name = "walkway-cells";
  using Of = DoorWalkways;

  static void keysOf(const DoorWalkwayList &walkways, std::vector<EntityKey> &keys);
};

// Files each entity under the cells its walkways' frontage strips cover.
struct FrontageCells {
  static constexpr std::string_view Name = "frontage-cells";
  using Of = DoorWalkways;

  static void keysOf(const DoorWalkwayList &walkways, std::vector<EntityKey> &keys);
};

// Files each entity under the segments its walkways with pieces join.
struct WalkwayJoins {
  static constexpr std::string_view Name = "walkway-joins";
  using Of = DoorWalkways;

  static void keysOf(const DoorWalkwayList &walkways, std::vector<EntityKey> &keys);
};

// Registers door-connections, connection-paths, door-walkways, walkway-cells, frontage-cells, and
// walkway-joins.
void addConnectionUnits(WorldSchema &schema);

} // namespace tpj

#endif
