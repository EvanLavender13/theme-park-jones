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

} // namespace
} // namespace tpj
