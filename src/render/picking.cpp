#include "render/picking.h"

#include <math.h>

namespace tpj {

std::optional<ParkPoint> groundAtCursor(const CameraView &view, float aspect, float ndcX,
                                        float ndcY) {
  const Vec3 forward = normalize(view.Target - view.Eye);
  const Vec3 right = normalize(cross(forward, {0.0f, 1.0f, 0.0f}));
  const Vec3 up = cross(right, forward);
  const float tanHalf = tanf(0.5f * view.FovY);
  const Vec3 direction = forward + right * (ndcX * tanHalf * aspect) + up * (ndcY * tanHalf);
  if (view.Eye.Y <= 0.0f || direction.Y >= 0.0f) {
    return std::nullopt;
  }
  const float t = -view.Eye.Y / direction.Y;
  return ParkPoint{view.Eye.X + direction.X * t, view.Eye.Z + direction.Z * t};
}

} // namespace tpj
