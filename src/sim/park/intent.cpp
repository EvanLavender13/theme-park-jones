#include "sim/park/intent.h"

#include "sim/clock.h"
#include "sim/park/internal/components.h"
#include "sim/park/internal/layout.h"

#include <optional>
#include <utility>
#include <vector>

namespace tpj {

namespace {

// Every entity holding the component T, made public by toValue, in ascending key order.
template <typename T, typename ToValue> auto collectIntent(const World &world, ToValue toValue) {
  std::vector<decltype(toValue(EntityKey{}, std::declval<const T &>()))> result;
  for (const EntityKey key : world.keysWith<T>()) {
    result.push_back(toValue(key, world.get<T>(key)));
  }
  return result;
}

} // namespace

void addParkIntent(WorldSchema &schema) {
  schema.addComponent<EntranceIntent>("entrance", DataKind::Intent);
  schema.addComponent<PathPointIntent>("path-point", DataKind::Intent);
  schema.addComponent<PathSegmentIntent>("path-segment", DataKind::Intent);
  schema.addComponent<BoxIntent>("box", DataKind::Intent);
  schema.addComponent<ShopStockIntent>("shop-stock", DataKind::Intent);
  schema.addComponent<OpeningHoursIntent>("opening-hours", DataKind::Intent);
  schema.addComponent<LeftFurnishingIntent>("left-furnishing", DataKind::Intent);
  schema.addComponent<RightFurnishingIntent>("right-furnishing", DataKind::Intent);
}

std::vector<ParkEntrance> parkEntrances(const World &world) {
  std::vector<ParkEntrance> entrances;
  parkEntrances(world, entrances);
  return entrances;
}

void parkEntrances(const World &world, std::vector<ParkEntrance> &entrances) {
  entrances.clear();
  world.forEachWith<EntranceIntent>([&entrances](EntityKey key, const EntranceIntent &entrance) {
    entrances.push_back(ParkEntrance{key, entrance.At});
  });
}

std::vector<ParkPathPoint> parkPathPoints(const World &world) {
  return collectIntent<PathPointIntent>(world, [](EntityKey key, const PathPointIntent &point) {
    return ParkPathPoint{key, point.At, point.Radius, point.Taper};
  });
}

std::vector<ParkSegment> parkSegments(const World &world) {
  std::vector<ParkSegment> segments = segmentsOf(layoutOf(world));
  for (ParkSegment &segment : segments) {
    if (const auto *left = world.tryGet<LeftFurnishingIntent>(segment.Key)) {
      segment.Furnished.Left = left->Held;
    }
    if (const auto *right = world.tryGet<RightFurnishingIntent>(segment.Key)) {
      segment.Furnished.Right = right->Held;
    }
  }
  return segments;
}

std::vector<ParkCorner> parkCorners(const World &world) { return cornersOf(layoutOf(world)); }

std::vector<ParkBox> parkBoxes(const World &world) {
  std::vector<ParkBox> boxes;
  parkBoxes(world, boxes);
  return boxes;
}

void parkBoxes(const World &world, std::vector<ParkBox> &boxes) {
  boxes.clear();
  world.forEachWith<BoxIntent>([&world, &boxes](EntityKey key, const BoxIntent &box) {
    const ShopStockIntent *stock = world.tryGet<ShopStockIntent>(key);
    boxes.push_back(
        ParkBox{key, box.Kind, box.At, stock != nullptr ? stock->Stock : ShopStock::Food});
  });
}

std::optional<ParkBox> parkBox(const World &world, EntityKey key) {
  const BoxIntent *box = world.tryGet<BoxIntent>(key);
  if (box == nullptr) {
    return std::nullopt;
  }
  const ShopStockIntent *stock = world.tryGet<ShopStockIntent>(key);
  return ParkBox{key, box->Kind, box->At, stock != nullptr ? stock->Stock : ShopStock::Food};
}

void parkSegmentKeys(const World &world, PathKind kind, std::vector<EntityKey> &keys) {
  keys.clear();
  world.forEachWith<PathSegmentIntent>(
      [kind, &keys](EntityKey key, const PathSegmentIntent &segment) {
        if (segment.Kind == kind) {
          keys.push_back(key);
        }
      });
}

OpeningHours parkHours(const World &world) {
  std::optional<OpeningHours> first;
  world.forEachWith<OpeningHoursIntent>(
      [&first](EntityKey /*key*/, const OpeningHoursIntent &hours) {
        if (!first) {
          first = hours.Hours;
        }
      });
  return first.value_or(DEFAULT_HOURS);
}

bool isOpenAt(const OpeningHours &hours, uint64_t second) {
  if (hours.Opening == hours.Closing) {
    return true;
  }
  return (second + DAY_SECONDS - hours.Opening) % DAY_SECONDS <
         (hours.Closing + DAY_SECONDS - hours.Opening) % DAY_SECONDS;
}

World makeNewPark(std::shared_ptr<const WorldSchema> schema, uint64_t seed) {
  World world(std::move(schema), seed);
  // The entrance's back lies on the edge at z = 128, and it faces into the park.
  const EntityKey entrance = world.createEntity();
  world.emplace<EntranceIntent>(entrance, EntranceIntent{Pose{0.0, 126.5, 0.0, -1.0}});
  // A guest path of one segment from 2 m in front of the entrance, 20 m into the park.
  const EntityKey near = world.createEntity();
  world.emplace<PathPointIntent>(near, PathPointIntent{{0.0, 123.0}});
  const EntityKey far = world.createEntity();
  world.emplace<PathPointIntent>(far, PathPointIntent{{0.0, 103.0}});
  const EntityKey segment = world.createEntity();
  world.emplace<PathSegmentIntent>(
      segment, PathSegmentIntent{PathKind::Guest, near, far, pathWidth(PathKind::Guest)});
  return world;
}

} // namespace tpj
