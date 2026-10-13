#include "app/input/interaction.h"

namespace tpj {
namespace {

// The stock of the shop box the key holds, or none when it holds no shop.
std::optional<ShopStock> shopStockOf(const ToolPark &park, EntityKey key) {
  for (const ParkBox &box : park.Boxes) {
    if (box.Key == key && box.Kind == BoxKind::Shop) {
      return box.Stock;
    }
  }
  return std::nullopt;
}

} // namespace

Interaction::Interaction(const World &world, uint64_t generation, uint64_t edits)
    : Generation(generation), Edits(edits) {
  keepToolPark(world, Park);
}

void Interaction::follow(const World &world, uint64_t generation, uint64_t edits) {
  if (generation == Generation && edits == Edits) {
    return;
  }
  if (generation != Generation) {
    tpj::selectTool(Tool, Tool.Kind);
    Subject.reset();
    StockChoice.reset();
    HoursChoice.reset();
  }
  Generation = generation;
  Edits = edits;
  keepToolPark(world, Park);
}

void Interaction::useButtons(const World &world, CommandQueue &commands,
                             const PointerButtons &buttons) {
  if (buttons.Pressed) {
    pressPointer(Tool, Park, world);
  }
  if (buttons.Released) {
    if (const std::optional<ParkEdit> edit = releasePointer(Tool, Park, world)) {
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

void Interaction::settleStock() {
  if (Subject && StockChoice && shopStockOf(Park, Subject->Key) == StockChoice) {
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
  tpj::movePointer(Tool, Park, world, pointer);
}

const std::optional<ParkEdit> &Interaction::tentativeEdit(const World &world) {
  tpj::tentativeEdit(Tool, Park, world, Edit);
  if (Edit) {
    return Edit;
  }
  if (Subject && Subject->Kind == SubjectKind::Shop && StockChoice) {
    const std::optional<ShopStock> stock = shopStockOf(Park, Subject->Key);
    if (stock && *stock != *StockChoice) {
      Edit = SetShopStock{Subject->Key, *StockChoice};
      return Edit;
    }
  }
  if (HoursChoice && *HoursChoice != parkHours(world)) {
    Edit = SetOpeningHours{*HoursChoice};
  }
  return Edit;
}

std::optional<EntityKey> Interaction::highlighted(const World &world) const {
  return highlightedEntity(Tool, Park, world);
}

std::optional<PathStart> Interaction::pathStart(const World &world) {
  return tpj::pathStart(Tool, Park, world);
}

std::optional<ParkPoint> Interaction::pathFinish(const World &world) {
  return tpj::pathFinish(Tool, Park, world);
}

void Interaction::editHandles(std::vector<EditHandle> &handles) const {
  tpj::editHandles(Tool, Park, handles);
}

std::optional<EditHandle> Interaction::hoveredHandle() const {
  return tpj::hoveredHandle(Tool, Park);
}

std::span<const SnapGuide> Interaction::snapGuides() const { return tpj::snapGuides(Tool); }

std::span<const MeasuredLength> Interaction::measuredLengths(const World &world) {
  tpj::measuredLengths(Tool, Park, world, Lengths);
  return Lengths;
}

} // namespace tpj
