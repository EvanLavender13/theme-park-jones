#ifndef TPJ_APP_UI_INSPECTOR_WINDOW_H
#define TPJ_APP_UI_INSPECTOR_WINDOW_H

#include "legible/inspect.h"
#include "sim/park/intent.h"

#include <optional>

namespace tpj {

// The Inspector's stock controls for a shop: the shop's stock, and the one marked.
struct StockControls {
  ShopStock Current = ShopStock::Food;
  ShopStock Chosen = ShopStock::Food;
};

// What the player did in the Inspector: whether it is still open, the stock they chose this frame,
// and whether they pressed Apply.
struct InspectorUse {
  bool Open = true;
  std::optional<ShopStock> Chosen;
  bool Apply = false;
};

// Draws the Inspector window: the inspection's title, then that its entity is gone or its rows,
// and the options a guest would weigh now as a table, one to a row. With stock controls, and while
// the entity is there, it also draws a Sells row of radio buttons, Food, Drink, and Both, with
// Chosen marked, and an Apply button, disabled while Chosen is Current. Call between
// ImGui::NewFrame and ImGui::Render.
InspectorUse drawInspector(const Inspection &inspection, const std::optional<StockControls> &stock);

} // namespace tpj

#endif
