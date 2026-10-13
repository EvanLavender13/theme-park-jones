#ifndef TPJ_APP_INPUT_INTERACTION_H
#define TPJ_APP_INPUT_INTERACTION_H

#include "app/input/cursor.h"
#include "app/input/input_map.h"
#include "legible/inspect.h"
#include "sim/command_queue.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/world.h"
#include "tools/tools.h"

#include <optional>
#include <span>
#include <stdint.h>
#include <vector>

namespace tpj {

// The player's hold on the park: the tool, the guest or shop the Inspector shows, the stock chosen
// for that shop, and the hours chosen in the Hours panel, both not yet applied. It follows the park
// session's generation, so a replaced world drops the tool's hold, the subject, and both choices.
// It keeps the park as the tools read it, filled again when the generation or the edit count
// changes, and the frame's tentative edit and measured lengths, so a frame's tool work allocates
// nothing once their lists have grown.
class Interaction {
public:
  // Starts with the tool at start and no subject, having seen the generation and the edit count,
  // with its park filled from the world.
  Interaction(const World &world, uint64_t generation, uint64_t edits);

  [[nodiscard]] const ToolState &tool() const { return Tool; }
  // The park as the tools read it, filled again whenever the generation or the edit count changes.
  [[nodiscard]] const ToolPark &park() const { return Park; }
  [[nodiscard]] const std::optional<InspectorSubject> &subject() const { return Subject; }

  // For a generation other than the last one it saw, selects the current tool again, so it drops
  // any hold and drawn points, and forgets the subject. For a generation or an edit count other
  // than the last it saw, fills its park again from the world.
  void follow(const World &world, uint64_t generation, uint64_t edits);
  // Gives the tool the frame's press, then its release, queueing the edit a release commits.
  void useButtons(const World &world, CommandQueue &commands, const PointerButtons &buttons);
  // True when the frame's press picks a subject: a press with the Look tool.
  [[nodiscard]] bool picks(const PointerButtons &buttons) const;
  // Sets the subject from the entity under the cursor, as entityUnderCursor finds it and
  // pickSubject keeps it, with lists the interaction keeps.
  void pick(const World &world, const SeatedGuests &seated, const std::optional<CursorNdc> &cursor,
            const CameraView &view);
  void forgetSubject();
  // The stock chosen in the Inspector and not yet applied.
  [[nodiscard]] const std::optional<ShopStock> &stockChoice() const { return StockChoice; }
  // Chooses the stock for the Inspector's shop. It is applied only by applyStock.
  void chooseStock(ShopStock stock) { StockChoice = stock; }
  // Queues a SetShopStock of the subject to the chosen stock, when the subject is a shop and a
  // stock is chosen. The choice stays until settleStock finds the shop has it.
  void applyStock(CommandQueue &commands) const;
  // Drops the chosen stock once the Inspector's shop has it, so the choice stays marked until its
  // SetShopStock applies.
  void settleStock();
  // The hours chosen in the Hours panel and not yet applied.
  [[nodiscard]] const std::optional<OpeningHours> &hoursChoice() const { return HoursChoice; }
  // Chooses the park's hours. They are applied only by applyHours.
  void chooseHours(OpeningHours hours) { HoursChoice = hours; }
  // Queues a SetOpeningHours of the chosen hours, when hours are chosen. The choice stays until
  // settleHours finds the park has them.
  void applyHours(CommandQueue &commands) const;
  // Drops the chosen hours once the park has them, so the choice stays shown until its
  // SetOpeningHours applies.
  void settleHours(const World &world);
  void selectTool(ToolKind kind);
  // Sets what the Furnish tool lays.
  void chooseFurnish(const FurnishChoice &choice);
  // Drops what the tool holds or has drawn, as selecting it again does, and gives whether there was
  // anything: a held press or a drawn stop.
  bool dropHeld();
  void movePointer(const World &world, const PointerSample &pointer);
  // The tool's edit, or else the Inspector's chosen stock as a SetShopStock while it differs from
  // the shop's, or else the Hours panel's chosen hours as a SetOpeningHours while they differ from
  // the park's. Kept until the next call, which refills it in place.
  [[nodiscard]] const std::optional<ParkEdit> &tentativeEdit(const World &world);
  [[nodiscard]] std::optional<EntityKey> highlighted(const World &world) const;
  // Where the tool's first click would start a path, as pathStart gives it.
  [[nodiscard]] std::optional<PathStart> pathStart(const World &world);
  // Where a click would finish the drawn path, as pathFinish gives it.
  [[nodiscard]] std::optional<ParkPoint> pathFinish(const World &world);
  // Edit path's handles, as editHandles gives them, in a list the caller keeps.
  void editHandles(std::vector<EditHandle> &handles) const;
  // The handle under the pointer, as hoveredHandle gives it.
  [[nodiscard]] std::optional<EditHandle> hoveredHandle() const;
  // The guides of the snap in effect, as snapGuides gives them.
  [[nodiscard]] std::span<const SnapGuide> snapGuides() const;
  // The lengths the ghost shows, as measuredLengths gives them, kept until the next call.
  [[nodiscard]] std::span<const MeasuredLength> measuredLengths(const World &world);

private:
  uint64_t Generation;
  uint64_t Edits;
  ToolState Tool;
  ToolPark Park;
  KeptEdit Edit;
  KeptEdit Released;
  PickLists Picks;
  TickBuffers PickBuffers;
  std::vector<MeasuredLength> Lengths;
  std::optional<InspectorSubject> Subject;
  std::optional<ShopStock> StockChoice;
  std::optional<OpeningHours> HoursChoice;
};

} // namespace tpj

#endif
