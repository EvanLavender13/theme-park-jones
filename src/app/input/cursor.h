#ifndef TPJ_APP_INPUT_CURSOR_H
#define TPJ_APP_INPUT_CURSOR_H

#include "render/renderer.h"
#include "sim/entity_key.h"
#include "sim/park/intent.h"
#include "sim/world.h"

#include <optional>

namespace tpj {

// Where the cursor lies in normalized device coordinates, X from -1 at the window's left to 1 at
// its right and Y from -1 at its bottom to 1 at its top, and the window's width over its height.
struct CursorNdc {
  float X = 0.0f;
  float Y = 0.0f;
  float Aspect = 1.0f;

  bool operator==(const CursorNdc &) const = default;
};

// The cursor at (x, y) pixels from the top left of a window width by height pixels, or none when
// the width or the height is not positive.
std::optional<CursorNdc> cursorNdc(int width, int height, float x, float y);
// The ground under the cursor, as groundAtCursor gives it for the view, or none without a cursor.
std::optional<ParkPoint> groundUnderCursor(const std::optional<CursorNdc> &cursor,
                                           const CameraView &view);
// The entity the cursor's ray first meets, as entityAtCursor gives it for the world and the view,
// or none without a cursor.
std::optional<EntityKey> entityUnderCursor(const World &world,
                                           const std::optional<CursorNdc> &cursor,
                                           const CameraView &view);

} // namespace tpj

#endif
