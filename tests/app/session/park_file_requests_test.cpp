#include "app/session/park_file_requests.h"

#include <catch2/catch_test_macros.hpp>

#include <string>

namespace tpj {
namespace {

TEST_CASE("With no dialog showing, pressing New records New with an empty path and shows no "
          "dialog") {
  ParkFileRequests requests;

  CHECK(requests.press(ParkAction::New) == ParkDialog::None);

  CHECK_FALSE(requests.dialogShowing());
  const FileRequest request = requests.take();
  CHECK(request.Action == ParkAction::New);
  CHECK(request.Path.empty());
}

TEST_CASE("With no dialog showing, pressing Open or Save returns that dialog, which then shows") {
  ParkFileRequests requests;

  SECTION("Open") {
    CHECK(requests.press(ParkAction::Open) == ParkDialog::Open);
    CHECK(requests.dialogShowing());
  }
  SECTION("Save") {
    CHECK(requests.press(ParkAction::Save) == ParkDialog::Save);
    CHECK(requests.dialogShowing());
  }
}

TEST_CASE("A press while a dialog shows, or of None, returns no dialog and leaves the request as "
          "it was") {
  // A request with a path, so a press that touched the request would show in the action or the
  // path.
  ParkFileRequests requests;
  REQUIRE(requests.press(ParkAction::Open) == ParkDialog::Open);
  requests.answer(ParkDialog::Open, "chosen.park");
  REQUIRE_FALSE(requests.dialogShowing());

  SECTION("None with no dialog showing") {
    CHECK(requests.press(ParkAction::None) == ParkDialog::None);
    CHECK_FALSE(requests.dialogShowing());
  }
  SECTION("Every action while a dialog shows") {
    REQUIRE(requests.press(ParkAction::Save) == ParkDialog::Save);
    CHECK(requests.press(ParkAction::New) == ParkDialog::None);
    CHECK(requests.press(ParkAction::Open) == ParkDialog::None);
    CHECK(requests.press(ParkAction::Save) == ParkDialog::None);
    CHECK(requests.press(ParkAction::None) == ParkDialog::None);
    CHECK(requests.dialogShowing());
  }

  const FileRequest request = requests.take();
  CHECK(request.Action == ParkAction::Open);
  CHECK(request.Path == "chosen.park");
}

TEST_CASE("An answer with a chosen path leaves no dialog showing and makes the request the "
          "answering dialog's action with that path") {
  ParkFileRequests requests;

  SECTION("Open, with no request before") {
    REQUIRE(requests.press(ParkAction::Open) == ParkDialog::Open);
    requests.answer(ParkDialog::Open, "opened.park");

    CHECK_FALSE(requests.dialogShowing());
    const FileRequest request = requests.take();
    CHECK(request.Action == ParkAction::Open);
    CHECK(request.Path == "opened.park");
  }
  SECTION("Save, replacing a New not yet taken") {
    REQUIRE(requests.press(ParkAction::New) == ParkDialog::None);
    REQUIRE(requests.press(ParkAction::Save) == ParkDialog::Save);
    requests.answer(ParkDialog::Save, "saved");

    CHECK_FALSE(requests.dialogShowing());
    const FileRequest request = requests.take();
    CHECK(request.Action == ParkAction::Save);
    CHECK(request.Path == "saved");
  }
}

TEST_CASE("An answer with no chosen path leaves no dialog showing and the request as it was") {
  // A cancelled or failed dialog chooses nothing, so the New pressed before it must survive.
  ParkFileRequests requests;
  REQUIRE(requests.press(ParkAction::New) == ParkDialog::None);
  REQUIRE(requests.press(ParkAction::Open) == ParkDialog::Open);

  requests.answer(ParkDialog::Open, nullptr);

  CHECK_FALSE(requests.dialogShowing());
  const FileRequest request = requests.take();
  CHECK(request.Action == ParkAction::New);
  CHECK(request.Path.empty());
}

TEST_CASE("take gives the request and leaves none, so the next take gives None with an empty "
          "path") {
  ParkFileRequests requests;
  REQUIRE(requests.press(ParkAction::Save) == ParkDialog::Save);
  requests.answer(ParkDialog::Save, "saved.park");

  const FileRequest first = requests.take();
  const FileRequest second = requests.take();

  CHECK(first.Action == ParkAction::Save);
  CHECK(first.Path == "saved.park");
  CHECK(second.Action == ParkAction::None);
  CHECK(second.Path.empty());
}

} // namespace
} // namespace tpj
