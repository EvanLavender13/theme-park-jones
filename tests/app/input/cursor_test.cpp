#include "app/input/cursor.h"
#include "app/session/park_file.h"
#include "render/picking.h"
#include "render/renderer.h"
#include "sim/entity_key.h"
#include "sim/park/intent.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace tpj {
namespace {

using Catch::Matchers::WithinAbs;

constexpr double NDC_TOLERANCE = 1e-6;

// The checked-in sketch park, which holds shops.
World sketchPark() {
  const std::string path = (std::filesystem::path(TPJ_PARKS_DIR) / "sketch.park").string();
  OpenedPark opened = openParkFile(path.c_str());
  if (!opened.Park.has_value()) {
    throw std::runtime_error(opened.Error);
  }
  return std::move(*opened.Park);
}

ParkBox firstShop(const World &world) {
  for (const ParkBox &box : parkBoxes(world)) {
    if (box.Kind == BoxKind::Shop) {
      return box;
    }
  }
  throw std::runtime_error("the park holds no shop");
}

void checkCursor(const std::optional<CursorNdc> &actual, double x, double y, double aspect) {
  REQUIRE(actual.has_value());
  if (!actual.has_value()) {
    return;
  }
  CHECK_THAT(actual->X, WithinAbs(x, NDC_TOLERANCE));
  CHECK_THAT(actual->Y, WithinAbs(y, NDC_TOLERANCE));
  CHECK_THAT(actual->Aspect, WithinAbs(aspect, NDC_TOLERANCE));
}

TEST_CASE("cursorNdc maps the window's top left to (-1, 1) and its bottom right to (1, -1), with "
          "its width over its height as the aspect") {
  checkCursor(cursorNdc(800, 600, 0.0f, 0.0f), -1.0, 1.0, 800.0 / 600.0);
  checkCursor(cursorNdc(800, 600, 800.0f, 600.0f), 1.0, -1.0, 800.0 / 600.0);
  checkCursor(cursorNdc(800, 600, 400.0f, 300.0f), 0.0, 0.0, 800.0 / 600.0);
  // A point off the center and the diagonal, in a window taller than wide.
  checkCursor(cursorNdc(400, 1000, 100.0f, 750.0f), -0.5, -0.5, 0.4);
}

TEST_CASE("cursorNdc gives none when the window's width or height is not positive") {
  CHECK_FALSE(cursorNdc(0, 600, 0.0f, 0.0f).has_value());
  CHECK_FALSE(cursorNdc(800, 0, 0.0f, 0.0f).has_value());
  CHECK_FALSE(cursorNdc(-800, 600, 0.0f, 0.0f).has_value());
  CHECK_FALSE(cursorNdc(800, -600, 0.0f, 0.0f).has_value());
}

TEST_CASE("groundUnderCursor gives groundAtCursor of the view and the cursor, and none without "
          "one") {
  CameraView view;
  view.Eye = Vec3{10.0f, 30.0f, 40.0f};
  view.Target = Vec3{0.0f, 0.0f, 0.0f};
  const CursorNdc cursor{.X = 0.3f, .Y = -0.2f, .Aspect = 1.5f};

  const std::optional<ParkPoint> expected = groundAtCursor(view, cursor.Aspect, cursor.X, cursor.Y);
  // An eye above the ground looking down at it has ground under a cursor near the center.
  REQUIRE(expected.has_value());
  CHECK(groundUnderCursor(cursor, view) == expected);
  CHECK_FALSE(groundUnderCursor(std::nullopt, view).has_value());
}

TEST_CASE("entityUnderCursor gives entityAtCursor of the world, the view, and the cursor, and none "
          "without one") {
  const World world = sketchPark();
  const ParkBox shop = firstShop(world);
  const auto shopX = static_cast<float>(shop.At.X);
  const auto shopZ = static_cast<float>(shop.At.Z);
  // Looking down on the shop from just off vertical, so the view's right is defined, a cursor at
  // the center meets the shop.
  CameraView view;
  view.Eye = Vec3{shopX, 40.0f, shopZ + 10.0f};
  view.Target = Vec3{shopX, 0.0f, shopZ};
  const CursorNdc cursor{.X = 0.0f, .Y = 0.0f, .Aspect = 1.0f};

  const std::optional<EntityKey> expected =
      entityAtCursor(world, view, cursor.Aspect, cursor.X, cursor.Y);
  REQUIRE(expected.has_value());
  CHECK(entityUnderCursor(world, cursor, view) == expected);
  CHECK_FALSE(entityUnderCursor(world, std::nullopt, view).has_value());
}

} // namespace
} // namespace tpj
