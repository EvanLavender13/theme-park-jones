#ifndef TPJ_APP_INPUT_INTERACTION_H
#define TPJ_APP_INPUT_INTERACTION_H

#include "app/input/input_map.h"
#include "legible/inspect.h"
#include "sim/command_queue.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/world.h"
#include "tools/tools.h"

#include <optional>
#include <stdint.h>
#include <vector>

namespace tpj {

// The player's hold on the park: the tool, and the guest or shop the Inspector shows. It follows
// the park session's generation, so a replaced world drops the tool's hold and the subject.
class Interaction {
public:
  // Starts with the tool at start and no subject, having seen the generation.
  explicit Interaction(uint64_t generation) : Generation(generation) {}

  [[nodiscard]] const ToolState &tool() const { return Tool; }
  [[nodiscard]] const std::optional<InspectorSubject> &subject() const { return Subject; }

  // For a generation other than the last one it saw, selects the current tool again, so it drops
  // any hold and drawn points, and forgets the subject.
  void follow(uint64_t generation);
  // Gives the tool the frame's press, then its release, queueing the edit a release commits.
  void useButtons(const World &world, CommandQueue &commands, const PointerButtons &buttons);
  // True when the frame's press picks a subject: a press with the Look tool.
  [[nodiscard]] bool picks(const PointerButtons &buttons) const;
  // Sets the subject from the entity under the cursor, as pickSubject does.
  void pick(const World &world, std::optional<EntityKey> entity);
  void forgetSubject() { Subject.reset(); }
  void selectTool(ToolKind kind);
  void movePointer(std::optional<ParkPoint> ground);
  [[nodiscard]] std::optional<ParkEdit> tentativeEdit(const World &world) const;
  [[nodiscard]] std::optional<EntityKey> highlighted(const World &world) const;
  // Where the tool's first click would start a path, as pathStart gives it.
  [[nodiscard]] std::optional<PathStart> pathStart(const World &world) const;
  // Where a click would finish the drawn path, as pathFinish gives it.
  [[nodiscard]] std::optional<ParkPoint> pathFinish(const World &world) const;
  // Edit path's radius and edge handles, as editHandles gives them.
  [[nodiscard]] std::vector<EditHandle> editHandles(const World &world) const;
  // The handle under the pointer, as hoveredHandle gives it.
  [[nodiscard]] std::optional<EditHandle> hoveredHandle(const World &world) const;

private:
  uint64_t Generation;
  ToolState Tool;
  std::optional<InspectorSubject> Subject;
};

} // namespace tpj

#endif
