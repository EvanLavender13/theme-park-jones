#ifndef TPJ_APP_SCENE_SCENE_SYNC_H
#define TPJ_APP_SCENE_SCENE_SYNC_H

#include "legible/inspect.h"
#include "legible/preview.h"
#include "render/guest_mesh.h"
#include "render/paving_overlay.h"
#include "sim/guests/guests.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/world.h"
#include "tools/tools.h"

#include <optional>
#include <stdint.h>
#include <vector>

namespace tpj {

// The park and guest meshes a frame rebuilds, and whether to frame the camera on the park mesh.
struct WorldMeshes {
  bool Park = false;
  bool Guests = false;
  bool FrameCamera = false;

  bool operator==(const WorldMeshes &) const = default;
};

// What the ghost and overlay meshes are built from besides the preview: the tool's highlight, the
// Inspector's subject, which need's overlay shows, if any, where a path tool's first click would
// start the path, and Edit path's radius and edge handles and the handle under the pointer.
struct PreviewLook {
  std::optional<EntityKey> Highlight;
  std::optional<InspectorSubject> Inspected;
  std::optional<GuestNeed> Overlay;
  std::optional<PathStart> Start;
  std::optional<ParkPoint> Finish;
  std::vector<EditHandle> Handles;
  std::optional<EditHandle> Hovered;

  bool operator==(const PreviewLook &) const = default;
};

// What the scene's meshes were last built from, and the kept preview, so that each frame rebuilds
// exactly the meshes whose sources changed. A world of a generation it has not seen rebuilds every
// mesh.
class SceneSync {
public:
  // After the frame's ticks: which of the park and guest meshes the world needs, remembering it as
  // drawn. The edit count is ParkSession::edits(). For a generation other than the last one it
  // saw, as at the first call, both, with the camera framed, and the kept preview is emptied.
  // Otherwise the park mesh when the edit count differs from the last call's, and the guest mesh
  // when the edit count or the world's tick does. When the edit count differs, the kept preview is
  // emptied, so an edit applied without a tick is previewed afresh. Whenever the guest mesh is
  // needed, the seated guests are brought up to date, starting afresh at a new generation, and the
  // guest mesh is built from them. Once its lists have grown, a call frees no memory it allocated.
  WorldMeshes syncWorld(const World &world, uint64_t generation, uint64_t edits);
  // The seat each seated guest is drawn on, as the last syncWorld left them.
  [[nodiscard]] const SeatedGuests &seated() const { return Seated; }
  // The guests as the last syncWorld that needed them drew them.
  [[nodiscard]] const ParkMesh &guestMesh() const { return GuestLists.Mesh; }
  // After the pointer moves: keeps the preview of the edit as keepPreview does, following ticks
  // when asked, returning whether it was made again.
  bool syncPreview(const World &world, const std::optional<ParkEdit> &edit, bool followTicks);
  [[nodiscard]] const Preview &preview() const { return Kept.Made; }
  // After the panels: whether the ghost and overlay meshes need building, which they do at the
  // first call, when the preview was just made again, when the look differs from the last call's,
  // or, while the Inspector's subject is a guest, when the last syncWorld rebuilt the guests, so
  // the guest's mark follows it. Remembers the look.
  bool syncLook(bool remade, const PreviewLook &look);
  // Keeps the overlay's shape for the previewed world: while shown, built again when the
  // world's path points or segments differ from those it was last built from, and while not
  // shown, empty. Returns whether the shape changed.
  bool syncOverlayShape(const World &previewed, bool shown);
  [[nodiscard]] const OverlayShape &overlayShape() const { return Shape; }

private:
  std::optional<uint64_t> Generation;
  // The edit count the park mesh was last built at.
  std::optional<uint64_t> Edits;
  std::optional<uint64_t> GuestTick;
  SeatedGuests Seated;
  GuestMeshLists GuestLists;
  // Whether the last syncWorld rebuilt the guests.
  bool GuestsRebuilt = false;
  KeptPreview Kept;
  std::optional<PreviewLook> Look;
  // The path points and segments the overlay's shape was built from, while it is shown.
  struct ShapedPaths {
    std::vector<ParkPathPoint> Points;
    std::vector<ParkSegment> Segments;

    bool operator==(const ShapedPaths &) const = default;
  };
  std::optional<ShapedPaths> Shaped;
  OverlayShape Shape;
};

} // namespace tpj

#endif
