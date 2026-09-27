#include "synthetic_types.h"

#include "sim/schema.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <string>
#include <vector>

namespace tpj {
namespace {

using test::Cached;
using test::Probe;
using test::Tag;

struct Rename {
  int Id = 0;
};

[[maybe_unused]] void applyCommand(World & /*world*/, const Rename & /*command*/) {}

void resolveNothing(World & /*world*/) {}
void resolveNothingElse(World & /*world*/) {}

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

std::vector<std::string> resolverNamesOf(const WorldSchema &schema) {
  std::vector<std::string> names;
  for (const ResolverType &resolver : schema.resolvers()) {
    names.push_back(resolver.Name);
  }
  return names;
}

TEST_CASE("resolvers() keeps registration order with each resolver's dependencies") {
  WorldSchema schema;
  schema.addResolver("zones", resolveNothing);
  schema.addResolver("access", resolveNothingElse, {"zones"});

  REQUIRE(resolverNamesOf(schema) == std::vector<std::string>{"zones", "access"});
  REQUIRE(schema.resolvers()[0].Dependencies.empty());
  REQUIRE(schema.resolvers()[1].Dependencies == std::vector<std::string>{"zones"});
  REQUIRE(schema.resolvers()[1].Resolve == &resolveNothingElse);
}

TEST_CASE("a schema refuses a resolver with a malformed name and is left unchanged") {
  WorldSchema schema;
  schema.addResolver("zones", resolveNothing);

  const std::vector<std::string> malformed = {"Access", "my_access", "my access"};
  for (const std::string &name : malformed) {
    CAPTURE(name);
    REQUIRE_THROWS_AS(schema.addResolver(name, resolveNothingElse), std::invalid_argument);
    REQUIRE(resolverNamesOf(schema) == std::vector<std::string>{"zones"});
  }
}

TEST_CASE("a schema refuses a resolver with a repeated name and is left unchanged") {
  WorldSchema schema;
  schema.addResolver("zones", resolveNothing);

  REQUIRE_THROWS_AS(schema.addResolver("zones", resolveNothingElse), std::invalid_argument);
  REQUIRE(resolverNamesOf(schema) == std::vector<std::string>{"zones"});
  REQUIRE(schema.resolvers()[0].Resolve == &resolveNothing);
}

// Only a dependency registered earlier is accepted, so registration order is a dependency order
// and no dependency cycle can be written down.
TEST_CASE("a schema refuses a resolver depending on anything but an already registered resolver") {
  WorldSchema schema;
  schema.addComponent<Probe>("probe", DataKind::State);
  schema.addResolver("zones", resolveNothing);

  // A name registered nowhere, a component's name, the resolver's own name, and a good dependency
  // alongside a bad one.
  const std::vector<std::vector<std::string>> refused = {
      {"missing"}, {"probe"}, {"access"}, {"zones", "missing"}};
  for (const std::vector<std::string> &dependencies : refused) {
    CAPTURE(dependencies);
    REQUIRE_THROWS_AS(schema.addResolver("access", resolveNothingElse, dependencies),
                      std::invalid_argument);
    REQUIRE(resolverNamesOf(schema) == std::vector<std::string>{"zones"});
  }
  // Once its dependencies are registered, the same resolver is accepted: the refused name was not
  // reserved, and only the order was wrong.
  schema.addResolver("missing", resolveNothing);
  REQUIRE_NOTHROW(schema.addResolver("access", resolveNothingElse, {"zones", "missing"}));
  REQUIRE(resolverNamesOf(schema) == std::vector<std::string>{"zones", "missing", "access"});
}

TEST_CASE("a schema refuses a command type registered twice and is left unchanged") {
  WorldSchema schema;
  schema.addCommand<Rename>();
  REQUIRE(schema.commands().size() == 1);

  REQUIRE_THROWS_AS(schema.addCommand<Rename>(), std::invalid_argument);
  REQUIRE(schema.commands().size() == 1);
  REQUIRE(schema.commands()[0].TypeId == entt::type_id<Rename>().hash());
}

} // namespace
} // namespace tpj
