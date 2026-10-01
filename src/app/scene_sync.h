#ifndef TPJ_APP_SCENE_SYNC_H
#define TPJ_APP_SCENE_SYNC_H

#include "legible/inspect.h"
#include "legible/preview.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/world.h"

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
// Inspector's subject, and whether the food overlay shows.
struct PreviewLook {
  std::optional<EntityKey> Highlight;
  std::optional<InspectorSubject> Inspected;
  bool FoodOverlay = false;

  bool operator==(const PreviewLook &) const = default;
};

// What the scene's meshes were last built from, and the kept preview, so that each frame rebuilds
// exactly the meshes whose sources changed. A world of a generation it has not seen rebuilds every
// mesh.
class SceneSync {
public:
  // After the frame's ticks: which of the park and guest meshes the world needs, remembering it as
  // drawn. For a generation other than the last one it saw, as at the first call, both, with the
  // camera framed, and the kept preview is emptied. Otherwise the park mesh when the world's intent
  // differs from the last call's, and the guest mesh when its tick does.
  WorldMeshes syncWorld(const World &world, uint64_t generation);
  // After the pointer moves: keeps the preview of the edit as keepPreview does, returning whether
  // it was made again.
  bool syncPreview(const World &world, const std::optional<ParkEdit> &edit);
  [[nodiscard]] const Preview &preview() const { return Kept.Made; }
  // After the panels: whether the ghost and overlay meshes need building, which they do at the
  // first call, when the preview was just made again, or when the look differs from the last
  // call's. Remembers the look.
  bool syncLook(bool remade, const PreviewLook &look);

private:
  // The intent a park mesh was built from.
  struct DrawnIntent {
    std::vector<ParkEntrance> Entrances;
    std::vector<ParkPath> Paths;
    std::vector<ParkBox> Boxes;

    bool operator==(const DrawnIntent &) const = default;
  };

  std::optional<uint64_t> Generation;
  std::optional<DrawnIntent> Intent;
  std::optional<uint64_t> GuestTick;
  KeptPreview Kept;
  std::optional<PreviewLook> Look;
};

} // namespace tpj

#endif
