#include "app/input/interaction.h"

namespace tpj {

void Interaction::follow(uint64_t generation) {
  if (generation == Generation) {
    return;
  }
  Generation = generation;
  tpj::selectTool(Tool, Tool.Kind);
  Subject.reset();
}

void Interaction::useButtons(const World &world, CommandQueue &commands,
                             const PointerButtons &buttons) {
  if (buttons.Pressed) {
    pressPointer(Tool, world);
  }
  if (buttons.Released) {
    if (const std::optional<ParkEdit> edit = releasePointer(Tool, world)) {
      queueEdit(commands, *edit);
    }
  }
}

bool Interaction::picks(const PointerButtons &buttons) const {
  return buttons.Pressed && Tool.Kind == ToolKind::None;
}

void Interaction::pick(const World &world, std::optional<EntityKey> entity) {
  pickSubject(Subject, world, entity);
}

void Interaction::selectTool(ToolKind kind) { tpj::selectTool(Tool, kind); }

void Interaction::movePointer(std::optional<ParkPoint> ground) { tpj::movePointer(Tool, ground); }

std::optional<ParkEdit> Interaction::tentativeEdit(const World &world) const {
  return tpj::tentativeEdit(Tool, world);
}

std::optional<EntityKey> Interaction::highlighted(const World &world) const {
  return highlightedEntity(Tool, world);
}

} // namespace tpj
