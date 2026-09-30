#ifndef TPJ_RENDER_GUEST_MESH_H
#define TPJ_RENDER_GUEST_MESH_H

#include "render/park_mesh.h"
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
// Adds a guest's box standing at the point, in its hunger's color.
void appendGuest(ParkMesh &mesh, GroundPoint point, double hunger);
// Every guest whose place resolves, in key order.
ParkMesh buildGuestMesh(const World &world);

} // namespace tpj

#endif
