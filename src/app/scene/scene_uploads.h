#ifndef TPJ_APP_SCENE_SCENE_UPLOADS_H
#define TPJ_APP_SCENE_SCENE_UPLOADS_H

#include "app/input/orbit_camera.h"
#include "app/scene/scene_sync.h"
#include "legible/preview.h"
#include "render/guest_mesh.h"
#include "render/paving_overlay.h"
#include "render/renderer.h"
#include "sim/world.h"

namespace tpj {

// Builds the park and guest meshes the frame needs, the guests on the seated guests' seats, and
// gives them to the renderer, framing the camera on the park mesh's bounds when asked and the mesh
// has vertices. False if an upload failed.
bool uploadWorldMeshes(Renderer &renderer, const World &world, const WorldMeshes &meshes,
                       const SeatedGuests &seated, OrbitCamera &camera);
// Builds the ghost, meaning the preview's edit with its candidate's walkways and starved marks,
// then the look's highlight, then its inspected guest's or box's mark, and gives it to the
// renderer, with the overlay's shape when it changed, and while the look shows a need's overlay,
// the shape's shades by the previewed world's availability of that need. An inspected guest is
// marked where the seated guests draw it. False if an upload failed.
bool uploadPreviewMeshes(Renderer &renderer, const World &world, const Preview &preview,
                         const PreviewLook &look, const SeatedGuests &seated,
                         const OverlayShape &shape, bool shapeChanged);

} // namespace tpj

#endif
