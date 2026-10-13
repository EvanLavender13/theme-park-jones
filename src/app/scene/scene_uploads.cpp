#include "app/scene/scene_uploads.h"

#include "render/park_mesh.h"

#include <optional>

namespace tpj {

bool uploadWorldMeshes(Renderer &renderer, const World &world, const WorldMeshes &meshes,
                       const ParkMesh &guests, OrbitCamera &camera) {
  if (meshes.Park) {
    const ParkMesh mesh = buildParkMesh(world);
    if (meshes.FrameCamera) {
      if (const std::optional<GroundBounds> bounds = meshBounds(mesh)) {
        frameOrbitCamera(camera, *bounds, CameraView{}.FovY);
      }
    }
    if (!setParkMesh(renderer, mesh)) {
      return false;
    }
  }
  return !meshes.Guests || setGuestMesh(renderer, guests);
}

bool uploadPreviewMeshes(Renderer &renderer, const SceneSync &scene, PreviewMeshes meshes,
                         bool shapeChanged) {
  return (!meshes.Ghost || setGhostMesh(renderer, scene.ghost())) &&
         (!shapeChanged || setOverlayShape(renderer, scene.overlayShape())) &&
         (!meshes.Shades || setOverlayShades(renderer, scene.shades()));
}

} // namespace tpj
