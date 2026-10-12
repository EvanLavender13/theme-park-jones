#include "app/scene/scene_uploads.h"

#include "legible/needs.h"
#include "render/guest_mesh.h"
#include "render/park_mesh.h"
#include "views/availability_overlay.h"

#include <optional>

namespace tpj {

namespace {

// The frame's ghost: the preview's edit with its candidate's walkways and starved marks, and where
// a path tool's first click would start the path, then, among its marks, where a click would finish
// it, then the highlight, then the point markers, Edit path's radius and edge handles, and the
// handle under the pointer, then the inspected guest or shop's mark.
GhostMesh ghostMesh(const World &world, const Preview &preview, const PreviewLook &look,
                    const SeatedGuests &seated) {
  GhostMesh ghost;
  if (preview.Edit) {
    ghost = buildGhostMesh(world, *preview.Edit, preview.Candidate);
  }
  ParkMesh &marks = ghost.Marks;
  // Where a path would start previews its paving, lit as the park is. Only its join mark is a mark.
  if (look.Start) {
    appendPathStart(ghost.Lit, look.Start->Kind, look.Start->At, false);
    if (look.Start->Joins) {
      appendJoinMark(marks, look.Start->At);
    }
  }
  if (look.Finish) {
    appendFinishMark(marks, *look.Finish);
  }
  if (look.Highlight) {
    appendEntity(marks, world, *look.Highlight, HIGHLIGHT_TINT);
  }
  for (const EditHandle &handle : look.Handles) {
    if (handle.Kind == HandleKind::Point) {
      appendPointMark(marks, handle.At, POINT_MARK_COLOR);
    } else if (handle.Kind == HandleKind::Radius) {
      appendHandle(marks, handle.At, HANDLE_COLOR);
    } else if (handle.Kind == HandleKind::Taper) {
      appendHandle(marks, handle.At, TAPER_HANDLE_COLOR);
    } else {
      appendHandle(marks, handle.At, EDGE_HANDLE_COLOR);
    }
  }
  if (look.Hovered && look.Hovered->Kind != HandleKind::Side) {
    appendHandle(marks, look.Hovered->At, HIGHLIGHT_TINT);
  } else if (look.Hovered) {
    appendSides(marks, world, look.Hovered->Key, HIGHLIGHT_TINT);
  }
  // Each adds nothing for a key of the other's kind.
  if (look.Inspected) {
    appendEntity(marks, world, look.Inspected->Key, HIGHLIGHT_TINT);
    appendGuestEntity(marks, world, seated, look.Inspected->Key, HIGHLIGHT_TINT);
  }
  return ghost;
}

} // namespace

bool uploadWorldMeshes(Renderer &renderer, const World &world, const WorldMeshes &meshes,
                       const SeatedGuests &seated, OrbitCamera &camera) {
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
  return !meshes.Guests || setGuestMesh(renderer, buildGuestMesh(world, seated));
}

bool uploadPreviewMeshes(Renderer &renderer, const World &world, const Preview &preview,
                         const PreviewLook &look, const SeatedGuests &seated,
                         const OverlayShape &shape, bool shapeChanged) {
  if (!setGhostMesh(renderer, ghostMesh(world, preview, look, seated)) ||
      (shapeChanged && !setOverlayShape(renderer, shape))) {
    return false;
  }
  return !look.Overlay ||
         setOverlayShades(renderer, availabilityShades(previewedWorld(world, preview), shape,
                                                       needAvailability(*look.Overlay)));
}

} // namespace tpj
