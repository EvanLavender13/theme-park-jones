#ifndef TPJ_RENDER_PICKING_H
#define TPJ_RENDER_PICKING_H

#include "render/math.h"
#include "render/renderer.h"
#include "sim/entity_key.h"
#include "sim/park/intent.h"
#include "sim/world.h"

#include <optional>

namespace tpj {

// A ray from the eye through a cursor: the points Origin + Direction * t for t at least 0.
struct CursorRay {
  Vec3 Origin;
  Vec3 Direction;
};

// The ray from the eye through a cursor at normalized device coordinates, x from -1 at the left to
// 1 at the right and y from -1 at the bottom to 1 at the top.
CursorRay cursorRay(const CameraView &view, float aspect, float ndcX, float ndcY);

// The point on the ground under a cursor at normalized device coordinates, where its ray meets the
// ground. None when the eye is not above the ground or the ray does not descend.
std::optional<ParkPoint> groundAtCursor(const CameraView &view, float aspect, float ndcX,
                                        float ndcY);

// Where the ray first meets the upright box appendBox draws for the pose, size, and height: the
// least ray parameter at least 0 inside it, edges included. None when the ray misses it or the
// pose has no footprint.
std::optional<double> rayEntry(const CursorRay &ray, const Pose &pose, FootprintSize size,
                               float height);

// The entrance, box, or guest the cursor's ray first meets, as the park and guest meshes draw
// them, or none when it meets none.
std::optional<EntityKey> entityAtCursor(const World &world, const CameraView &view, float aspect,
                                        float ndcX, float ndcY);

} // namespace tpj

#endif
