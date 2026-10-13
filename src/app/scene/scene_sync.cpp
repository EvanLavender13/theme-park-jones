#include "app/scene/scene_sync.h"

#include "legible/needs.h"

#include <span>
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
    Look.reset();
    LookEdits.reset();
    Shaded.reset();
    Seated = {};
    meshes.FrameCamera = true;
  }
  if (Edits != edits) {
    Edits = edits;
    meshes.Park = true;
    // An edit applied while paused changes the world without a tick, and may move guests.
    meshes.Guests = true;
    Kept.Tick.reset();
  }
  if (meshes.Park) {
    buildParkMesh(world, ParkLists);
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

namespace {

void clearMesh(ParkMesh &mesh) {
  mesh.Vertices.clear();
  mesh.Indices.clear();
}

// The look's marks: where a path tool's first click would start the path, its paving in Lit and
// its join mark in Marks, then where a click would finish it, the highlight, the point markers,
// Edit path's radius and edge handles, the handle or sides under the pointer, and an inspected
// shop, bench, or toilet block's mark.
// The boxes, path points, and segments the look's marks read, as parkBoxes, parkPathPoints, and
// parkSegments give them.
struct LookPark {
  std::span<const ParkBox> Boxes;
  std::span<const ParkPathPoint> Points;
  std::span<const ParkSegment> Segments;
};

void buildLookMarks(GhostMesh &marks, const World &world, const LookPark &park,
                    const PreviewLook &look) {
  clearMesh(marks.Lit);
  clearMesh(marks.Marks);
  if (look.Start) {
    appendPathStart(marks.Lit, look.Start->Kind, look.Start->At, false);
    if (look.Start->Joins) {
      appendJoinMark(marks.Marks, look.Start->At);
    }
  }
  if (look.Finish) {
    appendFinishMark(marks.Marks, *look.Finish);
  }
  if (look.Highlight) {
    appendEntity(marks.Marks, world, park.Boxes, park.Points, *look.Highlight, HIGHLIGHT_TINT);
  }
  for (const EditHandle &handle : look.Handles) {
    if (handle.Kind == HandleKind::Point) {
      appendPointMark(marks.Marks, handle.At, POINT_MARK_COLOR);
    } else if (handle.Kind == HandleKind::Radius) {
      appendHandle(marks.Marks, handle.At, HANDLE_COLOR);
    } else if (handle.Kind == HandleKind::Taper) {
      appendHandle(marks.Marks, handle.At, TAPER_HANDLE_COLOR);
    } else {
      appendHandle(marks.Marks, handle.At, EDGE_HANDLE_COLOR);
    }
  }
  if (look.Hovered && look.Hovered->Kind != HandleKind::Side) {
    appendHandle(marks.Marks, look.Hovered->At, HIGHLIGHT_TINT);
  } else if (look.Hovered) {
    appendSides(marks.Marks, park.Segments, look.Hovered->Key, HIGHLIGHT_TINT);
  }
  if (look.Inspected && look.Inspected->Kind != SubjectKind::Guest) {
    appendEntity(marks.Marks, world, park.Boxes, park.Points, look.Inspected->Key, HIGHLIGHT_TINT);
  }
}

} // namespace

void fillLook(Interaction &interaction, const World &world, std::optional<GuestNeed> overlay,
              PreviewLook &look) {
  look.Highlight = interaction.highlighted(world);
  look.Inspected = interaction.subject();
  look.Overlay = overlay;
  look.Start = interaction.pathStart(world);
  look.Finish = interaction.pathFinish(world);
  interaction.editHandles(look.Handles);
  look.Hovered = interaction.hoveredHandle();
}

PreviewMeshes SceneSync::syncLook(const World &world, const ToolPark &park, bool remade,
                                  const PreviewLook &look, bool shapeChanged) {
  PreviewMeshes meshes;
  const bool first = !Look.has_value();
  const bool editBuilt = remade || first;
  if (editBuilt) {
    const Preview &preview = Kept.Made;
    if (preview.Edit) {
      EditGhost = buildGhostMesh(world, *preview.Edit, preview.Candidate);
    } else {
      clearMesh(EditGhost.Tints);
      clearMesh(EditGhost.Lit);
      clearMesh(EditGhost.Marks);
    }
  }
  const bool lookBuilt = first || *Look != look || LookEdits != Edits;
  if (lookBuilt) {
    buildLookMarks(LookMarks, world, {park.Boxes, park.Points, park.Segments}, look);
    LookEdits = Edits;
  }
  const bool guest = look.Inspected && look.Inspected->Kind == SubjectKind::Guest;
  if (editBuilt || lookBuilt || (GuestsRebuilt && guest)) {
    clearMesh(SubjectMark);
    if (guest) {
      appendGuestEntity(SubjectMark, world, Seated, look.Inspected->Key, HIGHLIGHT_TINT);
    }
    // Assigning reuses the ghost's memory.
    Ghost.Tints = EditGhost.Tints;
    Ghost.Lit = EditGhost.Lit;
    appendMesh(Ghost.Lit, LookMarks.Lit);
    Ghost.Marks = EditGhost.Marks;
    appendMesh(Ghost.Marks, LookMarks.Marks);
    appendMesh(Ghost.Marks, SubjectMark);
    meshes.Ghost = true;
  }
  if (lookBuilt) {
    Look = look;
  }
  if (look.Overlay) {
    const World &previewed = previewedWorld(world, Kept.Made);
    if (remade || shapeChanged || !Shaded || Shaded->Tick != previewed.Tick ||
        Shaded->Need != *look.Overlay) {
      availabilityShades(previewed, ShapeLists.Shape, needAvailability(*look.Overlay), ShadeLists);
      Shaded = ShadedFor{previewed.Tick, *look.Overlay};
      meshes.Shades = true;
    }
  } else {
    Shaded.reset();
  }
  return meshes;
}

bool SceneSync::syncOverlayShape(const World &previewed, bool shown) {
  if (!shown) {
    Shaped.reset();
    OverlayShape &shape = ShapeLists.Shape;
    if (shape.Pieces.empty()) {
      return false;
    }
    // Emptied in place, so the lists keep their memory for the next time it shows.
    shape.Vertices.clear();
    shape.Indices.clear();
    shape.Pieces.clear();
    shape.LineRefs.clear();
    shape.Lines.clear();
    shape.Points.clear();
    shape.Overlaps.clear();
    return true;
  }
  const Preview &preview = Kept.Made;
  const bool pathEdit = preview.Candidate && preview.Edit && changesPaths(*preview.Edit);
  // Compared in place, so a frame copies no edit.
  if (Shaped && Shaped->Edits == Edits &&
      (pathEdit ? Shaped->Edit == preview.Edit : !Shaped->Edit.has_value())) {
    return false;
  }
  if (!Shaped) {
    Shaped.emplace();
  }
  Shaped->Edits = Edits;
  if (pathEdit) {
    Shaped->Edit = preview.Edit;
  } else {
    Shaped->Edit.reset();
  }
  buildOverlayShape(previewed, ShapeLists);
  return true;
}

} // namespace tpj
