#ifndef TPJ_RENDER_PICKING_H
#define TPJ_RENDER_PICKING_H

#include "render/renderer.h"
#include "sim/park/intent.h"

#include <optional>

namespace tpj {

// The point on the ground under a cursor at normalized device coordinates, x from -1 at the left
// to 1 at the right and y from -1 at the bottom to 1 at the top. None when the eye is not above the
// ground or the ray through the cursor does not descend.
std::optional<ParkPoint> groundAtCursor(const CameraView &view, float aspect, float ndcX,
                                        float ndcY);

} // namespace tpj

#endif
