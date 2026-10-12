#include "app/input/cursor.h"
#include "render/picking.h"

#include <math.h>

namespace tpj {

std::optional<CursorNdc> cursorNdc(int width, int height, float x, float y) {
  if (width <= 0 || height <= 0) {
    return std::nullopt;
  }
  return CursorNdc{2.0f * x / static_cast<float>(width) - 1.0f,
                   1.0f - 2.0f * y / static_cast<float>(height),
                   static_cast<float>(width) / static_cast<float>(height),
                   2.0f / static_cast<float>(width), 2.0f / static_cast<float>(height)};
}

std::optional<ParkPoint> groundUnderCursor(const std::optional<CursorNdc> &cursor,
                                           const CameraView &view) {
  if (!cursor) {
    return std::nullopt;
  }
  return groundAtCursor(view, cursor->Aspect, cursor->X, cursor->Y);
}

std::optional<ParkPoint> pointUnderCursor(const std::optional<CursorNdc> &cursor,
                                          const CameraView &view, float height) {
  if (!cursor) {
    return std::nullopt;
  }
  return pointAtCursor(view, cursor->Aspect, cursor->X, cursor->Y, height);
}

std::optional<double> groundPerPixel(const std::optional<CursorNdc> &cursor,
                                     const CameraView &view) {
  if (!cursor) {
    return std::nullopt;
  }
  const std::optional<ParkPoint> under = groundAtCursor(view, cursor->Aspect, cursor->X, cursor->Y);
  if (!under) {
    return std::nullopt;
  }
  double total = 0.0;
  int count = 0;
  for (const std::optional<ParkPoint> &beside :
       {groundAtCursor(view, cursor->Aspect, cursor->X + cursor->PixelX, cursor->Y),
        groundAtCursor(view, cursor->Aspect, cursor->X, cursor->Y + cursor->PixelY)}) {
    if (beside) {
      const double dx = beside->X - under->X;
      const double dz = beside->Z - under->Z;
      total += sqrt(dx * dx + dz * dz);
      ++count;
    }
  }
  if (count == 0) {
    return std::nullopt;
  }
  return total / count;
}

std::optional<EntityKey> entityUnderCursor(const World &world, const SeatedGuests &seated,
                                           const std::optional<CursorNdc> &cursor,
                                           const CameraView &view) {
  if (!cursor) {
    return std::nullopt;
  }
  return entityAtCursor(world, seated, view, cursor->Aspect, cursor->X, cursor->Y);
}

} // namespace tpj
