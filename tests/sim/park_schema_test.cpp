#include "sim/park_schema.h"
#include "sim/schema.h"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <memory>

namespace tpj {
namespace {

TEST_CASE("makeParkSchema holds exactly the component types, systems, swaps, resolvers, "
          "finishers, and commands that addPark registers into an empty schema") {
  WorldSchema added;
  addPark(added);
  const std::shared_ptr<const WorldSchema> park = makeParkSchema();
  REQUIRE(park != nullptr);

  CHECK(park->sameComponents(added));
  CHECK(park->systems() == added.systems());
  CHECK(park->swaps() == added.swaps());
  CHECK(park->finishers() == added.finishers());

  REQUIRE(park->resolvers().size() == added.resolvers().size());
  for (std::size_t index = 0; index < added.resolvers().size(); ++index) {
    const ResolverType &left = park->resolvers()[index];
    const ResolverType &right = added.resolvers()[index];
    CAPTURE(index, right.Name);
    CHECK(left.Name == right.Name);
    CHECK(left.Resolve == right.Resolve);
    CHECK(left.Dependencies == right.Dependencies);
  }

  REQUIRE(park->commands().size() == added.commands().size());
  for (std::size_t index = 0; index < added.commands().size(); ++index) {
    CAPTURE(index);
    CHECK(park->commands()[index].TypeId == added.commands()[index].TypeId);
    CHECK(park->commands()[index].Apply == added.commands()[index].Apply);
  }
}

} // namespace
} // namespace tpj
