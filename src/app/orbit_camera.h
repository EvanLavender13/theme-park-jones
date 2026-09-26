#ifndef TPJ_APP_ORBIT_CAMERA_H
#define TPJ_APP_ORBIT_CAMERA_H

#include "render/math.h"

namespace tpj {

// A camera orbiting a focus point on the ground. Yaw 0 looks toward -Z; pitch is the angle
// above the horizon.
struct OrbitCamera {
  Vec3 Focus;
  float Yaw = 0.6f;
  float Pitch = 0.75f;
  float Distance = 90.0f;
};

// Input gathered over one frame. Mouse deltas are in pixels; move and rotate axes are in
// [-1, 1] from held keys; Zoom is wheel steps, positive toward the focus.
struct CameraInput {
  float OrbitDx = 0.0f;
  float OrbitDy = 0.0f;
  float PanDx = 0.0f;
  float PanDy = 0.0f;
  float Zoom = 0.0f;
  float MoveForward = 0.0f;
  float MoveRight = 0.0f;
  float Rotate = 0.0f;
};

// Applies one frame of input. The focus stays within a square of `boundsHalfExtent` meters
// around the origin.
void updateOrbitCamera(OrbitCamera &camera, const CameraInput &input, float dt,
                       float boundsHalfExtent);

Vec3 orbitCameraEye(const OrbitCamera &camera);

} // namespace tpj

#endif
