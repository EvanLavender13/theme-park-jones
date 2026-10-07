#ifndef TPJ_APP_SCENE_SCENE_UPLOADS_H
#define TPJ_APP_SCENE_SCENE_UPLOADS_H

#include "app/input/orbit_camera.h"
#include "app/scene/scene_sync.h"
#include "legible/preview.h"
#include "render/paving_overlay.h"
#include "render/renderer.h"
#include "sim/world.h"

namespace tpj {

// Builds the park and guest meshes the frame needs and gives them to the renderer, framing the
// camera on the park mesh's bounds when asked and the mesh has vertices. False if an upload failed.
bool uploadWorldMeshes(Renderer &renderer, const World &world, const WorldMeshes &meshes,
                       OrbitCamera &camera);
// Builds the ghost, meaning the preview's edit with its candidate's walkways and starved marks,
// then the look's highlight, then its inspected guest or shop's mark, and gives it to the
// renderer, with the food overlay's shape when it changed, and while the look shows the overlay,
// the shape's shades by the previewed world's food availability. False if an upload failed.
bool uploadPreviewMeshes(Renderer &renderer, const World &world, const Preview &preview,
                         const PreviewLook &look, const OverlayShape &shape, bool shapeChanged);

} // namespace tpj

#endif
