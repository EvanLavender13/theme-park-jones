#ifndef TPJ_RENDER_GUEST_MESH_H
#define TPJ_RENDER_GUEST_MESH_H

#include "render/park_mesh.h"
#include "sim/entity_key.h"
#include "sim/medium/network.h"
#include "sim/park/intent.h"
#include "sim/world.h"

namespace tpj {

// A guest is drawn as an upright box of this footprint and height, colored from sated to hungry.
inline constexpr FootprintSize GUEST_SIZE{0.6, 0.6};
inline constexpr float GUEST_HEIGHT = 1.8f;
inline constexpr Rgba GUEST_SATED_COLOR{0.30f, 0.75f, 0.95f, 1.0f};
inline constexpr Rgba GUEST_HUNGRY_COLOR{0.95f, 0.20f, 0.50f, 1.0f};

// The sated color moved toward the hungry one by the hunger, clamped to [0, 1].
Rgba guestColor(double hunger);
// Where a guest's box stands for its ground point: there, with the default facing.
Pose guestPose(GroundPoint point);
// Adds a guest's box standing at the point, in its hunger's color.
void appendGuest(ParkMesh &mesh, GroundPoint point, double hunger);
// Adds the guest the key holds where buildGuestMesh draws it, in the color. Nothing when the key
// holds no guest or its place does not resolve.
void appendGuestEntity(ParkMesh &mesh, const World &world, EntityKey key, Rgba color);
// Every guest whose place resolves, in key order.
ParkMesh buildGuestMesh(const World &world);

} // namespace tpj

#endif
