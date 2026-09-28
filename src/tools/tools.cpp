#include "tools/tools.h"

#include "sim/park/geometry.h"

#include <algorithm>
#include <math.h>
#include <utility>
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

// The nearest point to a point on the segment between two distinct ground line points.
ParkPoint nearestOnSegment(ParkPoint point, const CarrierPoint &from, const CarrierPoint &to) {
  const double dx = to.X - from.X;
  const double dz = to.Z - from.Z;
  const double t = std::clamp(
      ((point.X - from.X) * dx + (point.Z - from.Z) * dz) / (dx * dx + dz * dz), 0.0, 1.0);
  return ParkPoint{from.X + dx * t, from.Z + dz * t};
}

bool isPlaceTool(ToolKind kind) {
  return kind == ToolKind::PlaceShop || kind == ToolKind::PlaceDepot;
}

BoxKind placedKind(ToolKind kind) {
  return kind == ToolKind::PlaceShop ? BoxKind::Shop : BoxKind::Depot;
}

bool isPathTool(ToolKind kind) {
  return kind == ToolKind::GuestPath || kind == ToolKind::BackstagePath;
}

PathKind drawnKind(ToolKind kind) {
  return kind == ToolKind::GuestPath ? PathKind::Guest : PathKind::Backstage;
}

// The point a path tool's press would append: the snapped pointer, unless it lies within
// FINISH_REACH of the last drawn point. See tools/SPEC.md.
std::optional<ParkPoint> nextPoint(const ToolState &tool, const World &world) {
  if (!tool.Pointer) {
    return std::nullopt;
  }
  const ParkPoint point = snapToPath(world, drawnKind(tool.Kind), *tool.Pointer);
  if (!tool.Drawn.empty()) {
    const double dx = point.X - tool.Drawn.back().X;
    const double dz = point.Z - tool.Drawn.back().Z;
    if (dx * dx + dz * dz <= FINISH_REACH * FINISH_REACH) {
      return std::nullopt;
    }
  }
  return point;
}

// A path tool's edit: the drawn points, followed by the next point while not holding.
std::optional<ParkEdit> pathEdit(const ToolState &tool, const World &world) {
  std::vector<ParkPoint> points = tool.Drawn;
  if (!tool.Holding && !points.empty()) {
    if (const std::optional<ParkPoint> next = nextPoint(tool, world)) {
      points.push_back(*next);
    }
  }
  if (points.size() < 2) {
    return std::nullopt;
  }
  return AddPath{drawnKind(tool.Kind), std::move(points)};
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
  tool.Drawn.clear();
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
  } else if (isPathTool(tool.Kind) && tool.Pointer) {
    if (const std::optional<ParkPoint> next = nextPoint(tool, world)) {
      tool.Drawn.push_back(*next);
    } else {
      tool.Holding = true;
    }
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
  if (isPathTool(tool.Kind)) {
    tool.Drawn.clear();
  }
  tool.Holding = false;
  tool.Held = NULL_KEY;
  return edit;
}

std::optional<ParkEdit> tentativeEdit(const ToolState &tool, const World &world) {
  if (isPathTool(tool.Kind)) {
    return pathEdit(tool, world);
  }
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

ParkPoint snapToPath(const World &world, PathKind kind, ParkPoint point) {
  ParkPoint snapped = point;
  std::optional<double> nearest;
  for (const ParkPath &path : parkPaths(world)) {
    if (path.Kind != kind) {
      continue;
    }
    const std::vector<CarrierPoint> line = groundLine(path.Points);
    for (size_t i = 0; i + 1 < line.size(); ++i) {
      const ParkPoint candidate = nearestOnSegment(point, line[i], line[i + 1]);
      const double dx = candidate.X - point.X;
      const double dz = candidate.Z - point.Z;
      const double distance = sqrt(dx * dx + dz * dz);
      if (distance <= SNAP_REACH && (!nearest || distance < *nearest)) {
        snapped = candidate;
        nearest = distance;
      }
    }
  }
  return snapped;
}

} // namespace tpj
