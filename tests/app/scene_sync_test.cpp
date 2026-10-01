#include "app/park_file_requests.h"
#include "app/park_session.h"
#include "app/scene_sync.h"
#include "legible/inspect.h"
#include "legible/preview.h"
#include "sim/command_queue.h"
#include "sim/entity_key.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

namespace tpj {
namespace {

// A session asked only for New, which neither confirms nor reports.
class SilentEdge : public ParkFileEdge {
public:
  bool confirmReplace(const std::string & /*path*/) override { return false; }
  void reportError(const std::string & /*message*/) override {}
};

constexpr WorldMeshes EVERY_MESH{.Park = true, .Guests = true, .FrameCamera = true};
constexpr WorldMeshes PARK_MESH{.Park = true, .Guests = false, .FrameCamera = false};
constexpr WorldMeshes GUEST_MESH{.Park = false, .Guests = true, .FrameCamera = false};
constexpr WorldMeshes NO_MESH{};

// Shops on open ground in the new park.
ParkEdit shopAt(double x, double z) { return AddBox{BoxKind::Shop, Pose{x, z, 0.0, -1.0}}; }

// The world with the edit applied and resolved but not stepped: other intent at the same tick.
World withEdit(const World &world, const ParkEdit &edit) {
  CommandQueue queue;
  queueEdit(queue, edit);
  return makeCandidate(world, queue);
}

// The world one cycle on: the same intent at another tick.
World stepped(const World &world) {
  World next = copyWorld(world);
  stepWorld(next);
  return next;
}

void checkSamePreview(const Preview &actual, const Preview &expected) {
  CHECK(actual.Edit == expected.Edit);
  CHECK(actual.Shop == expected.Shop);
  CHECK(actual.Candidate.has_value() == expected.Candidate.has_value());
  if (actual.Candidate.has_value() && expected.Candidate.has_value()) {
    CHECK(worldsEqual(*actual.Candidate, *expected.Candidate));
  }
}

TEST_CASE("syncWorld asks for every mesh and the camera framed at its first call and for each "
          "generation it has not seen, even when the world is unchanged") {
  // A session started from the new park and given New holds an equal world at tick 0 under a new
  // generation, so only the generation can tell the replacement apart.
  SilentEdge edge;
  ParkSession session(resolvedNewPark());
  SceneSync sync;

  CHECK(sync.syncWorld(session.world(), session.generation()) == EVERY_MESH);
  CHECK(sync.syncWorld(session.world(), session.generation()) == NO_MESH);

  session.useFileRequest(FileRequest{ParkAction::New, ""}, edge);

  CHECK(sync.syncWorld(session.world(), session.generation()) == EVERY_MESH);
  CHECK(sync.syncWorld(session.world(), session.generation()) == NO_MESH);
}

TEST_CASE("For the generation it last saw, syncWorld asks for the park mesh exactly when the "
          "world's intent differs from the last call's, and never for the camera framed") {
  const World park = resolvedNewPark();
  const World withShop = withEdit(park, shopAt(-40.0, -60.0));
  REQUIRE(parkBoxes(withShop) != parkBoxes(park));
  REQUIRE(withShop.Tick == park.Tick);
  SceneSync sync;
  sync.syncWorld(park, 7);

  // The comparison is with the last call's intent, so the mesh is asked for once per change,
  // including a change back.
  CHECK(sync.syncWorld(withShop, 7) == PARK_MESH);
  CHECK(sync.syncWorld(withShop, 7) == NO_MESH);
  CHECK(sync.syncWorld(park, 7) == PARK_MESH);
  // An equal world that is another object holds the same intent.
  CHECK(sync.syncWorld(copyWorld(park), 7) == NO_MESH);
}

TEST_CASE("For the generation it last saw, syncWorld asks for the guest mesh exactly when the "
          "world's tick differs from the last call's, and never for the camera framed") {
  const World park = resolvedNewPark();
  const World later = stepped(park);
  REQUIRE(later.Tick != park.Tick);
  REQUIRE(parkBoxes(later) == parkBoxes(park));
  SceneSync sync;
  sync.syncWorld(park, 7);

  CHECK(sync.syncWorld(later, 7) == GUEST_MESH);
  CHECK(sync.syncWorld(later, 7) == NO_MESH);
  // Other intent at the tick last seen asks for the park mesh alone.
  CHECK(sync.syncWorld(withEdit(later, shopAt(-40.0, -60.0)), 7) == PARK_MESH);
}

TEST_CASE("A generation syncWorld has not seen empties the kept preview, so the next syncPreview "
          "makes it again for the tick and edit it kept") {
  // A kept candidate of the replaced world would outlive it, and the new world is equal at the
  // same tick, so only the generation can empty it.
  SilentEdge edge;
  ParkSession session(resolvedNewPark());
  const std::optional<ParkEdit> edit = shopAt(-40.0, -60.0);
  SceneSync sync;
  sync.syncWorld(session.world(), session.generation());
  CHECK(sync.syncPreview(session.world(), edit));

  // The generation it last saw keeps the preview.
  sync.syncWorld(session.world(), session.generation());
  CHECK_FALSE(sync.syncPreview(session.world(), edit));

  session.useFileRequest(FileRequest{ParkAction::New, ""}, edge);
  sync.syncWorld(session.world(), session.generation());

  CHECK_FALSE(sync.preview().Edit.has_value());
  CHECK_FALSE(sync.preview().Candidate.has_value());
  CHECK(sync.syncPreview(session.world(), edit));
  checkSamePreview(sync.preview(), previewEdit(session.world(), edit));
}

TEST_CASE("syncPreview returns keepPreview's answer for the kept preview, the world, and the edit, "
          "and preview() is the kept preview's Made") {
  // Each call differs from the one before in one way keepPreview decides on: the first call, the
  // same tick and edit, another edit, another tick, and no edit.
  const World park = resolvedNewPark();
  const World later = stepped(park);
  struct Frame {
    const World *Park;
    std::optional<ParkEdit> Edit;
  };
  const std::vector<Frame> frames{
      {&park, shopAt(-40.0, -60.0)}, {&park, shopAt(-40.0, -60.0)}, {&park, shopAt(40.0, -60.0)},
      {&later, shopAt(40.0, -60.0)}, {&later, std::nullopt},
  };
  SceneSync sync;
  KeptPreview expected;

  for (size_t index = 0; index < frames.size(); ++index) {
    INFO("frame " << index);
    const Frame &frame = frames[index];
    sync.syncWorld(*frame.Park, 7);

    const bool remade = sync.syncPreview(*frame.Park, frame.Edit);

    CHECK(remade == keepPreview(expected, *frame.Park, frame.Edit));
    checkSamePreview(sync.preview(), expected.Made);
  }
}

TEST_CASE("syncLook returns true at its first call") {
  SceneSync sync;

  CHECK(sync.syncLook(false, PreviewLook{}));
}

TEST_CASE("syncLook returns true when told the preview was made again, even for the last look") {
  SceneSync sync;
  sync.syncLook(false, PreviewLook{});

  CHECK(sync.syncLook(true, PreviewLook{}));
  CHECK_FALSE(sync.syncLook(false, PreviewLook{}));
}

TEST_CASE("syncLook returns true exactly when the look differs from the last call's") {
  // Each part of the look changes what the ghost or the overlay shows.
  const PreviewLook plain;
  PreviewLook changed;
  SECTION("The highlight") { changed.Highlight = EntityKey{4}; }
  SECTION("The Inspector's subject") {
    changed.Inspected = InspectorSubject{EntityKey{5}, SubjectKind::Shop};
  }
  SECTION("Whether the food overlay shows") { changed.FoodOverlay = true; }
  SceneSync sync;
  sync.syncLook(false, plain);

  CHECK_FALSE(sync.syncLook(false, plain));
  CHECK(sync.syncLook(false, changed));
  CHECK_FALSE(sync.syncLook(false, changed));
  CHECK(sync.syncLook(false, plain));
}

} // namespace
} // namespace tpj
