#include "render/guest_mesh.h"

#include "sim/guests/guests.h"

#include <algorithm>
#include <optional>

namespace tpj {

Rgba guestColor(double need) {
  const auto n = static_cast<float>(std::clamp(need, 0.0, 1.0));
  const auto mix = [n](float sated, float distressed) { return sated + (distressed - sated) * n; };
  return {mix(GUEST_SATED_COLOR.R, GUEST_DISTRESSED_COLOR.R),
          mix(GUEST_SATED_COLOR.G, GUEST_DISTRESSED_COLOR.G),
          mix(GUEST_SATED_COLOR.B, GUEST_DISTRESSED_COLOR.B), 1.0f};
}

Pose guestPose(GroundPoint point) {
  Pose pose;
  pose.X = point.X;
  pose.Z = point.Z;
  return pose;
}

void appendGuest(ParkMesh &mesh, GroundPoint point, double need) {
  appendBox(mesh, guestPose(point), GUEST_SIZE, GUEST_HEIGHT, guestColor(need));
}

void appendGuestEntity(ParkMesh &mesh, const World &world, EntityKey key, Rgba color) {
  const std::optional<GuestRecord> record = guestRecord(world, key);
  if (record && record->Position) {
    appendBox(mesh, guestPose(*record->Position), GUEST_SIZE, GUEST_HEIGHT, color);
  }
}

ParkMesh buildGuestMesh(const World &world) {
  ParkMesh mesh;
  for (const EntityKey key : parkGuests(world)) {
    const std::optional<GuestRecord> record = guestRecord(world, key);
    if (record && record->Position) {
      appendGuest(mesh, *record->Position,
                  std::max({record->Hunger, record->Thirst, record->Tiredness, record->Bladder}));
    }
  }
  return mesh;
}

} // namespace tpj
