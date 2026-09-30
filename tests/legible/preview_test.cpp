#include "legible/preview.h"

#include "sim/command_queue.h"
#include "sim/entity_key.h"
#include "sim/guests/footfall.h"
#include "sim/medium/field.h"
#include "sim/medium/network.h"
#include "sim/operations/operations.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/routes/networks.h"
#include "sim/save.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdint.h>
#include <string>
#include <string_view>
#include <vector>

namespace tpj {
namespace {

using Catch::Matchers::WithinAbs;

// tests/parks/warm.park: guest path 2 runs from (0, 123) to (0, 100), where guest path 5 starts,
// backstage path 6 runs along x = 12, shop 7 faces guest path 2 and backs onto path 6, and depot 8
// supplies it. The saved park holds hungry footfall its guests have left on the paths.
constexpr EntityKey GUEST_PATH{2};
constexpr EntityKey BACKSTAGE_PATH{6};
constexpr EntityKey SHOP{7};
constexpr EntityKey DEPOT{8};
constexpr EntityKey MISSING{99};

// A shop between shop 7 and guest path 5, its front door at (3.5, 106.8), 3.5 m from guest path 2,
// 16.2 m along it, and its back door 2.5 m from backstage path 6, so it is connected and supplied.
constexpr Pose SUPPLIED_SHOP{6.5, 106.8, -1.0, 0.0};
// A shop facing +x whose front door, at (-3.5, 100), lies 3.5 m from where guest path 2 ends and
// guest path 5 starts, and whose back door reaches no backstage path, so it has no supply route.
constexpr Pose JUNCTION_SHOP{-6.5, 100.0, 1.0, 0.0};
// Open ground more than CONNECTION_REACH from every path, for a shop or a depot.
constexpr Pose OPEN_GROUND{-60.0, 40.0, 0.0, -1.0};
// Over shop 7.
constexpr Pose OVER_SHOP{6.5, 115.0, -1.0, 0.0};
// Over depot 8.
constexpr Pose OVER_DEPOT{12.0, 80.0, -1.0, 0.0};

// The distance along guest path 2 of the supplied shop's front door, and of the path's end.
constexpr double SUPPLIED_CONNECTION = 123.0 - 106.8;
constexpr double GUEST_PATH_LENGTH = 23.0;
// Carrier distances are sums of the path's sampled segments, so they may differ from the exact
// ones by rounding.
constexpr double DISTANCE_TOLERANCE = 1e-9;

World openWarm() {
  std::ifstream file(std::filesystem::path(TPJ_PARKS_DIR) / "warm.park", std::ios::binary);
  const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
  REQUIRE_FALSE(text.empty());
  World world = loadWorld(makeParkSchema(), text);
  resolveWorld(world);
  return world;
}

World candidateOf(const World &world, const std::vector<ParkEdit> &edits) {
  CommandQueue queue;
  for (const ParkEdit &edit : edits) {
    queueEdit(queue, edit);
  }
  return makeCandidate(world, queue);
}

World candidateOf(const World &world, const ParkEdit &edit) {
  return candidateOf(world, std::vector<ParkEdit>{edit});
}

// The keys of the candidate's boxes that the world has no box for, ascending.
std::vector<EntityKey> addedBoxes(const World &world, const World &candidate) {
  const std::vector<ParkBox> before = parkBoxes(world);
  std::vector<EntityKey> added;
  for (const ParkBox &box : parkBoxes(candidate)) {
    if (std::ranges::none_of(before, [&](const ParkBox &old) { return old.Key == box.Key; })) {
      added.push_back(box.Key);
    }
  }
  std::ranges::sort(added);
  return added;
}

ShopContext requireContext(const std::optional<ShopContext> &context) {
  REQUIRE(context.has_value());
  return context.value_or(ShopContext{});
}

TEST_CASE("A preview holds the edit it is given") {
  const World world = openWarm();
  const std::vector<std::optional<ParkEdit>> edits = {
      std::nullopt, ParkEdit{AddBox{BoxKind::Shop, SUPPLIED_SHOP}},
      ParkEdit{AddBox{BoxKind::Shop, OVER_SHOP}}};
  for (const std::optional<ParkEdit> &edit : edits) {
    INFO("edit given: " << edit.has_value());
    CHECK(previewEdit(world, edit).Edit == edit);
  }
}

TEST_CASE("A preview's candidate is the candidate of its edit when the edit is accepted, and none "
          "when it is refused or absent") {
  const World world = openWarm();
  // A shop added and a path deleted: an edit that adds an entity and one that removes one.
  for (const ParkEdit &accepted :
       {ParkEdit{AddBox{BoxKind::Shop, SUPPLIED_SHOP}}, ParkEdit{DeletePath{BACKSTAGE_PATH}}}) {
    REQUIRE(isAccepted(world, accepted));
    const Preview preview = previewEdit(world, accepted);
    REQUIRE(preview.Candidate.has_value());
    if (preview.Candidate.has_value()) {
      CHECK(worldsEqual(*preview.Candidate, candidateOf(world, accepted)));
    }
  }
  const ParkEdit refused = AddBox{BoxKind::Shop, OVER_SHOP};
  REQUIRE_FALSE(isAccepted(world, refused));
  CHECK_FALSE(previewEdit(world, refused).Candidate.has_value());
  CHECK_FALSE(previewEdit(world, std::nullopt).Candidate.has_value());
}

TEST_CASE("A preview's shop context is shopContext of its candidate when it holds one, and none "
          "when it holds no candidate") {
  const World world = openWarm();
  SECTION("an accepted shop") {
    const ParkEdit edit = AddBox{BoxKind::Shop, SUPPLIED_SHOP};
    const Preview preview = previewEdit(world, edit);
    REQUIRE(preview.Candidate.has_value());
    if (preview.Candidate.has_value()) {
      const std::optional<ShopContext> expected = shopContext(world, *preview.Candidate, edit);
      REQUIRE(expected.has_value());
      CHECK(preview.Shop == expected);
    }
  }
  // A move of a shop box names a shop whatever the candidate, so only the refusal can leave the
  // preview without a context.
  SECTION("a refused move of a shop box") {
    const ParkEdit edit = MoveBox{SHOP, OVER_DEPOT};
    REQUIRE_FALSE(isAccepted(world, edit));
    CHECK_FALSE(previewEdit(world, edit).Shop.has_value());
  }
  SECTION("no edit") { CHECK_FALSE(previewEdit(world, std::nullopt).Shop.has_value()); }
}

TEST_CASE("previewedWorld is the preview's candidate when it holds one, and the world otherwise") {
  const World world = openWarm();
  const Preview accepted = previewEdit(world, ParkEdit{AddBox{BoxKind::Shop, SUPPLIED_SHOP}});
  REQUIRE(accepted.Candidate.has_value());
  if (accepted.Candidate.has_value()) {
    CHECK(&previewedWorld(world, accepted) == &*accepted.Candidate);
  }
  const Preview refused = previewEdit(world, ParkEdit{AddBox{BoxKind::Shop, OVER_SHOP}});
  CHECK(&previewedWorld(world, refused) == &world);
  const Preview none = previewEdit(world, std::nullopt);
  CHECK(&previewedWorld(world, none) == &world);
}

TEST_CASE("shopContext of an AddBox of a shop gives a context for the lowest key of a box the "
          "candidate has and the world does not") {
  const World world = openWarm();
  const AddBox edit{BoxKind::Shop, SUPPLIED_SHOP};
  SECTION("the edit's own candidate") {
    const World candidate = candidateOf(world, edit);
    const std::vector<EntityKey> added = addedBoxes(world, candidate);
    REQUIRE(added.size() == 1);
    CHECK(requireContext(shopContext(world, candidate, edit)).Shop == added.front());
  }
  // Two shops added, so the lowest of the new keys differs from the highest.
  SECTION("a candidate that adds two shops") {
    const World candidate =
        candidateOf(world, {ParkEdit{edit}, ParkEdit{AddBox{BoxKind::Shop, JUNCTION_SHOP}}});
    const std::vector<EntityKey> added = addedBoxes(world, candidate);
    REQUIRE(added.size() == 2);
    CHECK(requireContext(shopContext(world, candidate, edit)).Shop == added.front());
  }
}

TEST_CASE("shopContext of a MoveBox of a shop box gives a context for that box") {
  const World world = openWarm();
  const MoveBox edit{SHOP, JUNCTION_SHOP};
  REQUIRE(isAccepted(world, edit));
  CHECK(requireContext(shopContext(world, candidateOf(world, edit), edit)).Shop == SHOP);
}

TEST_CASE("shopContext gives none for an edit that neither adds a shop nor moves a shop box, and "
          "for an AddBox of a shop whose candidate adds no box") {
  const World world = openWarm();
  const std::vector<ParkEdit> others = {
      AddBox{BoxKind::Depot, OPEN_GROUND},
      MoveBox{DEPOT, OPEN_GROUND},
      AddPath{PathKind::Guest, {{-60.0, 40.0}, {-60.0, 60.0}}},
      DeletePath{BACKSTAGE_PATH},
      // Deleting a shop box names a shop but neither adds nor moves one.
      DeleteBox{SHOP},
  };
  for (const ParkEdit &edit : others) {
    INFO("edit " << edit.index());
    REQUIRE(isAccepted(world, edit));
    CHECK_FALSE(shopContext(world, candidateOf(world, edit), edit).has_value());
  }
  // A MoveBox naming no box, against a candidate unchanged from the world.
  const World unchanged = copyWorld(world);
  CHECK_FALSE(shopContext(world, unchanged, MoveBox{MISSING, OPEN_GROUND}).has_value());
  CHECK_FALSE(shopContext(world, unchanged, AddBox{BoxKind::Shop, SUPPLIED_SHOP}).has_value());
}

TEST_CASE("A shop context's connection is the first guest path place at the node where the shop's "
          "guest connector meets the paths") {
  const World world = openWarm();
  SECTION("a door facing the middle of a guest path") {
    const AddBox edit{BoxKind::Shop, SUPPLIED_SHOP};
    const ShopContext context = requireContext(shopContext(world, candidateOf(world, edit), edit));
    REQUIRE(context.Connection.has_value());
    const Place connection = context.Connection.value_or(Place{});
    CHECK(connection.Carrier == GUEST_PATH);
    CHECK_THAT(connection.Distance, WithinAbs(SUPPLIED_CONNECTION, DISTANCE_TOLERANCE));
  }
  // Guest paths 2 and 5 both stop at that node, as the connector does, so the order shows: the
  // first place is on the lower-keyed path, at its end.
  SECTION("a door facing where two guest paths meet") {
    const AddBox edit{BoxKind::Shop, JUNCTION_SHOP};
    const ShopContext context = requireContext(shopContext(world, candidateOf(world, edit), edit));
    REQUIRE(context.Connection.has_value());
    const Place connection = context.Connection.value_or(Place{});
    CHECK(connection.Carrier == GUEST_PATH);
    CHECK_THAT(connection.Distance, WithinAbs(GUEST_PATH_LENGTH, DISTANCE_TOLERANCE));
  }
}

TEST_CASE("A shop whose front door reaches no guest path has a context with no connection and a "
          "footfall of 0.0") {
  const World world = openWarm();
  const AddBox edit{BoxKind::Shop, OPEN_GROUND};
  REQUIRE(isAccepted(world, edit));
  const ShopContext context = requireContext(shopContext(world, candidateOf(world, edit), edit));
  CHECK_FALSE(context.Connection.has_value());
  CHECK(context.Footfall == 0.0);
}

TEST_CASE("A shop context's footfall is the committed world's hungry footfall at its connection") {
  const World world = openWarm();
  const AddBox edit{BoxKind::Shop, SUPPLIED_SHOP};
  const ShopContext context = requireContext(shopContext(world, candidateOf(world, edit), edit));
  REQUIRE(context.Connection.has_value());
  const double committed = fieldValue<HungryFootfall>(world, parkNetwork(world, PathKind::Guest),
                                                      context.Connection.value_or(Place{}));
  // warm.park's guests have walked guest path 2, so the footfall there is not the default.
  CHECK(committed > 0.0);
  CHECK(context.Footfall == committed);
}

TEST_CASE("A shop context's supply is the nearest depot to its shop in the candidate") {
  const World world = openWarm();
  SECTION("an added shop backing onto the backstage path") {
    const AddBox edit{BoxKind::Shop, SUPPLIED_SHOP};
    const World candidate = candidateOf(world, edit);
    const ShopContext context = requireContext(shopContext(world, candidate, edit));
    CHECK(context.Supply.has_value());
    CHECK(context.Supply == nearestDepot(candidate, context.Shop));
  }
  // The shop is supplied where it stands and would not be where it moves, so the candidate's
  // route, not the world's, shows.
  SECTION("a supplied shop moved out of the backstage path's reach") {
    const MoveBox edit{SHOP, JUNCTION_SHOP};
    const World candidate = candidateOf(world, edit);
    REQUIRE(nearestDepot(world, SHOP).has_value());
    const ShopContext context = requireContext(shopContext(world, candidate, edit));
    CHECK_FALSE(context.Supply.has_value());
    CHECK(context.Supply == nearestDepot(candidate, SHOP));
  }
}

// Whether two previews agree: the same edit, candidates both none or equal, and the same context.
bool samePreview(const Preview &left, const Preview &right) {
  if (left.Edit != right.Edit || left.Shop != right.Shop ||
      left.Candidate.has_value() != right.Candidate.has_value()) {
    return false;
  }
  return !left.Candidate.has_value() || !right.Candidate.has_value() ||
         worldsEqual(*left.Candidate, *right.Candidate);
}

// A context no preview of warm.park gives, so a kept preview holding it shows it was not made
// again.
const ShopContext PLANTED_CONTEXT{MISSING, std::nullopt, -1.0, std::nullopt};

// A kept preview for the edit at the tick that previewEdit would never give: no candidate, and a
// planted context.
KeptPreview plantedPreview(std::optional<uint64_t> tick, const std::optional<ParkEdit> &edit) {
  KeptPreview kept;
  kept.Tick = tick;
  kept.Made.Edit = edit;
  kept.Made.Shop = PLANTED_CONTEXT;
  return kept;
}

TEST_CASE("keepPreview with nothing kept makes the preview, keeping previewEdit's preview at the "
          "world's tick") {
  const World world = openWarm();
  const std::optional<ParkEdit> edit = ParkEdit{AddBox{BoxKind::Shop, SUPPLIED_SHOP}};
  KeptPreview kept;
  CHECK(keepPreview(kept, world, edit));
  CHECK(kept.Tick == world.Tick);
  CHECK(samePreview(kept.Made, previewEdit(world, edit)));
}

TEST_CASE("keepPreview makes the preview again when the kept tick or edit differs from the world's "
          "tick or the edit, keeping previewEdit's preview at the world's tick") {
  const World world = openWarm();
  const std::optional<ParkEdit> addShop = ParkEdit{AddBox{BoxKind::Shop, SUPPLIED_SHOP}};
  KeptPreview kept;
  std::optional<ParkEdit> edit;
  SECTION("the world has ticked since") {
    kept = plantedPreview(world.Tick - 1, addShop);
    edit = addShop;
  }
  SECTION("the edit has changed") {
    kept = plantedPreview(world.Tick, ParkEdit{DeletePath{BACKSTAGE_PATH}});
    edit = addShop;
  }
  SECTION("the edit has gone") {
    kept = plantedPreview(world.Tick, addShop);
    edit = std::nullopt;
  }
  CHECK(keepPreview(kept, world, edit));
  CHECK(kept.Tick == world.Tick);
  CHECK(samePreview(kept.Made, previewEdit(world, edit)));
}

TEST_CASE("keepPreview leaves the kept preview unchanged when its tick is the world's and its edit "
          "is the edit") {
  const World world = openWarm();
  for (const std::optional<ParkEdit> &edit :
       {std::optional<ParkEdit>{AddBox{BoxKind::Shop, SUPPLIED_SHOP}}, std::optional<ParkEdit>{}}) {
    INFO("edit given: " << edit.has_value());
    KeptPreview kept = plantedPreview(world.Tick, edit);
    CHECK_FALSE(keepPreview(kept, world, edit));
    CHECK(kept.Tick == world.Tick);
    CHECK(samePreview(kept.Made, plantedPreview(world.Tick, edit).Made));
  }
}

// Principle 1: a preview is shown, never committed, so making one leaves nothing in the world.
TEST_CASE("Making previews, kept previews, and shop contexts leaves the world's hash unchanged") {
  const World world = openWarm();
  const uint64_t before = hashWorld(world);
  const std::vector<std::optional<ParkEdit>> edits = {
      std::nullopt,
      ParkEdit{AddBox{BoxKind::Shop, SUPPLIED_SHOP}},
      ParkEdit{MoveBox{SHOP, JUNCTION_SHOP}},
      ParkEdit{DeletePath{BACKSTAGE_PATH}},
      ParkEdit{AddBox{BoxKind::Shop, OVER_SHOP}},
  };
  KeptPreview kept;
  for (const std::optional<ParkEdit> &edit : edits) {
    const Preview preview = previewEdit(world, edit);
    if (edit.has_value() && preview.Candidate.has_value()) {
      static_cast<void>(shopContext(world, *preview.Candidate, *edit));
    }
    static_cast<void>(keepPreview(kept, world, edit));
  }
  CHECK(hashWorld(world) == before);
}

} // namespace
} // namespace tpj
