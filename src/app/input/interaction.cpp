#include "app/input/interaction.h"

namespace tpj {
namespace {

// The stock of the shop box the key holds, or none when it holds no shop.
std::optional<ShopStock> shopStockOf(const World &world, EntityKey key) {
  for (const ParkBox &box : parkBoxes(world)) {
    if (box.Key == key && box.Kind == BoxKind::Shop) {
      return box.Stock;
    }
  }
  return std::nullopt;
}

} // namespace

void Interaction::follow(uint64_t generation) {
  if (generation == Generation) {
    return;
  }
  Generation = generation;
  tpj::selectTool(Tool, Tool.Kind);
  Subject.reset();
  StockChoice.reset();
  HoursChoice.reset();
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
  const std::optional<InspectorSubject> was = Subject;
  pickSubject(Subject, world, entity);
  if (Subject != was) {
    StockChoice.reset();
  }
}

void Interaction::forgetSubject() {
  Subject.reset();
  StockChoice.reset();
}

void Interaction::applyStock(CommandQueue &commands) const {
  if (Subject && Subject->Kind == SubjectKind::Shop && StockChoice) {
    queueEdit(commands, SetShopStock{Subject->Key, *StockChoice});
  }
}

void Interaction::settleStock(const World &world) {
  if (Subject && StockChoice && shopStockOf(world, Subject->Key) == StockChoice) {
    StockChoice.reset();
  }
}

void Interaction::applyHours(CommandQueue &commands) const {
  if (HoursChoice) {
    queueEdit(commands, SetOpeningHours{*HoursChoice});
  }
}

void Interaction::settleHours(const World &world) {
  if (HoursChoice == parkHours(world)) {
    HoursChoice.reset();
  }
}

void Interaction::selectTool(ToolKind kind) { tpj::selectTool(Tool, kind); }

void Interaction::chooseFurnish(const FurnishChoice &choice) { setFurnishChoice(Tool, choice); }

bool Interaction::dropHeld() {
  if (!Tool.Holding && Tool.Drawn.empty()) {
    return false;
  }
  tpj::selectTool(Tool, Tool.Kind);
  return true;
}

void Interaction::movePointer(const World &world, const PointerSample &pointer) {
  tpj::movePointer(Tool, world, pointer);
}

std::optional<ParkEdit> Interaction::tentativeEdit(const World &world) const {
  if (std::optional<ParkEdit> edit = tpj::tentativeEdit(Tool, world)) {
    return edit;
  }
  if (Subject && Subject->Kind == SubjectKind::Shop && StockChoice) {
    const std::optional<ShopStock> stock = shopStockOf(world, Subject->Key);
    if (stock && *stock != *StockChoice) {
      return SetShopStock{Subject->Key, *StockChoice};
    }
  }
  if (HoursChoice && *HoursChoice != parkHours(world)) {
    return SetOpeningHours{*HoursChoice};
  }
  return std::nullopt;
}

std::optional<EntityKey> Interaction::highlighted(const World &world) const {
  return highlightedEntity(Tool, world);
}

std::optional<PathStart> Interaction::pathStart(const World &world) const {
  return tpj::pathStart(Tool, world);
}

std::optional<ParkPoint> Interaction::pathFinish(const World &world) const {
  return tpj::pathFinish(Tool, world);
}

std::vector<EditHandle> Interaction::editHandles(const World &world) const {
  return tpj::editHandles(Tool, world);
}

std::optional<EditHandle> Interaction::hoveredHandle(const World &world) const {
  return tpj::hoveredHandle(Tool, world);
}

std::vector<SnapGuide> Interaction::snapGuides() const { return tpj::snapGuides(Tool); }

std::vector<MeasuredLength> Interaction::measuredLengths(const World &world) const {
  return tpj::measuredLengths(Tool, world);
}

} // namespace tpj
