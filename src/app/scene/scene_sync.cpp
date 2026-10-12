#include "app/scene/scene_sync.h"

#include <utility>

namespace tpj {

WorldMeshes SceneSync::syncWorld(const World &world, uint64_t generation, uint64_t edits) {
  WorldMeshes meshes;
  if (Generation != generation) {
    Generation = generation;
    Edits.reset();
    GuestTick.reset();
    Kept = {};
    Shaped.reset();
    Seated = {};
    meshes.FrameCamera = true;
  }
  if (Edits != edits) {
    Edits = edits;
    meshes.Park = true;
    // An edit applied while paused changes the world without a tick, and may move guests.
    meshes.Guests = true;
    Kept = {};
  }
  if (GuestTick != world.Tick) {
    GuestTick = world.Tick;
    meshes.Guests = true;
  }
  if (meshes.Guests) {
    Seated.update(world);
    buildGuestMesh(world, Seated, GuestLists);
  }
  GuestsRebuilt = meshes.Guests;
  return meshes;
}

bool SceneSync::syncPreview(const World &world, const std::optional<ParkEdit> &edit,
                            bool followTicks) {
  return keepPreview(Kept, world, edit, followTicks);
}

bool SceneSync::syncLook(bool remade, const PreviewLook &look) {
  const bool guestMoved =
      GuestsRebuilt && look.Inspected && look.Inspected->Kind == SubjectKind::Guest;
  if (!remade && Look == look && !guestMoved) {
    return false;
  }
  Look = look;
  return true;
}

bool SceneSync::syncOverlayShape(const World &previewed, bool shown) {
  if (!shown) {
    Shaped.reset();
    if (Shape.Pieces.empty()) {
      return false;
    }
    Shape = {};
    return true;
  }
  ShapedPaths current{parkPathPoints(previewed), parkSegments(previewed)};
  if (Shaped == current) {
    return false;
  }
  Shaped = std::move(current);
  Shape = buildOverlayShape(previewed);
  return true;
}

} // namespace tpj
