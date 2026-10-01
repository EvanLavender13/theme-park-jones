#include "sim/park/intent.h"

#include "sim/park/internal/components.h"

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

} // namespace

void addParkIntent(WorldSchema &schema) {
  schema.addComponent<EntranceIntent>("entrance", DataKind::Intent);
  schema.addComponent<PathIntent>("path", DataKind::Intent);
  schema.addComponent<BoxIntent>("box", DataKind::Intent);
}

std::vector<ParkEntrance> parkEntrances(const World &world) {
  return collectIntent<EntranceIntent>(world, [](EntityKey key, const EntranceIntent &entrance) {
    return ParkEntrance{key, entrance.At};
  });
}

std::vector<ParkPath> parkPaths(const World &world) {
  return collectIntent<PathIntent>(world, [](EntityKey key, const PathIntent &path) {
    return ParkPath{key, path.Kind, path.Points};
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
  // A guest path from 2 m in front of the entrance, 20 m into the park.
  const EntityKey path = world.createEntity();
  world.Registry.emplace<PathIntent>(world.findEntity(path),
                                     PathIntent{PathKind::Guest, {{0.0, 123.0}, {0.0, 103.0}}});
  return world;
}

} // namespace tpj
