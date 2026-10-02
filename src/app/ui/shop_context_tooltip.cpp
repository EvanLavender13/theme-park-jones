#include "app/ui/shop_context_tooltip.h"

#include <imgui.h>

namespace tpj {

void drawShopContextTooltip(const ShopContext &context) {
  // A second tooltip in a frame joins the first, so this reads below the food tooltip.
  if (!ImGui::BeginTooltip()) {
    return;
  }
  if (context.Connection) {
    ImGui::Text("Hungry footfall %.2f", context.Footfall);
  } else {
    ImGui::TextUnformatted("No guest connector");
  }
  if (context.Supply) {
    ImGui::Text("Supply route %.1f m from depot %llu", context.Supply->Distance,
                static_cast<unsigned long long>(context.Supply->Depot));
  } else {
    ImGui::TextUnformatted("No supply route");
  }
  ImGui::EndTooltip();
}

} // namespace tpj
