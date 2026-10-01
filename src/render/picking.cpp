#include "render/picking.h"

#include "render/guest_mesh.h"
#include "render/park_mesh.h"
#include "sim/guests/guests.h"
#include "sim/park/geometry.h"

#include <algorithm>
#include <limits>
#include <math.h>
#include <utility>

namespace tpj {
namespace {

// Narrows [near, far] to the ray parameters at which a coordinate, starting at origin and moving
// by step per unit, lies within [low, high]. False when that leaves no parameter.
bool clipSlab(double origin, double step, double low, double high, double &near, double &far) {
  if (step == 0.0) {
    return origin >= low && origin <= high;
  }
  double enter = (low - origin) / step;
  double leave = (high - origin) / step;
  if (enter > leave) {
    std::swap(enter, leave);
  }
  near = std::max(near, enter);
  far = std::min(far, leave);
  return near <= far;
}

} // namespace

CursorRay cursorRay(const CameraView &view, float aspect, float ndcX, float ndcY) {
  const Vec3 forward = normalize(view.Target - view.Eye);
  const Vec3 right = normalize(cross(forward, {0.0f, 1.0f, 0.0f}));
  const Vec3 up = cross(right, forward);
  const float tanHalf = tanf(0.5f * view.FovY);
  return {view.Eye, forward + right * (ndcX * tanHalf * aspect) + up * (ndcY * tanHalf)};
}

std::optional<ParkPoint> groundAtCursor(const CameraView &view, float aspect, float ndcX,
                                        float ndcY) {
  const CursorRay ray = cursorRay(view, aspect, ndcX, ndcY);
  if (ray.Origin.Y <= 0.0f || ray.Direction.Y >= 0.0f) {
    return std::nullopt;
  }
  const float t = -ray.Origin.Y / ray.Direction.Y;
  return ParkPoint{ray.Origin.X + ray.Direction.X * t, ray.Origin.Z + ray.Direction.Z * t};
}

std::optional<double> rayEntry(const CursorRay &ray, const Pose &pose, FootprintSize size,
                               float height) {
  const std::optional<Footprint> footprint = footprintOf(pose, size);
  if (!footprint) {
    return std::nullopt;
  }
  // In the footprint's own frame: along Forward, across it along Right, and up.
  const double dx = ray.Origin.X - pose.X;
  const double dz = ray.Origin.Z - pose.Z;
  const auto along = [](double x, double z, ParkPoint axis) { return x * axis.X + z * axis.Z; };
  const ParkPoint forward = footprint->Forward;
  const ParkPoint right = footprint->Right;
  double near = 0.0;
  double far = std::numeric_limits<double>::infinity();
  const bool meets =
      clipSlab(along(dx, dz, forward), along(ray.Direction.X, ray.Direction.Z, forward),
               -0.5 * size.Depth, 0.5 * size.Depth, near, far) &&
      clipSlab(along(dx, dz, right), along(ray.Direction.X, ray.Direction.Z, right),
               -0.5 * size.Width, 0.5 * size.Width, near, far) &&
      clipSlab(ray.Origin.Y, ray.Direction.Y, 0.0, height, near, far);
  if (!meets) {
    return std::nullopt;
  }
  return near;
}

std::optional<EntityKey> entityAtCursor(const World &world, const CameraView &view, float aspect,
                                        float ndcX, float ndcY) {
  const CursorRay ray = cursorRay(view, aspect, ndcX, ndcY);
  std::optional<EntityKey> nearest;
  double nearestEntry = 0.0;
  // A later solid replaces an earlier one only when strictly nearer.
  const auto consider = [&nearest, &nearestEntry](EntityKey key, std::optional<double> entry) {
    if (entry && (!nearest || *entry < nearestEntry)) {
      nearest = key;
      nearestEntry = *entry;
    }
  };
  for (const ParkEntrance &entrance : parkEntrances(world)) {
    consider(entrance.Key, rayEntry(ray, entrance.At, ENTRANCE_SIZE, ENTRANCE_HEIGHT));
  }
  for (const ParkBox &box : parkBoxes(world)) {
    consider(box.Key, rayEntry(ray, box.At, boxSize(box.Kind), boxHeight(box.Kind)));
  }
  for (const EntityKey guest : parkGuests(world)) {
    const std::optional<GuestRecord> record = guestRecord(world, guest);
    if (record && record->Position) {
      consider(guest, rayEntry(ray, guestPose(*record->Position), GUEST_SIZE, GUEST_HEIGHT));
    }
  }
  return nearest;
}

} // namespace tpj
