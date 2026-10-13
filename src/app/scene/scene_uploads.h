#ifndef TPJ_APP_SCENE_SCENE_UPLOADS_H
#define TPJ_APP_SCENE_SCENE_UPLOADS_H

#include "app/input/orbit_camera.h"
#include "app/scene/scene_sync.h"
#include "render/guest_mesh.h"
#include "render/renderer.h"
#include "sim/world.h"

namespace tpj {

// Gives the renderer the meshes the frame needs: the park mesh, built from the world, and the
// guest mesh, as the scene's sync built it. Frames the camera on the park mesh's bounds when asked
// and the mesh has vertices. False if an upload failed.
bool uploadWorldMeshes(Renderer &renderer, const World &world, const WorldMeshes &meshes,
                       const ParkMesh &guests, OrbitCamera &camera);
// Gives the renderer the preview meshes the scene's sync built: the ghost when the frame rebuilt
// it, the overlay's shape when it changed, and the overlay's shades when the frame rebuilt them.
// False if an upload failed.
bool uploadPreviewMeshes(Renderer &renderer, const SceneSync &scene, PreviewMeshes meshes,
                         bool shapeChanged);

} // namespace tpj

#endif
