#include "render/picking.h"

#include "render/math.h"
#include "render/renderer.h"
#include "sim/park/intent.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <optional>
#include <string_view>

namespace tpj {
namespace {

// The round trip is checked to a thousandth of the screen's half extent.
constexpr double NDC_TOLERANCE = 1e-3;

struct Ndc {
  double X = 0.0;
  double Y = 0.0;
};

// Where drawFrame's projection, perspective after a look from the eye to the target with +Y up,
// puts a point on the ground.
Ndc project(const CameraView &view, float aspect, const ParkPoint &point) {
  const Mat4 viewProjection = multiply(perspective(view.FovY, aspect, view.NearZ, view.FarZ),
                                       lookAt(view.Eye, view.Target, Vec3{0.0f, 1.0f, 0.0f}));
  const auto &m = viewProjection.M;
  const auto x = static_cast<float>(point.X);
  const auto z = static_cast<float>(point.Z);
  // Column-major: element (row, col) is M[col * 4 + row], and the point is (x, 0, z, 1).
  const auto row = [&m, x, z](int r) { return m[r] * x + m[8 + r] * z + m[12 + r]; };
  const double w = row(3);
  return {row(0) / w, row(1) / w};
}

struct PickCase {
  std::string_view Name;
  CameraView View;
  float Aspect = 1.0f;
  Ndc Cursor;
};

// An oblique view like the orbit camera's, looking down at a target on the ground.
constexpr CameraView OBLIQUE{Vec3{30.0f, 40.0f, 60.0f}, Vec3{5.0f, 0.0f, -10.0f}};
// A level view, whose rays descend only below the middle of the screen.
constexpr CameraView LEVEL{Vec3{0.0f, 10.0f, 20.0f}, Vec3{0.0f, 10.0f, 0.0f}};

TEST_CASE("groundAtCursor gives a ground point that drawFrame's projection maps back to the "
          "cursor") {
  const PickCase cases[] = {
      {"the middle of a wide oblique view", OBLIQUE, 16.0f / 9.0f, {0.0, 0.0}},
      // Off both axes, so x and y must each take their own share of the field of view.
      {"toward a corner of a wide oblique view", OBLIQUE, 16.0f / 9.0f, {0.8, -0.7}},
      // A tall aspect, which must scale x alone.
      {"a tall oblique view", OBLIQUE, 0.6f, {-0.9, 0.4}},
      {"below the middle of a level view", LEVEL, 1.5f, {0.3, -0.5}},
      // A target above the ground, so the ground point is not the target.
      {"a view of a target in the air",
       CameraView{Vec3{-20.0f, 15.0f, -35.0f}, Vec3{10.0f, 2.0f, 5.0f}},
       4.0f / 3.0f,
       {0.2, 0.1}},
  };
  for (const PickCase &pick : cases) {
    INFO(pick.Name);
    const std::optional<ParkPoint> ground =
        groundAtCursor(pick.View, pick.Aspect, static_cast<float>(pick.Cursor.X),
                       static_cast<float>(pick.Cursor.Y));
    REQUIRE(ground.has_value());
    const Ndc back = project(pick.View, pick.Aspect, ground.value_or(ParkPoint{}));
    CHECK(std::abs(back.X - pick.Cursor.X) <= NDC_TOLERANCE);
    CHECK(std::abs(back.Y - pick.Cursor.Y) <= NDC_TOLERANCE);
  }
}

TEST_CASE("groundAtCursor gives none when the eye is not above the ground") {
  // Looking down, so the rays descend, from below the ground and from exactly on it.
  CHECK_FALSE(groundAtCursor(CameraView{Vec3{0.0f, -5.0f, 10.0f}, Vec3{0.0f, -10.0f, 0.0f}}, 1.5f,
                             0.0f, 0.0f)
                  .has_value());
  CHECK_FALSE(groundAtCursor(CameraView{Vec3{0.0f, 0.0f, 10.0f}, Vec3{0.0f, -10.0f, 0.0f}}, 1.5f,
                             0.0f, 0.0f)
                  .has_value());
}

TEST_CASE("groundAtCursor gives none when the ray through the cursor does not descend") {
  // The middle of a level view runs parallel to the ground, and above it the rays climb.
  CHECK_FALSE(groundAtCursor(LEVEL, 1.5f, 0.0f, 0.0f).has_value());
  CHECK_FALSE(groundAtCursor(LEVEL, 1.5f, -0.3f, 0.5f).has_value());
}

} // namespace
} // namespace tpj
