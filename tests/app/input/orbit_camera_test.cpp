#include "app/input/orbit_camera.h"
#include "render/math.h"
#include "render/renderer.h"

#include <catch2/catch_test_macros.hpp>

namespace tpj {
namespace {

void checkSameVec(const Vec3 &actual, const Vec3 &expected) {
  CHECK(actual.X == expected.X);
  CHECK(actual.Y == expected.Y);
  CHECK(actual.Z == expected.Z);
}

TEST_CASE("orbitCameraView looks from the camera's eye to its focus, its other fields a default "
          "CameraView's") {
  const OrbitCamera camera{
      .Focus = Vec3{3.0f, 0.0f, -7.0f}, .Yaw = 1.1f, .Pitch = 0.5f, .Distance = 40.0f};
  const CameraView view = orbitCameraView(camera);
  const CameraView defaults;
  checkSameVec(view.Eye, orbitCameraEye(camera));
  checkSameVec(view.Target, camera.Focus);
  CHECK(view.FovY == defaults.FovY);
  CHECK(view.NearZ == defaults.NearZ);
  CHECK(view.FarZ == defaults.FarZ);
}

} // namespace
} // namespace tpj
