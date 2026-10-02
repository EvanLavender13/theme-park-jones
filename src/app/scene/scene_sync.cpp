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
    meshes.FrameCamera = true;
  }
  DrawnIntent current{parkEntrances(world), parkPaths(world), parkBoxes(world)};
  if (Intent != current) {
    Intent = std::move(current);
    meshes.Park = true;
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

} // namespace tpj
