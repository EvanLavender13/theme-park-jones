#include "app/scene/scene_sync.h"

#include <utility>

namespace tpj {

WorldMeshes SceneSync::syncWorld(const World &world, uint64_t generation) {
  WorldMeshes meshes;
  if (Generation != generation) {
    Generation = generation;
    Intent.reset();
    GuestTick.reset();
    Kept = {};
    Shaped.reset();
    meshes.FrameCamera = true;
  }
  DrawnIntent current{parkEntrances(world), parkPathPoints(world), parkSegments(world),
                      parkBoxes(world), parkHours(world)};
  if (Intent != current) {
    Intent = std::move(current);
    meshes.Park = true;
    // An edit applied while paused changes the world without a tick, and may move guests.
    meshes.Guests = true;
    Kept = {};
  }
  if (GuestTick != world.Tick) {
    GuestTick = world.Tick;
    meshes.Guests = true;
  }
  return meshes;
}

bool SceneSync::syncPreview(const World &world, const std::optional<ParkEdit> &edit) {
  return keepPreview(Kept, world, edit);
}

bool SceneSync::syncLook(bool remade, const PreviewLook &look) {
  if (!remade && Look == look) {
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
