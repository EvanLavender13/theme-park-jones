#include "app/input/cursor.h"
#include "render/picking.h"

namespace tpj {

std::optional<CursorNdc> cursorNdc(int width, int height, float x, float y) {
  if (width <= 0 || height <= 0) {
    return std::nullopt;
  }
  return CursorNdc{2.0f * x / static_cast<float>(width) - 1.0f,
                   1.0f - 2.0f * y / static_cast<float>(height),
                   static_cast<float>(width) / static_cast<float>(height)};
}

std::optional<ParkPoint> groundUnderCursor(const std::optional<CursorNdc> &cursor,
                                           const CameraView &view) {
  if (!cursor) {
    return std::nullopt;
  }
  return groundAtCursor(view, cursor->Aspect, cursor->X, cursor->Y);
}

std::optional<EntityKey> entityUnderCursor(const World &world,
                                           const std::optional<CursorNdc> &cursor,
                                           const CameraView &view) {
  if (!cursor) {
    return std::nullopt;
  }
  return entityAtCursor(world, view, cursor->Aspect, cursor->X, cursor->Y);
}

} // namespace tpj
