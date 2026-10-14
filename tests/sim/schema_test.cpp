#include "support/synthetic_types.h"

#include "sim/schema.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <string>
#include <vector>

namespace tpj {
namespace {

using test::Probe;
using test::Tag;

void resolveNothing(World & /*world*/, TickBuffers & /*buffers*/) {}
void resolveNothingElse(World & /*world*/, TickBuffers & /*buffers*/) {}

std::vector<std::string> namesOf(const WorldSchema &schema) {
  std::vector<std::string> names;
  for (const ComponentType &type : schema.components()) {
    names.push_back(type.Name);
  }
  return names;
}

TEST_CASE("a schema accepts a name exactly when it is lowercase letters, digits, and hyphens") {
  WorldSchema schema;

  REQUIRE_NOTHROW(schema.addComponent<Tag>("tag-2", DataKind::Intent));
  // A letter, but not a lowercase one.
  REQUIRE_THROWS_AS(schema.addComponent<Probe>("Probe", DataKind::State), std::invalid_argument);
  REQUIRE(namesOf(schema) == std::vector<std::string>{"tag-2"});
}

// A repeated name or type would make two registrations indistinguishable to the walk and the save,
// and saves reserve the name entities for their list of bare keys.
TEST_CASE("a schema refuses a repeated or reserved registration and is left unchanged") {
  WorldSchema schema;
  schema.addComponent<Tag>("tag", DataKind::Intent);

  REQUIRE_THROWS_AS(schema.addComponent<Probe>("tag", DataKind::State), std::invalid_argument);
  REQUIRE_THROWS_AS(schema.addComponent<Probe>("entities", DataKind::State), std::invalid_argument);
  REQUIRE(namesOf(schema) == std::vector<std::string>{"tag"});
  // The refused type was not half registered.
  REQUIRE_NOTHROW(schema.addComponent<Probe>("probe", DataKind::State));
}

std::vector<std::string> resolverNamesOf(const WorldSchema &schema) {
  std::vector<std::string> names;
  for (const ResolverType &resolver : schema.resolvers()) {
    names.push_back(resolver.Name);
  }
  return names;
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

} // namespace
} // namespace tpj
