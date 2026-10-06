#ifndef TPJ_SIM_PARK_INTENT_H
#define TPJ_SIM_PARK_INTENT_H

#include "sim/entity_key.h"
#include "sim/medium/network.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <array>
#include <memory>
#include <stdint.h>
#include <string_view>
#include <vector>

namespace tpj {

// The park is a square this many meters on a side, centered on the origin of the ground.
inline constexpr double PARK_SIZE = 256.0;

// A position on the ground, in meters.
struct ParkPoint {
  double X = 0.0;
  double Z = 0.0;

  bool operator==(const ParkPoint &) const = default;
};

template <typename Visitor> void visitFields(Visitor &visitor, ParkPoint &point) {
  visitor.field("x", point.X);
  visitor.field("z", point.Z);
}

enum class PathKind : uint8_t { Guest, Backstage };

constexpr std::array<std::string_view, 2> enumNames(PathKind /*value*/) {
  return {"guest", "backstage"};
}

enum class BoxKind : uint8_t { Shop, Depot };

constexpr std::array<std::string_view, 2> enumNames(BoxKind /*value*/) { return {"shop", "depot"}; }

// A position and a facing, a direction on the ground held as given, not normalized.
struct Pose {
  double X = 0.0;
  double Z = 0.0;
  double FacingX = 0.0;
  double FacingZ = -1.0;

  bool operator==(const Pose &) const = default;
};

// A footprint's width, across its facing, and depth, along it, in meters.
struct FootprintSize {
  double Width = 0.0;
  double Depth = 0.0;
};

// The width a new segment of the kind takes, in meters. Until navigable-networks' wide-routes,
// routes gives every carrier of the kind this width.
constexpr double pathWidth(PathKind kind) { return kind == PathKind::Guest ? 3.0 : 2.0; }

// The corner radius a new path point takes, in meters.
inline constexpr double DEFAULT_CORNER_RADIUS = 0.0;

// The edge radius a new segment end takes, in meters: a sharp edge.
inline constexpr double DEFAULT_EDGE_RADIUS = 0.0;

// The taper length a new path point takes, in meters: a step.
inline constexpr double DEFAULT_TAPER_LENGTH = 0.0;

constexpr FootprintSize boxSize(BoxKind kind) {
  return kind == BoxKind::Shop ? FootprintSize{8.0, 6.0} : FootprintSize{12.0, 8.0};
}

inline constexpr FootprintSize ENTRANCE_SIZE{10.0, 3.0};

// An entrance's intent, as other modules read it.
struct ParkEntrance {
  EntityKey Key = NULL_KEY;
  Pose At;

  bool operator==(const ParkEntrance &) const = default;
};

// A path point's intent, as other modules read it: its ground position, the radius of the corner
// it makes when exactly two segments meet there, and the length over which the narrower of two
// segments curves out to the wider one's side where they meet there, or 0 for its natural length.
struct ParkPathPoint {
  EntityKey Key = NULL_KEY;
  ParkPoint At;
  double Radius = 0.0;
  double Taper = DEFAULT_TAPER_LENGTH;

  bool operator==(const ParkPathPoint &) const = default;
};

// The corner a path point makes where exactly two segments meet, as other modules read it: its
// point's key and position, the unit direction along each of its two segments, the lower-keyed
// segment's first, and the unit direction into the corner, halfway between them. Radius is the
// radius the corner is rounded with, at most MaxRadius, the largest that fits. Middle is the
// arc's middle, where the two segments' ground lines meet.
struct ParkCorner {
  EntityKey Point = NULL_KEY;
  ParkPoint At;
  std::array<ParkPoint, 2> Along;
  ParkPoint Toward;
  double Radius = 0.0;
  double MaxRadius = 0.0;
  ParkPoint Middle;

  bool operator==(const ParkCorner &) const = default;
};

// A path segment's intent, as other modules read it: its kind, its two points, its width, its
// ground line, which is empty when the segment has no physical shape, and its edge radius at each
// end, for the edge that starts from its left side there, seen walking away from that end's point.
struct ParkSegment {
  EntityKey Key = NULL_KEY;
  PathKind Kind = PathKind::Guest;
  EntityKey From = NULL_KEY;
  EntityKey To = NULL_KEY;
  double Width = 0.0;
  std::vector<CarrierPoint> Line;
  double FromEdge = DEFAULT_EDGE_RADIUS;
  double ToEdge = DEFAULT_EDGE_RADIUS;

  bool operator==(const ParkSegment &) const = default;
};

// A box's intent, as other modules read it.
struct ParkBox {
  EntityKey Key = NULL_KEY;
  BoxKind Kind = BoxKind::Shop;
  Pose At;

  bool operator==(const ParkBox &) const = default;
};

// Registers the private intent component types entrance, path-point, path-segment, and box, in
// that order.
void addParkIntent(WorldSchema &schema);

// Every entity holding that kind of intent, in ascending key order.
std::vector<ParkEntrance> parkEntrances(const World &world);
std::vector<ParkPathPoint> parkPathPoints(const World &world);
std::vector<ParkSegment> parkSegments(const World &world);
// Every corner, in ascending key order of its point.
std::vector<ParkCorner> parkCorners(const World &world);
std::vector<ParkBox> parkBoxes(const World &world);

// The new-park template with the schema and the seed, resolution pending: an entrance on the
// park's edge facing in, and a guest path of one segment running into the park from in front of
// it.
World makeNewPark(std::shared_ptr<const WorldSchema> schema, uint64_t seed);

} // namespace tpj

#endif
