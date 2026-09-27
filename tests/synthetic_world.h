#ifndef TPJ_TESTS_SYNTHETIC_WORLD_H
#define TPJ_TESTS_SYNTHETIC_WORLD_H

#include "sim/entity_key.h"
#include "sim/mix.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <entt/entity/registry.hpp>

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <random>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

// Synthetic component types for the walk's tests. They are declared here, in the tests, and reach
// the walk only through the public schema, because nothing reads another module's internals.
// Between them they use every field type a visitFields may list: bool, every integer type,
// double, EntityKey, enums, vectors of those, and nested structs.
namespace tpj::synthetic {

enum class Mood : uint8_t { Calm, Happy, Cross };
enum class Grade : int16_t { Low = -5, Mid = 0, High = 7 };

constexpr std::array<Mood, 3> MOODS{Mood::Calm, Mood::Happy, Mood::Cross};
constexpr std::array<Grade, 3> GRADES{Grade::Low, Grade::Mid, Grade::High};

struct Point {
  double X = 0.0;
  double Y = 0.0;
  double Z = 0.0;
};

template <typename Visitor> void visitFields(Visitor &visitor, Point &point) {
  visitor.field("x", point.X);
  visitor.field("y", point.Y);
  visitor.field("z", point.Z);
}

// Scalars: bool, every standard integer type, and a double.
struct Numbers {
  bool Flag = false;
  char Letter = 0;
  signed char TinySigned = 0;
  unsigned char TinyUnsigned = 0;
  short ShortSigned = 0;
  unsigned short ShortUnsigned = 0;
  int IntSigned = 0;
  unsigned int IntUnsigned = 0;
  long LongSigned = 0;
  unsigned long LongUnsigned = 0;
  long long LongLongSigned = 0;
  unsigned long long LongLongUnsigned = 0;
  double Real = 0.0;
};

template <typename Visitor> void visitFields(Visitor &visitor, Numbers &numbers) {
  visitor.field("flag", numbers.Flag);
  visitor.field("letter", numbers.Letter);
  visitor.field("tiny-signed", numbers.TinySigned);
  visitor.field("tiny-unsigned", numbers.TinyUnsigned);
  visitor.field("short-signed", numbers.ShortSigned);
  visitor.field("short-unsigned", numbers.ShortUnsigned);
  visitor.field("int-signed", numbers.IntSigned);
  visitor.field("int-unsigned", numbers.IntUnsigned);
  visitor.field("long-signed", numbers.LongSigned);
  visitor.field("long-unsigned", numbers.LongUnsigned);
  visitor.field("long-long-signed", numbers.LongLongSigned);
  visitor.field("long-long-unsigned", numbers.LongLongUnsigned);
  visitor.field("real", numbers.Real);
}

// A reference to another entity, enums, and a nested struct.
struct Link {
  EntityKey Target = NULL_KEY;
  Mood Feeling = Mood::Calm;
  Grade Level = Grade::Mid;
  Point Anchor;
};

template <typename Visitor> void visitFields(Visitor &visitor, Link &link) {
  visitor.field("target", link.Target);
  visitor.field("feeling", link.Feeling);
  visitor.field("level", link.Level);
  visitor.field("anchor", link.Anchor);
}

// Vectors of every allowed element kind.
struct Series {
  std::vector<int32_t> Counts;
  std::vector<uint8_t> Bytes;
  std::vector<double> Samples;
  std::vector<EntityKey> Targets;
  std::vector<Mood> Moods;
  std::vector<Point> Path;
};

template <typename Visitor> void visitFields(Visitor &visitor, Series &series) {
  visitor.field("counts", series.Counts);
  visitor.field("bytes", series.Bytes);
  visitor.field("samples", series.Samples);
  visitor.field("targets", series.Targets);
  visitor.field("moods", series.Moods);
  visitor.field("path", series.Path);
}

// Structs nested three deep, with vectors of structs inside.
struct Inner {
  int64_t Id = 0;
  Point At;
  std::vector<double> Weights;
};

template <typename Visitor> void visitFields(Visitor &visitor, Inner &inner) {
  visitor.field("id", inner.Id);
  visitor.field("at", inner.At);
  visitor.field("weights", inner.Weights);
}

struct Middle {
  Inner Head;
  std::vector<Inner> Rest;
  Grade Level = Grade::Mid;
};

template <typename Visitor> void visitFields(Visitor &visitor, Middle &middle) {
  visitor.field("head", middle.Head);
  visitor.field("rest", middle.Rest);
  visitor.field("level", middle.Level);
}

struct Deep {
  Middle Body;
  uint16_t Depth = 0;
};

template <typename Visitor> void visitFields(Visitor &visitor, Deep &deep) {
  visitor.field("body", deep.Body);
  visitor.field("depth", deep.Depth);
}

// A tag: no members and no visitFields.
struct Marker {};

// Calls fn with std::type_identity<T> for each synthetic component type, in registration order.
template <typename Fn> void forEachSyntheticType(Fn &&fn) {
  fn(std::type_identity<Numbers>{});
  fn(std::type_identity<Link>{});
  fn(std::type_identity<Series>{});
  fn(std::type_identity<Deep>{});
  fn(std::type_identity<Marker>{});
}

constexpr std::array<std::string_view, 5> SYNTHETIC_NAMES{"numbers", "link", "series", "deep",
                                                          "marker"};

inline std::shared_ptr<const WorldSchema> makeSyntheticSchema() {
  auto schema = std::make_shared<WorldSchema>();
  schema->addComponent<Numbers>("numbers", DataKind::State);
  schema->addComponent<Link>("link", DataKind::Intent);
  schema->addComponent<Series>("series", DataKind::Derived);
  schema->addComponent<Deep>("deep", DataKind::State);
  schema->addComponent<Marker>("marker", DataKind::Intent);
  return schema;
}

template <typename T> struct IsStdVector : std::false_type {};
template <typename T, typename Allocator>
struct IsStdVector<std::vector<T, Allocator>> : std::true_type {};

// Records every value in a component as 64-bit words, independently of the walk, so tests can
// compare components value by value. Vector lengths are recorded too.
class FieldRecorder {
public:
  template <typename Field> void field(std::string_view /*name*/, Field &value) { record(value); }

