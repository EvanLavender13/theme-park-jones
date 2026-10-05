#include "sim/park/intent.h"

#include "sim/park/geometry.h"
#include "sim/park/internal/components.h"

#include <cmath>

#include <utility>
#include <vector>

namespace tpj {

namespace {

// Every entity holding the component T, made public by toValue, in ascending key order.
template <typename T, typename ToValue> auto collectIntent(const World &world, ToValue toValue) {
  std::vector<decltype(toValue(EntityKey{}, std::declval<const T &>()))> result;
  for (const EntityKey key : world.keys()) {
    if (const T *intent = world.Registry.try_get<T>(world.findEntity(key)); intent != nullptr) {
      result.push_back(toValue(key, *intent));
    }
  }
  return result;
}

// The path point the key holds, or null.
const PathPointIntent *findPoint(const World &world, EntityKey key) {
  const entt::entity entity = world.findEntity(key);
  return entity == entt::null ? nullptr : world.Registry.try_get<PathPointIntent>(entity);
}

// The segment's ground line: empty unless its ends are two different path points and its width is
// finite and above zero.
std::vector<CarrierPoint> lineOf(const World &world, const PathSegmentIntent &segment) {
  const PathPointIntent *from = findPoint(world, segment.From);
  const PathPointIntent *to = findPoint(world, segment.To);
  if (from == nullptr || to == nullptr || segment.From == segment.To ||
      !std::isfinite(segment.Width) || !(segment.Width > 0.0)) {
    return {};
  }
  return segmentLine(from->At, to->At);
}

} // namespace

void addParkIntent(WorldSchema &schema) {
  schema.addComponent<EntranceIntent>("entrance", DataKind::Intent);
  schema.addComponent<PathPointIntent>("path-point", DataKind::Intent);
  schema.addComponent<PathSegmentIntent>("path-segment", DataKind::Intent);
  schema.addComponent<BoxIntent>("box", DataKind::Intent);
}

std::vector<ParkEntrance> parkEntrances(const World &world) {
  return collectIntent<EntranceIntent>(world, [](EntityKey key, const EntranceIntent &entrance) {
    return ParkEntrance{key, entrance.At};
  });
}

std::vector<ParkPathPoint> parkPathPoints(const World &world) {
  return collectIntent<PathPointIntent>(world, [](EntityKey key, const PathPointIntent &point) {
    return ParkPathPoint{key, point.At, point.Radius};
  });
}

std::vector<ParkSegment> parkSegments(const World &world) {
  return collectIntent<PathSegmentIntent>(
      world, [&world](EntityKey key, const PathSegmentIntent &segment) {
        return ParkSegment{key,        segment.Kind,  segment.From,
                           segment.To, segment.Width, lineOf(world, segment)};
      });
}

std::vector<ParkBox> parkBoxes(const World &world) {
  return collectIntent<BoxIntent>(
      world, [](EntityKey key, const BoxIntent &box) { return ParkBox{key, box.Kind, box.At}; });
}

World makeNewPark(std::shared_ptr<const WorldSchema> schema, uint64_t seed) {
  World world(std::move(schema), seed);
  // The entrance's back lies on the edge at z = 128, and it faces into the park.
  const EntityKey entrance = world.createEntity();
  world.Registry.emplace<EntranceIntent>(world.findEntity(entrance),
                                         EntranceIntent{Pose{0.0, 126.5, 0.0, -1.0}});
  // A guest path of one segment from 2 m in front of the entrance, 20 m into the park.
  const EntityKey near = world.createEntity();
  world.Registry.emplace<PathPointIntent>(world.findEntity(near), PathPointIntent{{0.0, 123.0}});
  const EntityKey far = world.createEntity();
  world.Registry.emplace<PathPointIntent>(world.findEntity(far), PathPointIntent{{0.0, 103.0}});
  const EntityKey segment = world.createEntity();
  world.Registry.emplace<PathSegmentIntent>(
      world.findEntity(segment),
      PathSegmentIntent{PathKind::Guest, near, far, pathWidth(PathKind::Guest)});
  return world;
}

} // namespace tpj
