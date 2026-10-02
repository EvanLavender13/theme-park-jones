#include "app/scene/scene_uploads.h"

#include "render/guest_mesh.h"
#include "render/park_mesh.h"
#include "views/food_overlay.h"

#include <optional>

namespace tpj {

namespace {

// The frame's ghost: the preview's edit with its candidate's walkways and starved marks, then the
// highlight, then the inspected guest or shop's mark.
ParkMesh ghostMesh(const World &world, const Preview &preview, const PreviewLook &look) {
  ParkMesh mesh;
  if (preview.Edit) {
    mesh = buildGhostMesh(world, *preview.Edit, preview.Candidate);
  }
  if (look.Highlight) {
    appendEntity(mesh, world, *look.Highlight, HIGHLIGHT_TINT);
  }
  // Each adds nothing for a key of the other's kind.
  if (look.Inspected) {
    appendEntity(mesh, world, look.Inspected->Key, HIGHLIGHT_TINT);
    appendGuestEntity(mesh, world, look.Inspected->Key, HIGHLIGHT_TINT);
  }
  return mesh;
}

// The food overlay while it is shown, shaded by the food availability at each place, and an empty
// mesh while it is not.
ParkMesh foodOverlayMesh(const World &world, bool show) {
  if (!show) {
    return {};
  }
  return buildFoodAvailabilityOverlay(world);
}

} // namespace

bool uploadWorldMeshes(Renderer &renderer, const World &world, const WorldMeshes &meshes,
                       OrbitCamera &camera) {
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
  return !meshes.Guests || setGuestMesh(renderer, buildGuestMesh(world));
}

bool uploadPreviewMeshes(Renderer &renderer, const World &world, const Preview &preview,
                         const PreviewLook &look) {
  return setGhostMesh(renderer, ghostMesh(world, preview, look)) &&
         setOverlayMesh(renderer,
                        foodOverlayMesh(previewedWorld(world, preview), look.FoodOverlay));
}

} // namespace tpj