  std::vector<uint64_t> Words;

private:
  template <typename Field> void record(Field &value) {
    if constexpr (std::is_same_v<Field, double>) {
      Words.push_back(std::bit_cast<uint64_t>(value));
    } else if constexpr (std::is_same_v<Field, bool>) {
      Words.push_back(value ? 1U : 0U);
    } else if constexpr (std::is_enum_v<Field>) {
      Words.push_back(static_cast<uint64_t>(static_cast<std::underlying_type_t<Field>>(value)));
    } else if constexpr (std::is_integral_v<Field>) {
      Words.push_back(static_cast<uint64_t>(value));
    } else if constexpr (IsStdVector<Field>::value) {
      Words.push_back(value.size());
      for (auto &element : value) {
        record(element);
      }
    } else {
      visitFields(*this, value);
    }
  }
};

template <typename T> std::vector<uint64_t> recordComponent(const T &component) {
  FieldRecorder recorder;
  if constexpr (!std::is_empty_v<T>) {
    T copy = component;
    visitFields(recorder, copy);
  }
  return recorder.Words;
}

// Fills a component with seeded random values. EntityKey fields take live keys or NULL_KEY.
// Doubles are never NaN, and are sometimes 0.0 or -0.0.
class FieldRandomizer {
public:
  FieldRandomizer(std::mt19937_64 &rng, std::vector<EntityKey> keys)
      : Rng(rng), Keys(std::move(keys)) {}

  template <typename Field> void field(std::string_view /*name*/, Field &value) { fill(value); }

private:
  template <typename Field> void fill(Field &value) {
    if constexpr (std::is_same_v<Field, bool>) {
      value = (Rng() & 1U) != 0;
    } else if constexpr (std::is_same_v<Field, double>) {
      value = randomDouble();
    } else if constexpr (std::is_same_v<Field, EntityKey>) {
      value = (Keys.empty() || Rng() % 5 == 0) ? NULL_KEY : Keys[Rng() % Keys.size()];
    } else if constexpr (std::is_same_v<Field, Mood>) {
      value = MOODS[Rng() % MOODS.size()];
    } else if constexpr (std::is_same_v<Field, Grade>) {
      value = GRADES[Rng() % GRADES.size()];
    } else if constexpr (std::is_integral_v<Field>) {
      value = static_cast<Field>(Rng());
    } else if constexpr (IsStdVector<Field>::value) {
      value.resize(Rng() % 4);
      for (auto &element : value) {
        fill(element);
      }
    } else {
      visitFields(*this, value);
    }
  }

  double randomDouble() {
    switch (Rng() % 6) {
    case 0:
      return 0.0;
    case 1:
      return -0.0;
    case 2:
      return 1.5;
    default:
      return std::uniform_real_distribution<double>(-1000.0, 1000.0)(Rng);
    }
  }

  std::mt19937_64 &Rng;
  std::vector<EntityKey> Keys;
};

inline Mood nextEnumValue(Mood mood) {
  return MOODS[(static_cast<size_t>(mood) + 1) % MOODS.size()];
}

inline Grade nextEnumValue(Grade grade) {
  switch (grade) {
  case Grade::Low:
    return Grade::Mid;
  case Grade::Mid:
    return Grade::High;
  case Grade::High:
    return Grade::Low;
  }
  return Grade::Low;
}

// Makes exactly one single-value change to a component: the one at site Target, counting sites in
// visitFields order. Every bool, integer, EntityKey, and enum is one site (flip, add 1, next key,
// next enumerator). Every double is one site (the next representable value), plus a second site
// when it is zero (0.0 to -0.0 or back). Every element of a vector contributes its own sites, and
// every vector two more (grow by one, and shrink by one when not empty).
class FieldMutator {
public:
  explicit FieldMutator(size_t target) : Target(target) {}

  template <typename Field> void field(std::string_view /*name*/, Field &value) { mutate(value); }

