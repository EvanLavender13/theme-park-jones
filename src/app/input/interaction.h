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

// The player's hold on the park: the tool, the guest or shop the Inspector shows, the stock chosen
// for that shop, and the hours chosen in the Hours panel, both not yet applied. It follows the park
// session's generation, so a replaced world drops the tool's hold, the subject, and both choices.
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
  void settleStock(const World &world);
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
  // the park's.
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
  // The guides of the snap in effect, as snapGuides gives them.
  [[nodiscard]] std::vector<SnapGuide> snapGuides() const;
  // The lengths the ghost shows, as measuredLengths gives them.
  [[nodiscard]] std::vector<MeasuredLength> measuredLengths(const World &world) const;

private:
  uint64_t Generation;
  ToolState Tool;
  std::optional<InspectorSubject> Subject;
  std::optional<ShopStock> StockChoice;
  std::optional<OpeningHours> HoursChoice;
};

} // namespace tpj

#endif
