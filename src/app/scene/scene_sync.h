#ifndef TPJ_APP_SCENE_SCENE_SYNC_H
#define TPJ_APP_SCENE_SCENE_SYNC_H

#include "legible/inspect.h"
#include "legible/preview.h"
#include "render/guest_mesh.h"
#include "render/park_mesh.h"
#include "render/paving_overlay.h"
#include "sim/guests/guests.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/world.h"
#include "tools/tools.h"
#include "views/availability_overlay.h"

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

// Which of the preview's meshes a frame rebuilt: the ghost and the overlay's shades.
struct PreviewMeshes {
  bool Ghost = false;
  bool Shades = false;

  bool operator==(const PreviewMeshes &) const = default;
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
  // After the panels: builds the parts of the ghost whose sources changed, and the overlay's
  // shades, and says which of the two it rebuilt. The edit's ghost, the preview's edit with its
  // candidate's walkways and starved marks, is built at the first call and when the preview was
  // just made again. The look's marks are built when the look or the edit count differs from those
  // they were built at: where a path tool's first click would start the path, where a click would
  // finish it, the highlight, the point markers, Edit path's radius and edge handles, the handle or
  // sides under the pointer, and an inspected shop, bench, or toilet block. The inspected guest's
  // mark is built when either of those is, or when the last syncWorld rebuilt the guests while the
  // subject is a guest, so the mark follows the guest where the seated guests draw it. When any
  // part is built, the ghost is put together from them in that order. While the look shows a need's
  // overlay, the shades are the previewed world's availability of that need over the overlay's
  // shape, built again when the preview was just made again, the shape changed, or the need or the
  // previewed world's tick differs from those they were built for. Remembers the look. Once its
  // lists have grown, a call frees no memory it allocated.
  PreviewMeshes syncLook(const World &world, bool remade, const PreviewLook &look,
                         bool shapeChanged);
  [[nodiscard]] const GhostMesh &ghost() const { return Ghost; }
  [[nodiscard]] const OverlayShades &shades() const { return ShadeLists.Shades; }
  // Keeps the overlay's shape for the previewed world: while shown, built again when the edit
  // count differs from the one it was built at, or when the preview's edit differs while the
  // preview holds a candidate of an edit that can change paths. These fix the previewed world's
  // path points and segments. While not shown, it is empty. Returns whether the shape changed.
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
  // The edit count the look's marks were built at.
  std::optional<uint64_t> LookEdits;
  // The ghost's parts, and the ghost put together from them. The look's marks use only Lit and
  // Marks.
  GhostMesh EditGhost;
  GhostMesh LookMarks;
  ParkMesh SubjectMark;
  GhostMesh Ghost;
  // What the overlay's shape was built for, while it is shown: the edit count, and the preview's
  // edit when the preview held a candidate of an edit that can change paths.
  struct ShapedFor {
    std::optional<uint64_t> Edits;
    std::optional<ParkEdit> Edit;
  };
  std::optional<ShapedFor> Shaped;
  OverlayShape Shape;
  // The previewed world's tick and the need the shades were built for, while an overlay is shown.
  struct ShadedFor {
    uint64_t Tick = 0;
    GuestNeed Need = GuestNeed::Hunger;
  };
  std::optional<ShadedFor> Shaded;
  AvailabilityShadeLists ShadeLists;
};

} // namespace tpj

#endif
