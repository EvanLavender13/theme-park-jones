#include "render/guest_mesh.h"

#include "sim/guests/guests.h"

#include <algorithm>
#include <optional>

namespace tpj {

Rgba guestColor(double hunger) {
  const auto h = static_cast<float>(std::clamp(hunger, 0.0, 1.0));
  const auto mix = [h](float sated, float hungry) { return sated + (hungry - sated) * h; };
  return {mix(GUEST_SATED_COLOR.R, GUEST_HUNGRY_COLOR.R),
          mix(GUEST_SATED_COLOR.G, GUEST_HUNGRY_COLOR.G),
          mix(GUEST_SATED_COLOR.B, GUEST_HUNGRY_COLOR.B), 1.0f};
}

void appendGuest(ParkMesh &mesh, GroundPoint point, double hunger) {
  Pose pose;
  pose.X = point.X;
  pose.Z = point.Z;
  appendBox(mesh, pose, GUEST_SIZE, GUEST_HEIGHT, guestColor(hunger));
}

ParkMesh buildGuestMesh(const World &world) {
  ParkMesh mesh;
  for (const EntityKey key : parkGuests(world)) {
    const std::optional<GuestRecord> record = guestRecord(world, key);
    if (record && record->Position) {
      appendGuest(mesh, *record->Position, record->Hunger);
    }
  }
  return mesh;
}

} // namespace tpj
