#ifndef TPJ_APP_UI_PANEL_DATA_H
#define TPJ_APP_UI_PANEL_DATA_H

#include "legible/arrivals.h"
#include "legible/availability.h"
#include "legible/inspect.h"
#include "render/graph_overlay.h"
#include "render/paving_overlay.h"
#include "render/renderer.h"
#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/park/intent.h"
#include "sim/world.h"

#include <optional>
#include <stdint.h>
#include <vector>

namespace tpj {

// The data the frame's panels and tooltips show, worked out into lists kept between frames, so
// working it out again allocates nothing once they have grown.
class PanelData {
public:
  // The number of guests in the park.
  uint64_t guestCount(const World &world);
  // The arrival day for the hours, made again only when the hours change.
  const ArrivalDay &arrivalDay(const World &world, OpeningHours hours);
  // The subject's inspection.
  const Inspection &inspection(const World &world, const InspectorSubject &subject);
  // The stock setting of the shop the key holds, or none when it holds no shop.
  static std::optional<ShopStock> shopStock(const World &world, EntityKey key);
  // The need's availability at the overlay place of the point in the shape, or null when there is
  // no point or the shape has no overlay place there.
  const Availability *tooltipAvailability(const World &world, GuestNeed need,
                                          const OverlayShape &shape,
                                          std::optional<ParkPoint> point);
  // The Graph view's lines and nodes for the view in a window of the width and height.
  const GraphOverlay &graph(const World &world, const CameraView &view, float width, float height);

private:
  std::vector<EntityKey> Guests;
  KeptArrivalDay Arrivals;
  InspectLists InspectKept;
  Inspection Inspected;
  std::vector<uint32_t> OverlayLines;
  AvailabilityLists AvailabilityKept;
  Availability Tooltip;
  std::vector<EntityKey> GraphKeys;
  GraphOverlay Graph;
};

} // namespace tpj

#endif
