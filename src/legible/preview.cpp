#include "legible/preview.h"

#include "sim/guests/footfall.h"
#include "sim/park/intent.h"
#include "sim/routes/networks.h"

#include <algorithm>
#include <variant>
#include <vector>

namespace tpj {
namespace {

// The lists the shop context reuses: the world's boxes, the candidate's, and the keys of the
// candidate's guest segments.
struct ShopLists {
  std::vector<ParkBox> Boxes;
  std::vector<ParkBox> CandidateBoxes;
  std::vector<EntityKey> GuestSegments;
};

bool holdsBox(const std::vector<ParkBox> &boxes, EntityKey key) {
  return std::ranges::any_of(boxes, [key](const ParkBox &box) { return box.Key == key; });
}

// The shop box the edit adds or moves: an AddBox's is the lowest key the candidate holds a box at
// and the world does not, and a MoveBox's is its Box when the world holds a shop there.
std::optional<EntityKey> ghostShop(const World &world, const World &candidate, const ParkEdit &edit,
                                   ShopLists &lists) {
  parkBoxes(world, lists.Boxes);
  if (const auto *add = std::get_if<AddBox>(&edit)) {
    if (add->Kind != BoxKind::Shop) {
      return std::nullopt;
    }
    // Boxes come in ascending key order, so the first new one has the lowest key.
    parkBoxes(candidate, lists.CandidateBoxes);
    for (const ParkBox &box : lists.CandidateBoxes) {
      if (!holdsBox(lists.Boxes, box.Key)) {
        return box.Key;
      }
    }
    return std::nullopt;
  }
  if (const auto *move = std::get_if<MoveBox>(&edit)) {
    for (const ParkBox &box : lists.Boxes) {
      if (box.Key == move->Box && box.Kind == BoxKind::Shop) {
        return box.Key;
      }
    }
  }
  return std::nullopt;
}

// Where the shop's guest connector meets a guest path in the candidate: the first place at its
// last stop's node on a guest segment's carrier, or none.
std::optional<Place> connectionPlace(const World &candidate, EntityKey shop, ShopLists &lists) {
  const Network &network = parkNetwork(candidate, PathKind::Guest);
  const std::vector<Carrier> &carriers = network.carriers();
  const auto connector =
      std::ranges::find(carriers, connectorKey(shop, Face::Front), &Carrier::Key);
  if (connector == carriers.end() || connector->Stops.empty()) {
    return std::nullopt;
  }
  parkSegmentKeys(candidate, PathKind::Guest, lists.GuestSegments);
  for (const Place &place : network.stopPlaces(connector->Stops.back().Node)) {
    if (std::ranges::binary_search(lists.GuestSegments, place.Carrier)) {
      return place;
    }
  }
  return std::nullopt;
}

// What the preview's candidate applies: the edit's plan, with the lists applying it reuses.
struct PlannedChange {
  const ParkEdit *Edit = nullptr;
  const PlannedEdit *Planned = nullptr;
  TickBuffers *Buffers = nullptr;
};

void applyPlannedChange(World &into, const void *context) {
  const auto &change = *static_cast<const PlannedChange *>(context);
  applyPlanned(into, *change.Edit, *change.Planned, *change.Buffers);
}

// Makes preview the preview of the edit on the world, planning the edit once. Its candidate is made
// in spare, which it refills when spare holds a world on the world's schema, and then moves into
// the preview. A candidate the preview held before moves into spare first. When the preview holds
// no candidate, spare keeps the world it holds.
void makePreview(Preview &preview, std::optional<World> &spare, KeptFieldSample &footfall,
                 TickBuffers &buffers, const World &world, const std::optional<ParkEdit> &edit) {
  // Each preview that holds a candidate took it from spare and left spare empty, so the swap
  // destroys no world.
  if (preview.Candidate) {
    spare.swap(preview.Candidate);
    preview.Candidate.reset();
  }
  preview.Edit = edit;
  preview.Shop.reset();
  if (!edit) {
    return;
  }
  planEdit(world, *edit, preview.Planned, buffers);
  if (!isAccepted(preview.Planned)) {
    return;
  }
  if (!spare || &spare->schema() != &world.schema()) {
    // A world on the world's schema can only be made by copying it.
    spare = copyWorld(world);
  }
  const PlannedChange change{&*edit, &preview.Planned, &buffers};
  makeCandidate(world, applyPlannedChange, &change, buffers, *spare);
  preview.Candidate.swap(spare);
  preview.Shop = shopContext(world, *preview.Candidate, *edit, footfall, buffers);
}

} // namespace

Preview previewEdit(const World &world, const std::optional<ParkEdit> &edit) {
  Preview preview;
  std::optional<World> spare;
  KeptFieldSample footfall;
  TickBuffers buffers;
  makePreview(preview, spare, footfall, buffers, world, edit);
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
  makePreview(kept.Made, kept.Spare, kept.Footfall, kept.Buffers, world, edit);
  kept.Tick = world.Tick;
  return true;
}

std::optional<ShopContext> shopContext(const World &world, const World &candidate,
                                       const ParkEdit &edit) {
  KeptFieldSample footfall;
  TickBuffers buffers;
  return shopContext(world, candidate, edit, footfall, buffers);
}

std::optional<ShopContext> shopContext(const World &world, const World &candidate,
                                       const ParkEdit &edit, KeptFieldSample &footfall,
                                       TickBuffers &buffers) {
  ShopLists &lists = buffers.get<ShopLists>();
  const std::optional<EntityKey> shop = ghostShop(world, candidate, edit, lists);
  if (!shop) {
    return std::nullopt;
  }
  ShopContext context;
  context.Shop = *shop;
  context.Connection = connectionPlace(candidate, *shop, lists);
  // Footfall is sampled in the committed world, since a candidate is never stepped.
  if (context.Connection) {
    context.Footfall = fieldValue<HungryFootfall>(world, parkNetwork(world, PathKind::Guest),
                                                  *context.Connection, footfall);
  }
  context.Supply = nearestDepot(candidate, *shop, buffers);
  return context;
}

} // namespace tpj