  [[nodiscard]] size_t sites() const { return Count; }
  [[nodiscard]] bool mutated() const { return Count > Target; }
  [[nodiscard]] bool mutatedZeroSign() const { return ZeroSign; }

private:
  bool hit() { return Count++ == Target; }

  template <typename Field> void mutate(Field &value) {
    if constexpr (std::is_same_v<Field, bool>) {
      if (hit()) {
        value = !value;
      }
    } else if constexpr (std::is_same_v<Field, double>) {
      const bool zero = value == 0.0;
      if (hit()) {
        value = std::nextafter(value, std::numeric_limits<double>::infinity());
      }
      if (zero && hit()) {
        value = -value;
        ZeroSign = true;
      }
    } else if constexpr (std::is_same_v<Field, EntityKey>) {
      if (hit()) {
        value = EntityKey{static_cast<uint64_t>(value) + 1};
      }
    } else if constexpr (std::is_enum_v<Field>) {
      if (hit()) {
        value = nextEnumValue(value);
      }
    } else if constexpr (std::is_integral_v<Field>) {
      if (hit()) {
        value = value == std::numeric_limits<Field>::max() ? static_cast<Field>(value - 1)
                                                           : static_cast<Field>(value + 1);
      }
    } else if constexpr (IsStdVector<Field>::value) {
      for (auto &element : value) {
        mutate(element);
      }
      if (hit()) {
        value.emplace_back();
      }
      if (!value.empty() && hit()) {
        value.pop_back();
      }
    } else {
      visitFields(*this, value);
    }
  }

  size_t Target;
  size_t Count = 0;
  bool ZeroSign = false;
};

// The entity for a live key. Throws, failing the calling test, instead of letting EnTT abort on
// entt::null when the key is not live.
inline entt::entity requireEntity(const World &world, EntityKey key) {
  const entt::entity entity = world.findEntity(key);
  if (entity == entt::null || !world.Registry.valid(entity)) {
    throw std::logic_error("no live entity for key " + std::to_string(static_cast<uint64_t>(key)));
  }
  return entity;
}

// A seeded random world over the synthetic schema: a few counter keys, sometimes one destroyed,
// one or two derived keys, a random tick and seed, and a random subset of components per entity.
inline World buildRandomWorld(const std::shared_ptr<const WorldSchema> &schema, uint64_t seed) {
  std::mt19937_64 rng(seed);
  World world(schema, rng());
  world.Tick = rng() % 10000;
  const size_t count = 3 + (rng() % 5);
  std::vector<EntityKey> created;
  for (size_t i = 0; i < count; ++i) {
    created.push_back(world.createEntity());
  }
  if ((rng() & 1U) != 0) {
    world.destroyEntity(created[1 + (rng() % (count - 1))]);
  }
  world.createDerivedEntity(created[0], hashName("synthetic"), rng() % 4);
  if ((rng() & 1U) != 0) {
    world.createDerivedEntity(created[0], hashName("synthetic-extra"), 0);
  }
  const std::vector<EntityKey> live = world.keys();
  FieldRandomizer randomizer(rng, live);
  for (const EntityKey key : live) {
    const entt::entity entity = requireEntity(world, key);
    forEachSyntheticType([&](auto type) {
      using T = typename decltype(type)::type;
      if (rng() % 3 == 0) {
        return;
      }
      if constexpr (std::is_empty_v<T>) {
        world.Registry.emplace<T>(entity);
      } else {
        T value{};
        visitFields(randomizer, value);
        world.Registry.emplace<T>(entity, std::move(value));
      }
    });
  }
  return world;
}

// One component on one key, recorded value by value.
struct ComponentRecord {
  EntityKey Key = NULL_KEY;
  size_t TypeIndex = 0;
  std::vector<uint64_t> Words;
  bool operator==(const ComponentRecord &) const = default;
};

// Everything the walk covers, observed through the public interface and FieldRecorder only.
struct WorldSnapshot {
  uint64_t Tick = 0;
  uint64_t Seed = 0;
  uint64_t NextKey = 0;
  std::vector<EntityKey> Keys;
  std::vector<ComponentRecord> Components;
  bool operator==(const WorldSnapshot &) const = default;
};

inline WorldSnapshot snapshotWorld(const World &world) {
  WorldSnapshot snapshot{world.Tick, world.Seed, world.nextKey(), world.keys(), {}};
  for (const EntityKey key : snapshot.Keys) {
    const entt::entity entity = requireEntity(world, key);
    size_t typeIndex = 0;
    forEachSyntheticType([&](auto type) {
      using T = typename decltype(type)::type;
      if (world.Registry.all_of<T>(entity)) {
        if constexpr (std::is_empty_v<T>) {
          snapshot.Components.push_back({key, typeIndex, {}});
        } else {
          snapshot.Components.push_back(
              {key, typeIndex, recordComponent(world.Registry.get<T>(entity))});
        }
      }
      ++typeIndex;
    });
  }
  return snapshot;
}

} // namespace tpj::synthetic

#endif
