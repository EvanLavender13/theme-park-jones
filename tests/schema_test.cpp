#include "sim/schema.h"
#include "sim/world.h"
#include "synthetic_world.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <entt/core/type_info.hpp>

#include <cstddef>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <string_view>
#include <vector>

namespace tpj {
namespace {

struct Shop {
  int32_t Stock = 0;
};

template <typename Visitor> void visitFields(Visitor &visitor, Shop &shop) {
  visitor.field("stock", shop.Stock);
}

struct Box {
  double Size = 0.0;
};

template <typename Visitor> void visitFields(Visitor &visitor, Box &box) {
  visitor.field("size", box.Size);
}

struct Crate {
  uint8_t Load = 0;
};

template <typename Visitor> void visitFields(Visitor &visitor, Crate &crate) {
  visitor.field("load", crate.Load);
}

struct Beacon {};

struct Entry {
  std::string Name;
  DataKind Kind = DataKind::State;
  entt::id_type TypeId = 0;
  bool operator==(const Entry &) const = default;
};

std::vector<Entry> entriesOf(const WorldSchema &schema) {
  std::vector<Entry> entries;
  for (const ComponentType &type : schema.components()) {
    entries.push_back({type.Name, type.Kind, type.TypeId});
  }
  return entries;
}

template <typename T> entt::id_type typeIdOf() { return entt::type_id<T>().hash(); }

TEST_CASE("addComponent accepts names of lowercase letters, digits, and hyphens") {
  WorldSchema schema;
  CHECK_NOTHROW(schema.addComponent<Shop>("shop", DataKind::State));
  CHECK_NOTHROW(schema.addComponent<Box>("box-2", DataKind::Intent));
  CHECK_NOTHROW(
      schema.addComponent<Crate>("abcdefghijklmnopqrstuvwxyz-0123456789", DataKind::Derived));
  CHECK(schema.components().size() == 3);
}

TEST_CASE("addComponent rejects a malformed name and leaves the schema unchanged") {
  const std::string name =
      GENERATE(as<std::string>{}, "", "Shop", "a b", "a_b", "SHOP", "box.2", "a=b", "caf\xc3\xa9");
  CAPTURE(name);
  WorldSchema schema;
  schema.addComponent<Shop>("shop", DataKind::State);
  const std::vector<Entry> before = entriesOf(schema);

  CHECK_THROWS_AS(schema.addComponent<Box>(name, DataKind::State), std::invalid_argument);
  CHECK(entriesOf(schema) == before);
  CHECK(schema.findComponent(typeIdOf<Box>()) == nullptr);
}

TEST_CASE("addComponent rejects a name already registered and leaves the schema unchanged") {
  WorldSchema schema;
  schema.addComponent<Shop>("shop", DataKind::State);
  schema.addComponent<Box>("box", DataKind::Intent);
  const std::vector<Entry> before = entriesOf(schema);

  CHECK_THROWS_AS(schema.addComponent<Crate>("shop", DataKind::State), std::invalid_argument);
  CHECK_THROWS_AS(schema.addComponent<Crate>("box", DataKind::Intent), std::invalid_argument);
  CHECK(entriesOf(schema) == before);
  CHECK(schema.findComponent(typeIdOf<Crate>()) == nullptr);
}

TEST_CASE("addComponent rejects a type already registered under another name") {
  WorldSchema schema;
  schema.addComponent<Shop>("shop", DataKind::State);
  schema.addComponent<Beacon>("beacon", DataKind::State);
  const std::vector<Entry> before = entriesOf(schema);

  CHECK_THROWS_AS(schema.addComponent<Shop>("store", DataKind::State), std::invalid_argument);
  CHECK_THROWS_AS(schema.addComponent<Shop>("store", DataKind::Derived), std::invalid_argument);
  CHECK_THROWS_AS(schema.addComponent<Beacon>("light", DataKind::State), std::invalid_argument);
  CHECK(entriesOf(schema) == before);
  const ComponentType *shop = schema.findComponent(typeIdOf<Shop>());
  REQUIRE(shop != nullptr);
  CHECK(shop->Name == "shop");
}

TEST_CASE("a schema keeps working after a rejected registration") {
  WorldSchema schema;
  schema.addComponent<Shop>("shop", DataKind::State);
  CHECK_THROWS_AS(schema.addComponent<Box>("Box", DataKind::State), std::invalid_argument);
  CHECK_NOTHROW(schema.addComponent<Box>("box", DataKind::State));
  const std::vector<Entry> expected{{"shop", DataKind::State, typeIdOf<Shop>()},
                                    {"box", DataKind::State, typeIdOf<Box>()}};
  CHECK(entriesOf(schema) == expected);
}

TEST_CASE("components lists registered types in registration order with names and kinds") {
  WorldSchema schema;
  schema.addComponent<Crate>("crate", DataKind::Derived);
  schema.addComponent<Shop>("shop", DataKind::Intent);
  schema.addComponent<Beacon>("beacon", DataKind::State);
  schema.addComponent<Box>("box", DataKind::State);

  const std::vector<Entry> expected{{"crate", DataKind::Derived, typeIdOf<Crate>()},
                                    {"shop", DataKind::Intent, typeIdOf<Shop>()},
                                    {"beacon", DataKind::State, typeIdOf<Beacon>()},
                                    {"box", DataKind::State, typeIdOf<Box>()}};
  CHECK(entriesOf(schema) == expected);
}

TEST_CASE("findComponent returns the entry for a registered type and nullptr otherwise") {
  WorldSchema schema;
  schema.addComponent<Shop>("shop", DataKind::Intent);
  schema.addComponent<Beacon>("beacon", DataKind::Derived);

  const ComponentType *shop = schema.findComponent(typeIdOf<Shop>());
  REQUIRE(shop != nullptr);
  CHECK(shop->Name == "shop");
  CHECK(shop->Kind == DataKind::Intent);
  CHECK(shop->TypeId == typeIdOf<Shop>());

  const ComponentType *beacon = schema.findComponent(typeIdOf<Beacon>());
  REQUIRE(beacon != nullptr);
  CHECK(beacon->Name == "beacon");
  CHECK(beacon->Kind == DataKind::Derived);

  CHECK(schema.findComponent(typeIdOf<Box>()) == nullptr);
  CHECK(WorldSchema().findComponent(typeIdOf<Shop>()) == nullptr);
}

// This file compiling shows that bool, every integer type, double, EntityKey, enums, vectors of
// these, and nested structs are allowed field types.
TEST_CASE("component types using every allowed field type register") {
  const auto schema = synthetic::makeSyntheticSchema();
  REQUIRE(schema->components().size() == synthetic::SYNTHETIC_NAMES.size());
  for (size_t i = 0; i < synthetic::SYNTHETIC_NAMES.size(); ++i) {
    CHECK(schema->components()[i].Name == synthetic::SYNTHETIC_NAMES[i]);
  }
  CHECK(schema->findComponent(typeIdOf<synthetic::Numbers>()) != nullptr);
  CHECK(schema->findComponent(typeIdOf<synthetic::Link>()) != nullptr);
  CHECK(schema->findComponent(typeIdOf<synthetic::Series>()) != nullptr);
  CHECK(schema->findComponent(typeIdOf<synthetic::Deep>()) != nullptr);
}

TEST_CASE("a tag component with no members registers without a visitFields") {
  WorldSchema schema;
  CHECK_NOTHROW(schema.addComponent<Beacon>("beacon", DataKind::State));
  const ComponentType *beacon = schema.findComponent(typeIdOf<Beacon>());
  REQUIRE(beacon != nullptr);
  CHECK(beacon->Name == "beacon");
}

} // namespace
} // namespace tpj
