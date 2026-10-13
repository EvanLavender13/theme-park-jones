#ifndef TPJ_RENDER_GUEST_MESH_H
#define TPJ_RENDER_GUEST_MESH_H

#include "render/park_mesh.h"
#include "sim/entity_key.h"
#include "sim/furniture/furniture.h"
#include "sim/medium/network.h"
#include "sim/park/intent.h"
#include "sim/world.h"

#include <optional>
#include <stdint.h>
#include <utility>
#include <vector>

namespace tpj {

// A guest is drawn as an upright box of this footprint and height, colored from sated to
// distressed.
inline constexpr FootprintSize GUEST_SIZE{0.6, 0.6};
inline constexpr float GUEST_HEIGHT = 1.8f;
// A seated guest is drawn as a box of GUEST_SIZE and this height on its seat.
inline constexpr float GUEST_SEATED_HEIGHT = 1.2f;
inline constexpr Rgba GUEST_SATED_COLOR{0.30f, 0.75f, 0.95f, 1.0f};
inline constexpr Rgba GUEST_DISTRESSED_COLOR{0.95f, 0.20f, 0.50f, 1.0f};

// The seat each seated guest is drawn on, kept from one update to the next. A guest whose record
// gives a seat place keeps its seat while it stays seated at the same piece, and guests newly
// seated take the lowest free seat of their piece, in key order. A guest with no free seat is drawn
// standing. Drawing only: no save holds it.
class SeatedGuests {
public:
  // Brings the seats up to date with the world's guests. Allocates nothing once its lists have
  // grown.
  void update(const World &world);
  // The pose of the guest's seat, or none when it isn't drawn seated.
  [[nodiscard]] std::optional<Pose> seatOf(EntityKey guest) const;

private:
  // A guest's seat: the piece, the seat's index, and its pose.
  struct Taken {
    EntityKey Guest = NULL_KEY;
    EntityKey Piece = NULL_KEY;
    uint32_t Seat = 0;
    Pose At;
  };

  // Ascending by guest.
  std::vector<Taken> Seats;
  // Used only during update: the world's guests, the seats being made, and the guests newly
  // seated.
  std::vector<EntityKey> Guests;
  std::vector<Taken> Next;
  std::vector<std::pair<EntityKey, SeatPlace>> Newcomers;
};

// The sated color moved toward the distressed one by the need, clamped to [0, 1].
Rgba guestColor(double need);
// Where a guest's box stands for its ground point: there, with the default facing.
Pose guestPose(GroundPoint point);
// Adds a guest's box standing at the point, in its most pressing need's color.
void appendGuest(ParkMesh &mesh, GroundPoint point, double need);
// Adds the guest the key holds where buildGuestMesh draws it, in the color. Nothing when the key
// holds no guest or it has no position.
void appendGuestEntity(ParkMesh &mesh, const World &world, const SeatedGuests &seated,
                       EntityKey key, Rgba color);
// The lists building the guest mesh reuses: the guests' keys and the mesh.
struct GuestMeshLists {
  std::vector<EntityKey> Guests;
  ParkMesh Mesh;
};
// Every guest that has a position, in key order, on its seat while seated, else standing, in
// lists.Mesh. Uses lists the caller keeps, which it clears first, and allocates nothing once they
// have grown.
void buildGuestMesh(const World &world, const SeatedGuests &seated, GuestMeshLists &lists);

} // namespace tpj

#endif
