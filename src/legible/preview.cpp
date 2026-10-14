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
// last stop's node on a guest segment's carrier, or none.
std::optional<Place> connectionPlace(const World &candidate, EntityKey shop) {
  const Network &network = parkNetwork(candidate, PathKind::Guest);
  const std::vector<Carrier> &carriers = network.carriers();
  const auto connector =
      std::ranges::find(carriers, connectorKey(shop, Face::Front), &Carrier::Key);
  if (connector == carriers.end() || connector->Stops.empty()) {
    return std::nullopt;
  }
  const std::vector<ParkSegment> segments = parkSegments(candidate);
  for (const Place &place : network.stopPlaces(connector->Stops.back().Node)) {
    const bool onGuestPath = std::ranges::any_of(segments, [&place](const ParkSegment &segment) {
      return segment.Kind == PathKind::Guest && segment.Key == place.Carrier;
    });
    if (onGuestPath) {
      return place;
    }
  }
  return std::nullopt;
}

// The world's hungry footfall at the place on its guest network.
double footfallAt(const World &world, const Place &place) {
  return fieldValue<HungryFootfall>(world, parkNetwork(world, PathKind::Guest), place);
}

// Makes preview the preview of the edit on the world. Its candidate is made in spare, which it
// refills when spare holds a world on the world's schema, and then moves into the preview. A
// candidate the preview held before moves into spare first. When the preview holds no candidate,
// spare keeps the world it holds.
void makePreview(Preview &preview, std::optional<World> &spare, CommandQueue &queue,
                 TickBuffers &buffers, const World &world, const std::optional<ParkEdit> &edit) {
  // Each preview that holds a candidate took it from spare and left spare empty, so the swap
  // destroys no world.
  if (preview.Candidate) {
    spare.swap(preview.Candidate);
    preview.Candidate.reset();
  }
  preview.Edit = edit;
  preview.Shop.reset();
  if (!edit || !isAccepted(world, *edit)) {
    return;
  }
  queue.clear();
  queueEdit(queue, *edit);
  if (spare && &spare->schema() == &world.schema()) {
    makeCandidate(world, queue, buffers, *spare);
  } else {
    // A world on the world's schema can only be made by copying it, so the copy is the candidate's
    // start, and committing the edit to it makes the candidate.
    spare = copyWorld(world);
    commitCommands(*spare, queue, buffers);
  }
  preview.Candidate.swap(spare);
  preview.Shop = shopContext(world, *preview.Candidate, *edit);
}

} // namespace

Preview previewEdit(const World &world, const std::optional<ParkEdit> &edit) {
  Preview preview;
  std::optional<World> spare;
  CommandQueue queue;
  TickBuffers buffers;
  makePreview(preview, spare, queue, buffers, world, edit);
  return preview;
}

const World &previewedWorld(const World &world, const Preview &preview) {
  return preview.Candidate ? *preview.Candidate : world;
}

bool keepPreview(KeptPreview &kept, const World &world, const std::optional<ParkEdit> &edit,
                 bool followTicks) {
  if (kept.Tick.has_value() && kept.Made.Edit == edit &&
      (kept.Tick == world.Tick || !followTicks || !kept.Made.Candidate)) {
    // The shop's footfall is the world's now, which ticks change.
    if (kept.Made.Shop && kept.Made.Shop->Connection) {
      kept.Made.Shop->Footfall = fieldValue<HungryFootfall>(
          world, parkNetwork(world, PathKind::Guest), *kept.Made.Shop->Connection, kept.Footfall);
    }
    return false;
  }
  makePreview(kept.Made, kept.Spare, kept.Queue, kept.Buffers, world, edit);
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
    context.Footfall = footfallAt(world, *context.Connection);
  }
  context.Supply = nearestDepot(candidate, *shop);
  return context;
}

} // namespace tpj
