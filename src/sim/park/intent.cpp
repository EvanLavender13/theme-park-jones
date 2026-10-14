#include "sim/park/intent.h"

#include "sim/clock.h"
#include "sim/park/internal/components.h"
#include "sim/park/internal/ground_units.h"
#include "sim/park/internal/layout.h"

#include <optional>
#include <span>
#include <stddef.h>
#include <utility>
#include <vector>

namespace tpj {

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
  std::vector<ParkPathPoint> points;
  parkPathPoints(world, points);
  return points;
}

void parkPathPoints(const World &world, std::vector<ParkPathPoint> &points) {
  points.clear();
  world.forEachWith<PathPointIntent>([&points](EntityKey key, const PathPointIntent &point) {
    points.push_back(ParkPathPoint{key, point.At, point.Radius, point.Taper});
  });
}

std::vector<ParkSegment> parkSegments(const World &world) {
  GroundLists lists;
  worldGround(world, lists);
  std::vector<ParkSegment> segments = std::move(lists.Ground.Segments);
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

void parkSegments(const World &world, std::vector<ParkSegment> &segments, TickBuffers &buffers) {
  GroundLists &lists = buffers.get<GroundLists>();
  worldGround(world, lists);
  const std::vector<ParkSegment> &ground = lists.Ground.Segments;
  resizeKeeping(segments, ground.size(), lists.CallerSpare);
  for (size_t i = 0; i < ground.size(); ++i) {
    const ParkSegment &from = ground[i];
    ParkSegment &segment = segments[i];
    segment.Key = from.Key;
    segment.Kind = from.Kind;
    segment.From = from.From;
    segment.To = from.To;
    segment.Width = from.Width;
    copyGrowing(segment.Line, std::span<const CarrierPoint>(from.Line));
    segment.FromEdge = from.FromEdge;
    segment.ToEdge = from.ToEdge;
    segment.Furnished = {};
    if (const auto *left = world.tryGet<LeftFurnishingIntent>(segment.Key)) {
      segment.Furnished.Left = left->Held;
    }
    if (const auto *right = world.tryGet<RightFurnishingIntent>(segment.Key)) {
      segment.Furnished.Right = right->Held;
    }
  }
}

std::vector<ParkCorner> parkCorners(const World &world) {
  GroundLists lists;
  worldGround(world, lists);
  return std::move(lists.Ground.Corners);
}

void parkCorners(const World &world, std::vector<ParkCorner> &corners, TickBuffers &buffers) {
  GroundLists &lists = buffers.get<GroundLists>();
  worldGround(world, lists);
  copyGrowing(corners, std::span<const ParkCorner>(lists.Ground.Corners));
}

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
