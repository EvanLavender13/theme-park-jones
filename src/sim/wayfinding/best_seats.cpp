#include "sim/wayfinding/best_seats.h"

#include "sim/entity_key.h"
#include "sim/furniture/furniture.h"
#include "sim/medium/field.h"
#include "sim/medium/network.h"
#include "sim/operations/seats.h"
#include "sim/park/intent.h"
#include "sim/routes/best_places.h"
#include "sim/routes/networks.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <algorithm>
#include <optional>
#include <stdint.h>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace tpj {

namespace {

// The piece's seat offer, or none when it has none.
const SeatOffer *offerOf(std::span<const FieldSlot<SeatOffer>> offers, EntityKey piece) {
  const auto found = std::ranges::lower_bound(offers, piece, {}, &FieldSlot<SeatOffer>::Source);
  if (found == offers.end() || found->Source != piece || found->Entries.empty()) {
    return nullptr;
  }
  return &found->Entries.front().Value;
}

// Publishes the field's best places from the seeds, under the field's own key, when there are any.
template <typename F>
void publishBestSeats(World &world, const Network &network, const std::vector<PlaceSeed> &seeds) {
  std::vector<PlacedEntry<BestPlace>> entries = bestPlaces(network, seeds, SEAT_CHOICES);
  if (!entries.empty()) {
    publishResolved<F>(world, fieldKey(F::Name), std::move(entries));
  }
}

// Seeds best rest seats with each piece's rest relief and best meal seats with its meal relief,
// each when above 0, from the seat places and seat offers, and publishes both.
void resolveBestSeats(World &world) {
  const Network &network = parkNetwork(world, PathKind::Guest);
  const std::span<const FieldSlot<SeatOffer>> offers = resolvedSlots<SeatOffers>(world);
  std::vector<PlaceSeed> rest;
  std::vector<PlaceSeed> meal;
  for (const FieldSlot<SeatPlace> &slot : resolvedSlots<SeatPlaces>(world)) {
    for (const PlacedEntry<SeatPlace> &entry : slot.Entries) {
      const SeatOffer *offer = offerOf(offers, entry.Value.Piece);
      if (offer == nullptr) {
        continue;
      }
      const PlaceSeed seed{entry.Value.Piece, entry.Value.Segment, entry.At, 0.0};
      if (offer->RestRelief > 0.0) {
        rest.push_back(seed);
        rest.back().Value = offer->RestRelief;
      }
      if (offer->MealRelief > 0.0) {
        meal.push_back(seed);
        meal.back().Value = offer->MealRelief;
      }
    }
  }
  publishBestSeats<BestRestSeats>(world, network, rest);
  publishBestSeats<BestMealSeats>(world, network, meal);
}

// The use's field's entries at the place, as bestSeatsAt gives them.
template <typename F>
void sampleBestSeats(const World &world, const Network &network, const Place &place,
                     SampleScratch<BestPlace> &scratch, std::vector<BestPlace> &out) {
  const FieldSlot<BestPlace> *slot = sourceSlot<F>(world, fieldKey(F::Name));
  if (slot == nullptr) {
    return;
  }
  const std::optional<NetworkPosition> position = network.resolve(place);
  if (!position) {
    return;
  }
  // As sampleField reads a node: the slot's entries at the node's stop places.
  if (const auto *node = std::get_if<NodePosition>(&*position)) {
    positionsAtAny(*slot, network.stopPlaces(node->Node), scratch.Positions);
    for (const uint32_t i : scratch.Positions) {
      out.push_back(slot->Entries[i].Value);
    }
    return;
  }
  // As sampleField reads a place inside an edge: the field's rule over the entries at the edge's
  // ends and along it.
  const auto &inside = std::get<EdgePosition>(*position);
  const auto read = [slot](uint32_t i) { return slot->Entries[i].Value; };
  if (gatherEdgeSample(network, *slot, network.edges()[inside.Edge], scratch, read)) {
    scratch.Edge.FromOffset = inside.FromOffset;
    scratch.Edge.ToOffset = inside.ToOffset;
    sampleBestPlaceEdge(scratch.Edge, SEAT_CHOICES, out);
  }
}

} // namespace

void bestSeatsAt(const World &world, const Network &network, const Place &place, SeatUse use,
                 SampleScratch<BestPlace> &scratch, std::vector<BestPlace> &out) {
  out.clear();
  if (use == SeatUse::Rest) {
    sampleBestSeats<BestRestSeats>(world, network, place, scratch, out);
  } else {
    sampleBestSeats<BestMealSeats>(world, network, place, scratch, out);
  }
}

void BestRestSeats::sampleEdge(const EdgeSample<BestPlace> &sample, std::vector<BestPlace> &out) {
  sampleBestPlaceEdge(sample, SEAT_CHOICES, out);
}

void BestMealSeats::sampleEdge(const EdgeSample<BestPlace> &sample, std::vector<BestPlace> &out) {
  sampleBestPlaceEdge(sample, SEAT_CHOICES, out);
}

void addBestSeats(WorldSchema &schema) {
  addField<BestRestSeats>(schema);
  addField<BestMealSeats>(schema);
  schema.addResolver("best-seats", &resolveBestSeats,
                     {std::string(SeatOffers::Name), std::string(BestRestSeats::Name) + "-field",
                      std::string(BestMealSeats::Name) + "-field"});
}

} // namespace tpj
