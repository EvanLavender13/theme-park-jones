#include "app/ui/panel_data.h"

#include "legible/needs.h"

namespace tpj {

uint64_t PanelData::guestCount(const World &world) {
  parkGuests(world, Guests);
  return Guests.size();
}

const ArrivalDay &PanelData::arrivalDay(const World &world, OpeningHours hours) {
  keepArrivalDay(Arrivals, world, hours);
  return Arrivals.Day;
}

const Inspection &PanelData::inspection(const World &world, const InspectorSubject &subject) {
  inspectSubject(world, subject, InspectKept, Inspected);
  return Inspected;
}

std::optional<ShopStock> PanelData::shopStock(const World &world, EntityKey key) {
  const std::optional<ParkBox> box = parkBox(world, key);
  if (!box || box->Kind != BoxKind::Shop) {
    return std::nullopt;
  }
  return box->Stock;
}

const Availability *PanelData::tooltipAvailability(const World &world, GuestNeed need,
                                                   const OverlayShape &shape,
                                                   std::optional<ParkPoint> point) {
  if (!point) {
    return nullptr;
  }
  const std::optional<Place> place =
      overlayPlace(shape, GroundPoint{point->X, point->Z}, OverlayLines);
  if (!place) {
    return nullptr;
  }
  availabilityAt(world, needAvailability(need), *place, AvailabilityKept, Tooltip);
  return &Tooltip;
}

const GraphOverlay &PanelData::graph(const World &world, const CameraView &view, float width,
                                     float height) {
  buildGraphOverlay(world, view, width, height, GraphKeys, Graph);
  return Graph;
}

} // namespace tpj
