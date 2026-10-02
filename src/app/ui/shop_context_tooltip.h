#ifndef TPJ_APP_UI_SHOP_CONTEXT_TOOLTIP_H
#define TPJ_APP_UI_SHOP_CONTEXT_TOOLTIP_H

#include "legible/preview.h"

namespace tpj {

// Draws, in the tooltip at the cursor, a shop ghost's hungry footfall where it would meet the guest
// path and its supply route, or that it has none. Call between ImGui::NewFrame and ImGui::Render.
void drawShopContextTooltip(const ShopContext &context);

} // namespace tpj

#endif
