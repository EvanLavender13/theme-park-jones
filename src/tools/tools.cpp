#include "tools/tools.h"

#include "sim/park/geometry.h"

#include <algorithm>
#include <math.h>
#include <vector>

namespace tpj {
namespace {

// The distance from a point to the segment between two distinct ground line points.
double segmentDistance(ParkPoint point, const CarrierPoint &from, const CarrierPoint &to) {
  const double dx = to.X - from.X;
  const double dz = to.Z - from.Z;
  const double px = point.X - from.X;
  const double pz = point.Z - from.Z;
  const double t = std::clamp((px * dx + pz * dz) / (dx * dx + dz * dz), 0.0, 1.0);
  const double ex = px - dx * t;
  const double ez = pz - dz * t;
  return sqrt(ex * ex + ez * ez);
}

bool isPlaceTool(ToolKind kind) {
  return kind == ToolKind::PlaceShop || kind == ToolKind::PlaceDepot;
}

BoxKind placedKind(ToolKind kind) {
  return kind == ToolKind::PlaceShop ? BoxKind::Shop : BoxKind::Depot;
}

// The pose the box the key holds has, if it holds one.
std::optional<Pose> boxPose(const World &world, EntityKey key) {
  for (const ParkBox &box : parkBoxes(world)) {
    if (box.Key == key) {
      return box.At;
    }
  }
  return std::nullopt;
}

} // namespace

void selectTool(ToolState &tool, ToolKind kind) {
  tool.Kind = kind;
  tool.Holding = false;
  tool.Held = NULL_KEY;
}

void movePointer(ToolState &tool, std::optional<ParkPoint> ground) {
  tool.Pointer = ground;
  if (!tool.Holding || !ground) {
    return;
  }
  if (isPlaceTool(tool.Kind)) {
    const double dx = ground->X - tool.Landing.X;
    const double dz = ground->Z - tool.Landing.Z;
    if (dx * dx + dz * dz >= MIN_FACING_DRAG * MIN_FACING_DRAG) {
      tool.Landing.FacingX = dx;
      tool.Landing.FacingZ = dz;
    }
  } else if (tool.Kind == ToolKind::MoveBox) {
    tool.Target.X = ground->X + tool.GrabOffset.X;
    tool.Target.Z = ground->Z + tool.GrabOffset.Z;
  }
}

void pressPointer(ToolState &tool, const World &world) {
  if (tool.Holding) {
    return;
  }
  if (tool.Kind == ToolKind::Delete) {
    tool.Holding = true;
  } else if (isPlaceTool(tool.Kind) && tool.Pointer) {
    tool.Holding = true;
    tool.Landing = Pose{tool.Pointer->X, tool.Pointer->Z, tool.FacingX, tool.FacingZ};
  } else if (tool.Kind == ToolKind::MoveBox && tool.Pointer) {
    const std::optional<EntityKey> key = boxAt(world, *tool.Pointer);
    const std::optional<Pose> pose = key ? boxPose(world, *key) : std::nullopt;
    if (pose) {
      tool.Holding = true;
      tool.Held = *key;
      tool.HeldFrom = *pose;
      tool.Target = *pose;
      tool.GrabOffset = ParkPoint{pose->X - tool.Pointer->X, pose->Z - tool.Pointer->Z};
    }
  }
}

std::optional<ParkEdit> releasePointer(ToolState &tool, const World &world) {
  if (!tool.Holding) {
    return std::nullopt;
  }
  std::optional<ParkEdit> edit = tentativeEdit(tool, world);
  if (isPlaceTool(tool.Kind)) {
    tool.FacingX = tool.Landing.FacingX;
    tool.FacingZ = tool.Landing.FacingZ;
  }
  tool.Holding = false;
  tool.Held = NULL_KEY;
  return edit;
}

std::optional<ParkEdit> tentativeEdit(const ToolState &tool, const World &world) {
  if (isPlaceTool(tool.Kind)) {
    if (tool.Holding) {
      return AddBox{placedKind(tool.Kind), tool.Landing};
    }
    if (tool.Pointer) {
      return AddBox{placedKind(tool.Kind),
                    Pose{tool.Pointer->X, tool.Pointer->Z, tool.FacingX, tool.FacingZ}};
    }
    return std::nullopt;
  }
  if (tool.Kind == ToolKind::MoveBox) {
    if (tool.Holding && !(tool.Target == tool.HeldFrom)) {
      return MoveBox{tool.Held, tool.Target};
    }
    return std::nullopt;
  }
  if (tool.Kind == ToolKind::Delete && tool.Pointer) {
    if (const std::optional<EntityKey> box = boxAt(world, *tool.Pointer)) {
      return DeleteBox{*box};
    }
    if (const std::optional<EntityKey> path = pathAt(world, *tool.Pointer)) {
      return DeletePath{*path};
    }
  }
  return std::nullopt;
}

std::optional<EntityKey> highlightedEntity(const ToolState &tool, const World &world) {
  if (tool.Kind == ToolKind::MoveBox && !tool.Holding && tool.Pointer) {
    return boxAt(world, *tool.Pointer);
  }
  return std::nullopt;
}

std::optional<EntityKey> boxAt(const World &world, ParkPoint point) {
  for (const ParkBox &box : parkBoxes(world)) {
    const FootprintSize size = boxSize(box.Kind);
    const std::optional<Footprint> footprint = footprintOf(box.At, size);
    if (!footprint) {
      continue;
    }
    const double dx = point.X - box.At.X;
    const double dz = point.Z - box.At.Z;
    const double along = dx * footprint->Forward.X + dz * footprint->Forward.Z;
    const double across = dx * footprint->Right.X + dz * footprint->Right.Z;
    if (fabs(along) <= 0.5 * size.Depth && fabs(across) <= 0.5 * size.Width) {
      return box.Key;
    }
  }
  return std::nullopt;
}

std::optional<EntityKey> pathAt(const World &world, ParkPoint point) {
  for (const ParkPath &path : parkPaths(world)) {
    const std::vector<CarrierPoint> line = groundLine(path.Points);
    const double half = 0.5 * pathWidth(path.Kind);
    for (size_t i = 0; i + 1 < line.size(); ++i) {
      if (segmentDistance(point, line[i], line[i + 1]) <= half) {
        return path.Key;
      }
    }
  }
  return std::nullopt;
}

} // namespace tpj
