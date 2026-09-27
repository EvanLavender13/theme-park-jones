#include "synthetic_types.h"

#include "sim/schema.h"

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <string>
#include <vector>

namespace tpj {
namespace {

using test::Cached;
using test::Probe;
using test::Tag;

std::vector<std::string> namesOf(const WorldSchema &schema) {
  std::vector<std::string> names;
  for (const ComponentType &type : schema.components()) {
    names.push_back(type.Name);
  }
  return names;
}

TEST_CASE("a schema accepts names of lowercase letters, digits, and hyphens") {
  WorldSchema schema;
  REQUIRE_NOTHROW(schema.addComponent<Probe>("probe", DataKind::State));
  REQUIRE_NOTHROW(schema.addComponent<Tag>("tag-2", DataKind::Intent));
  REQUIRE_NOTHROW(schema.addComponent<Cached>("0-z-9", DataKind::Derived));
  REQUIRE(namesOf(schema) == std::vector<std::string>{"probe", "tag-2", "0-z-9"});
}

TEST_CASE("a schema refuses a malformed name and is left unchanged") {
  WorldSchema schema;
  schema.addComponent<Tag>("tag", DataKind::Intent);

  // Uppercase, an underscore, a space, and a letter outside ASCII.
  const std::vector<std::string> malformed = {"Probe", "my_probe", "my probe", "caf\xc3\xa9"};
  for (const std::string &name : malformed) {
    CAPTURE(name);
    REQUIRE_THROWS_AS(schema.addComponent<Probe>(name, DataKind::State), std::invalid_argument);
    REQUIRE(namesOf(schema) == std::vector<std::string>{"tag"});
  }
  // The refused type was not half registered.
  REQUIRE_NOTHROW(schema.addComponent<Probe>("probe", DataKind::State));
}

TEST_CASE("a schema refuses a repeated name and is left unchanged") {
  WorldSchema schema;
  schema.addComponent<Tag>("tag", DataKind::Intent);

  REQUIRE_THROWS_AS(schema.addComponent<Probe>("tag", DataKind::State), std::invalid_argument);
  REQUIRE(schema.components().size() == 1);
  REQUIRE(schema.components()[0].TypeId == entt::type_id<Tag>().hash());
  REQUIRE(schema.components()[0].Kind == DataKind::Intent);
  REQUIRE_NOTHROW(schema.addComponent<Probe>("probe", DataKind::State));
}

TEST_CASE("a schema refuses a type registered twice and is left unchanged") {
  WorldSchema schema;
  schema.addComponent<Tag>("tag", DataKind::Intent);

  REQUIRE_THROWS_AS(schema.addComponent<Tag>("marker", DataKind::Intent), std::invalid_argument);
  REQUIRE(namesOf(schema) == std::vector<std::string>{"tag"});
  // The refused name was not reserved either.
  REQUIRE_NOTHROW(schema.addComponent<Probe>("marker", DataKind::State));
}

TEST_CASE("components() keeps registration order with each type's name and kind") {
  // Neither alphabetical nor in DataKind order.
  WorldSchema schema;
  schema.addComponent<Tag>("tag", DataKind::Intent);
  schema.addComponent<Cached>("cached", DataKind::Derived);
  schema.addComponent<Probe>("probe", DataKind::State);

  const std::vector<ComponentType> &types = schema.components();
  REQUIRE(namesOf(schema) == std::vector<std::string>{"tag", "cached", "probe"});
  REQUIRE(types[0].Kind == DataKind::Intent);
  REQUIRE(types[1].Kind == DataKind::Derived);
  REQUIRE(types[2].Kind == DataKind::State);
}

} // namespace
} // namespace tpj
