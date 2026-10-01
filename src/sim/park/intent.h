#ifndef TPJ_SIM_PARK_INTENT_H
#define TPJ_SIM_PARK_INTENT_H

#include "sim/entity_key.h"
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

constexpr double pathWidth(PathKind kind) { return kind == PathKind::Guest ? 3.0 : 2.0; }

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

// A path's intent, as other modules read it: its kind and the points the player clicked, in order.
struct ParkPath {
  EntityKey Key = NULL_KEY;
  PathKind Kind = PathKind::Guest;
  std::vector<ParkPoint> Points;

  bool operator==(const ParkPath &) const = default;
};

// A box's intent, as other modules read it.
struct ParkBox {
  EntityKey Key = NULL_KEY;
  BoxKind Kind = BoxKind::Shop;
  Pose At;

  bool operator==(const ParkBox &) const = default;
};

// Registers the private intent component types entrance, path, and box, in that order.
void addParkIntent(WorldSchema &schema);

// Every entity holding that kind of intent, in ascending key order.
std::vector<ParkEntrance> parkEntrances(const World &world);
std::vector<ParkPath> parkPaths(const World &world);
std::vector<ParkBox> parkBoxes(const World &world);

// The new-park template with the schema and the seed, resolution pending: an entrance on the
// park's edge facing in, and a guest path running into the park from in front of it.
World makeNewPark(std::shared_ptr<const WorldSchema> schema, uint64_t seed);

} // namespace tpj

#endif
