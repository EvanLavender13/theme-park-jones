#ifndef TPJ_TESTS_SYNTHETIC_TYPES_H
#define TPJ_TESTS_SYNTHETIC_TYPES_H

#include "sim/entity_key.h"
#include "sim/mix.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <memory>
#include <stdint.h>
#include <utility>
#include <vector>

// Synthetic component types, owned by the tests and registered through the public schema, so the
// walk reaches them only through their registered functions.
namespace tpj::test {

enum class Mood : uint8_t { Calm, Excited };

struct Offset {
  double X = 0.0;
  double Z = 0.0;
};

template <typename Visitor> void visitFields(Visitor &visitor, Offset &offset) {
  visitor.field("x", offset.X);
  visitor.field("z", offset.Z);
}

// One field of every kind visitFields may list.
struct Probe {
  bool Flag = false;
  int32_t Count = 0;
  uint16_t Small = 0;
  double Level = 0.0;
  Mood Feeling = Mood::Calm;
  EntityKey Target = NULL_KEY;
  std::vector<double> Samples;
  Offset Place;
};

template <typename Visitor> void visitFields(Visitor &visitor, Probe &probe) {
  visitor.field("flag", probe.Flag);
  visitor.field("count", probe.Count);
  visitor.field("small", probe.Small);
  visitor.field("level", probe.Level);
  visitor.field("feeling", probe.Feeling);
  visitor.field("target", probe.Target);
  visitor.field("samples", probe.Samples);
  visitor.field("place", probe.Place);
}

struct Tag {};

struct Cached {
  int64_t Total = 0;
};

template <typename Visitor> void visitFields(Visitor &visitor, Cached &cached) {
  visitor.field("total", cached.Total);
}

inline constexpr uint64_t SLOT_PURPOSE = hashName("slot");

// Registers probe (state), tag (intent), and cached (derived), in that order.
inline std::shared_ptr<const WorldSchema> makeSchema() {
  auto schema = std::make_shared<WorldSchema>();
  schema->addComponent<Probe>("probe", DataKind::State);
  schema->addComponent<Tag>("tag", DataKind::Intent);
  schema->addComponent<Cached>("cached", DataKind::Derived);
  return schema;
}

template <typename T> T &componentOf(World &world, EntityKey key) {
  return world.Registry.get<T>(world.findEntity(key));
}

// Counter keys 1 and 2 hold a probe and a tag each, key 3 holds nothing, and a key derived from 1
// holds a cached value.
inline World buildWorld(std::shared_ptr<const WorldSchema> schema) {
  World world(std::move(schema), 12345);
  world.Tick = 3;
  const EntityKey first = world.createEntity();
  const EntityKey second = world.createEntity();
  world.createEntity();
  const EntityKey derived = world.createDerivedEntity(first, SLOT_PURPOSE, 0);

  Probe firstProbe;
  firstProbe.Flag = true;
  firstProbe.Count = -7;
  firstProbe.Small = 9;
  firstProbe.Level = 1.5;
  firstProbe.Feeling = Mood::Excited;
  firstProbe.Target = second;
  firstProbe.Samples = {0.25, -3.0};
  firstProbe.Place = {.X = 4.5, .Z = -2.0};
  world.Registry.emplace<Probe>(world.findEntity(first), firstProbe);
  world.Registry.emplace<Probe>(world.findEntity(second));
  world.Registry.emplace<Tag>(world.findEntity(first));
  world.Registry.emplace<Tag>(world.findEntity(second));
  world.Registry.emplace<Cached>(world.findEntity(derived), Cached{.Total = 42});
  return world;
}

} // namespace tpj::test

#endif
