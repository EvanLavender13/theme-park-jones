// Previews on a checked-in park: placing a shop in tests/parks/warm.park, the preview shows the
// park its commit makes, with every capability resolving the candidate, and the food availability
// it shows rises beside the new shop. It names a park and an edit, so it is not a standard that
// park_standards_test.cpp could check over every park file, and it is not a food loop consequence
// of food_loop_test.cpp.

#include "support/park_files.h"

#include "legible/food.h"
#include "legible/path_place.h"
#include "legible/preview.h"
#include "sim/command_queue.h"
#include "sim/medium/network.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <stdint.h>
#include <string_view>

namespace tpj {
namespace {

using test::openPark;
using test::parkFile;

World openParkFile(std::string_view name) { return openPark(parkFile(name)); }

// A second shop beside warm.park's shop, facing guest path 2 along x = 0 and backing onto
// backstage path 6 along x = 12: its front door, at (3.5, 106.8), is 3.5 m from the guest path,
// and its back door, at (9.5, 106.8), is 2.5 m from the backstage path, both within reach, so it
// touches both paths. Its footprint, z from 102.8 to 110.8, clears the first shop's, which starts
// at z = 111, and guest path 5, which leaves guest path 2's end at z = 100.
constexpr Pose SECOND_SHOP{6.5, 106.8, -1.0, 0.0};
constexpr GroundPoint SECOND_SHOP_FRONT_DOOR{3.5, 106.8};

// Catches a preview that is not the park its edit commits, such as a candidate whose new shop is
// not connected, supplied, or offered as committing makes it, or whose guests are carried
// differently; and a new shop whose offer is not published until it steps, so the preview shows
// no gain.
TEST_CASE("In warm.park, the preview of a second shop touching both paths is the park its "
          "placement commits, and food availability beside the new shop rises") {
  const World warm = openParkFile("warm.park");
  const AddBox placement{BoxKind::Shop, SECOND_SHOP};

  // The preview is made from the world as a cycle leaves it, and the placement is committed in
  // that cycle.
  World before = copyWorld(warm);
  stepWorld(before);
  REQUIRE(isAccepted(before, ParkEdit{placement}));
  const Preview preview = previewEdit(before, ParkEdit{placement});
  REQUIRE(preview.Candidate.has_value());
  World committed = copyWorld(warm);
  CommandQueue commands;
  queueEdit(commands, ParkEdit{placement});
  stepWorld(committed, commands);
  const World &candidate = previewedWorld(before, preview);
  CHECK(worldsEqual(candidate, committed));

  const std::optional<PathPlace> beside = nearestGuestPathPlace(before, SECOND_SHOP_FRONT_DOOR);
  REQUIRE(beside.has_value());
  const Place place = beside.value_or(PathPlace{}).At;
  INFO("place on carrier " << static_cast<uint64_t>(place.Carrier) << " at " << place.Distance);
  const double availabilityBefore = foodAvailability(before, place).Value;
  const double availabilityPreviewed = foodAvailability(candidate, place).Value;
  CHECK(availabilityPreviewed > availabilityBefore);
}

} // namespace
} // namespace tpj
