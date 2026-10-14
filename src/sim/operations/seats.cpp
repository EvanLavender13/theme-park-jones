#include "sim/operations/seats.h"

#include "sim/entity_key.h"
#include "sim/fixed_list.h"
#include "sim/furniture/furniture.h"
#include "sim/medium/field.h"
#include "sim/medium/flow.h"
#include "sim/operations/facilities.h"
#include "sim/operations/internal/seat_service.h"
#include "sim/operations/operations.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <algorithm>
#include <span>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

namespace tpj {

namespace {

bool isLive(const World &world, EntityKey key) { return key != NULL_KEY && world.isLive(key); }

// Copies the endpoint's stock entries of the kind into out, handles ascending.
template <typename K>
void copyStocks(const World &world, EntityKey endpoint, std::vector<FlowStock> &out) {
  out.clear();
  const Ledger *ledger = ledgerOf<K>(world);
  if (ledger == nullptr) {
    return;
  }
  const auto [first, last] =
      std::ranges::equal_range(ledger->Stocks, endpoint, {}, &FlowStock::Endpoint);
  copyGrowing(out, std::span<const FlowStock>(first, last));
}

// Publishes one offer for each seat place, at its place, from its piece, pieces in ascending key
// order.
void resolveSeatOffers(World &world, TickBuffers & /*buffers*/) {
  using PieceOffer = std::pair<EntityKey, PlacedEntry<SeatOffer>>;
  std::vector<PieceOffer> offers;
  for (const FieldSlot<SeatPlace> &slot : resolvedSlots<SeatPlaces>(world)) {
    for (const PlacedEntry<SeatPlace> &entry : slot.Entries) {
      offers.emplace_back(
          entry.Value.Piece,
          PlacedEntry<SeatOffer>{entry.At, seatOffer(entry.Value.Kind, entry.Value.Seats)});
    }
  }
  // Each publish then goes at the end of the field's slots.
  std::ranges::sort(offers, {}, &PieceOffer::first);
  for (const PieceOffer &offer : offers) {
    publishResolved<SeatOffers>(world, offer.first).push_back(offer.second);
  }
}

// The lists stepSeats reuses for every piece, so its loop allocates only while they grow.
struct SeatBuffers {
  // The live endpoints holding seat visits under another key, ascending.
  std::vector<EntityKey> Pieces;
  std::vector<FlowStock> Visits;
  std::vector<FlowStock> Rests;
  // The taken seats' free ticks of a piece with no seat state.
  FixedList<uint64_t, MOST_FURNITURE_SEATS> FreeAt;
};

// The piece's seats, as its seat offer gives them, or 0 when it has none.
uint32_t seatsOf(const World &world, EntityKey piece) {
  const FieldSlot<SeatOffer> *slot = sourceSlot<SeatOffers>(world, piece);
  return slot != nullptr && !slot->Entries.empty() ? slot->Entries.front().Value.Seats : 0;
}

// The piece consumes, as abandoned, the rests and visits a gone guest left it. It seats each other
// visit it holds while a seat is free, sending the visit and a rest back after SEAT_TIME, and sends
// the rest back after RETURN_DELAY with nothing.
void servePiece(World &world, EntityKey piece, SeatBuffers &buffers) {
  copyStocks<Rests>(world, piece, buffers.Rests);
  for (const FlowStock &held : buffers.Rests) {
    consumeUnits<Rests>(world, piece, held.Handle, held.Units, ABANDONED_CAUSE);
  }
  copyStocks<SeatVisits>(world, piece, buffers.Visits);
  SeatService *stored = world.tryGet<SeatService>(piece);
  if (stored == nullptr) {
    buffers.FreeAt.clear();
  }
  FixedList<uint64_t, MOST_FURNITURE_SEATS> &freeAt =
      stored != nullptr ? stored->FreeAt : buffers.FreeAt;
  freeAt.erase(std::remove_if(freeAt.begin(), freeAt.end(),
                              [&world](uint64_t at) { return at <= world.Tick; }),
               freeAt.end());
  const uint32_t seats = seatsOf(world, piece);
  for (const FlowStock &held : buffers.Visits) {
    const EntityKey guest = held.Handle;
    if (!isLive(world, guest)) {
      consumeUnits<SeatVisits>(world, piece, guest, held.Units, ABANDONED_CAUSE);
      continue;
    }
    for (int64_t unit = 0; unit < held.Units; ++unit) {
      if (std::cmp_less(freeAt.size(), seats)) {
        createUnits<Rests>(world, piece, guest, 1);
        sendUnits<Rests>(world, piece, guest, guest, 1, SEAT_TIME);
        sendUnits<SeatVisits>(world, piece, guest, guest, 1, SEAT_TIME);
        freeAt.push_back(world.Tick + SEAT_TIME);
      } else {
        sendUnits<SeatVisits>(world, piece, guest, guest, 1, RETURN_DELAY);
      }
    }
  }
  if (freeAt.empty()) {
    world.remove<SeatService>(piece);
  } else if (stored == nullptr) {
    world.emplace<SeatService>(piece, SeatService{buffers.FreeAt});
  }
}

// Each piece holding seat visits, in ascending key order, seats or turns away its visitors. A
// piece holds a visit under its guest's key, and a guest holds its own under its own. A piece the
// resolution after the last swap removed is skipped, and the next swap passes its stock to its
// guests.
void stepSeats(World &world, TickBuffers &tick) {
  const Ledger *ledger = ledgerOf<SeatVisits>(world);
  if (ledger == nullptr) {
    return;
  }
  SeatBuffers &buffers = tick.get<SeatBuffers>();
  buffers.Pieces.clear();
  for (const FlowStock &stock : ledger->Stocks) {
    if (stock.Handle != stock.Endpoint && isLive(world, stock.Endpoint) &&
        (buffers.Pieces.empty() || buffers.Pieces.back() != stock.Endpoint)) {
      buffers.Pieces.push_back(stock.Endpoint);
    }
  }
  for (const EntityKey piece : buffers.Pieces) {
    servePiece(world, piece, buffers);
  }
}

} // namespace

void addSeats(WorldSchema &schema) {
  addFlow<SeatVisits>(schema);
  schema.addComponent<SeatService>("seat-service", DataKind::State);
  addField<SeatOffers>(schema);
  schema.addResolver(SeatOffers::Name, &resolveSeatOffers,
                     {"furniture", std::string(SeatOffers::Name) + "-field"});
  schema.addSystem(&stepSeats);
}

} // namespace tpj
