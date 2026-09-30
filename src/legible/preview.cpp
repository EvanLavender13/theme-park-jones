#include "legible/preview.h"

#include "sim/command_queue.h"
#include "sim/guests/footfall.h"
#include "sim/medium/field.h"
#include "sim/park/intent.h"
#include "sim/routes/networks.h"

#include <algorithm>
#include <variant>
#include <vector>

namespace tpj {
namespace {

bool holdsBox(const std::vector<ParkBox> &boxes, EntityKey key) {
  return std::ranges::any_of(boxes, [key](const ParkBox &box) { return box.Key == key; });
}

// The shop box the edit adds or moves: an AddBox's is the lowest key the candidate holds a box at
// and the world does not, and a MoveBox's is its Box when the world holds a shop there.
std::optional<EntityKey> ghostShop(const World &world, const World &candidate,
                                   const ParkEdit &edit) {
  const std::vector<ParkBox> boxes = parkBoxes(world);
  if (const auto *add = std::get_if<AddBox>(&edit)) {
    if (add->Kind != BoxKind::Shop) {
      return std::nullopt;
    }
    // Boxes come in ascending key order, so the first new one has the lowest key.
    for (const ParkBox &box : parkBoxes(candidate)) {
      if (!holdsBox(boxes, box.Key)) {
        return box.Key;
      }
    }
    return std::nullopt;
  }
  if (const auto *move = std::get_if<MoveBox>(&edit)) {
    for (const ParkBox &box : boxes) {
      if (box.Key == move->Box && box.Kind == BoxKind::Shop) {
        return box.Key;
      }
    }
  }
  return std::nullopt;
}

// Where the shop's guest connector meets a guest path in the candidate: the first place at its
// last stop's node on a guest path's carrier, or none.
std::optional<Place> connectionPlace(const World &candidate, EntityKey shop) {
  const Network &network = parkNetwork(candidate, PathKind::Guest);
  const std::vector<Carrier> &carriers = network.carriers();
  const auto connector =
      std::ranges::find(carriers, connectorKey(shop, Face::Front), &Carrier::Key);
  if (connector == carriers.end() || connector->Stops.empty()) {
    return std::nullopt;
  }
  const std::vector<ParkPath> paths = parkPaths(candidate);
  for (const Place &place : network.stopPlaces(connector->Stops.back().Node)) {
    const bool onGuestPath = std::ranges::any_of(paths, [&place](const ParkPath &path) {
      return path.Kind == PathKind::Guest && path.Key == place.Carrier;
    });
    if (onGuestPath) {
      return place;
    }
  }
  return std::nullopt;
}

} // namespace

Preview previewEdit(const World &world, const std::optional<ParkEdit> &edit) {
  Preview preview{edit, std::nullopt, std::nullopt};
  if (!edit || !isAccepted(world, *edit)) {
    return preview;
  }
  CommandQueue queue;
  queueEdit(queue, *edit);
  preview.Candidate = makeCandidate(world, queue);
  preview.Shop = shopContext(world, *preview.Candidate, *edit);
  return preview;
}

const World &previewedWorld(const World &world, const Preview &preview) {
  return preview.Candidate ? *preview.Candidate : world;
}

bool keepPreview(KeptPreview &kept, const World &world, const std::optional<ParkEdit> &edit) {
  if (kept.Tick == world.Tick && kept.Made.Edit == edit) {
    return false;
  }
  kept.Made = previewEdit(world, edit);
  kept.Tick = world.Tick;
  return true;
}

std::optional<ShopContext> shopContext(const World &world, const World &candidate,
                                       const ParkEdit &edit) {
  const std::optional<EntityKey> shop = ghostShop(world, candidate, edit);
  if (!shop) {
    return std::nullopt;
  }
  ShopContext context;
  context.Shop = *shop;
  context.Connection = connectionPlace(candidate, *shop);
  // Footfall is sampled in the committed world, since a candidate is never stepped.
  if (context.Connection) {
    context.Footfall =
        fieldValue<HungryFootfall>(world, parkNetwork(world, PathKind::Guest), *context.Connection);
  }
  context.Supply = nearestDepot(candidate, *shop);
  return context;
}

} // namespace tpj
