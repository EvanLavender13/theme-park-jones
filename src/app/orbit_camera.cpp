#include "app/orbit_camera.h"

#include <math.h>

namespace tpj {
namespace {

constexpr float MIN_PITCH = 0.17f;
constexpr float MAX_PITCH = 1.48f;
constexpr float MIN_DISTANCE = 4.0f;
constexpr float MAX_DISTANCE = 400.0f;
constexpr float ORBIT_RADIANS_PER_PIXEL = 0.005f;
constexpr float PAN_PER_PIXEL_PER_METER = 0.0015f;
constexpr float MOVE_PER_SECOND_PER_METER = 1.0f;
constexpr float ROTATE_RADIANS_PER_SECOND = 1.5f;
constexpr float ZOOM_FACTOR_PER_STEP = 0.88f;

float clampf(float value, float low, float high) { return fminf(fmaxf(value, low), high); }

} // namespace

void updateOrbitCamera(OrbitCamera &camera, const CameraInput &input, float dt,
                       float boundsHalfExtent) {
  camera.Yaw -= input.OrbitDx * ORBIT_RADIANS_PER_PIXEL;
  camera.Yaw += input.Rotate * ROTATE_RADIANS_PER_SECOND * dt;
  camera.Pitch =
      clampf(camera.Pitch + input.OrbitDy * ORBIT_RADIANS_PER_PIXEL, MIN_PITCH, MAX_PITCH);
  camera.Distance =
      clampf(camera.Distance * powf(ZOOM_FACTOR_PER_STEP, input.Zoom), MIN_DISTANCE, MAX_DISTANCE);

  // Panning scales with distance so the ground under the cursor moves at a steady rate.
  const Vec3 forward = {-sinf(camera.Yaw), 0.0f, -cosf(camera.Yaw)};
  const Vec3 right = {cosf(camera.Yaw), 0.0f, -sinf(camera.Yaw)};
  const float dragScale = PAN_PER_PIXEL_PER_METER * camera.Distance;
  const float moveScale = MOVE_PER_SECOND_PER_METER * camera.Distance * dt;
  const Vec3 offset = right * (input.MoveRight * moveScale - input.PanDx * dragScale) +
                      forward * (input.MoveForward * moveScale + input.PanDy * dragScale);
  camera.Focus = camera.Focus + offset;
  camera.Focus.X = clampf(camera.Focus.X, -boundsHalfExtent, boundsHalfExtent);
  camera.Focus.Z = clampf(camera.Focus.Z, -boundsHalfExtent, boundsHalfExtent);
}

Vec3 orbitCameraEye(const OrbitCamera &camera) {
  const float horizontal = cosf(camera.Pitch) * camera.Distance;
  const Vec3 offset = {sinf(camera.Yaw) * horizontal, sinf(camera.Pitch) * camera.Distance,
                       cosf(camera.Yaw) * horizontal};
  return camera.Focus + offset;
}

void frameOrbitCamera(OrbitCamera &camera, const GroundBounds &bounds, float fovY) {
  const float width = bounds.MaxX - bounds.MinX;
  const float depth = bounds.MaxZ - bounds.MinZ;
  const float radius = 0.5f * sqrtf(width * width + depth * depth);
  camera.Focus = {0.5f * (bounds.MinX + bounds.MaxX), 0.0f, 0.5f * (bounds.MinZ + bounds.MaxZ)};
  camera.Distance = clampf(radius / sinf(0.5f * fovY), MIN_DISTANCE, MAX_DISTANCE);
}

} // namespace tpj
