#include "sim/operations/facilities.h"

#include "sim/medium/field.h"
#include "sim/medium/flow.h"
#include "sim/operations/internal/facility_service.h"
#include "sim/operations/internal/offer_entries.h"
#include "sim/operations/operations.h"
#include "sim/park/intent.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <algorithm>
#include <array>
#include <optional>
#include <span>
#include <stddef.h>
#include <string>
#include <utility>
#include <vector>

namespace tpj {

namespace {

// The kinds, relief, places, and time of each facility kind.
template <BoxKind K> struct FacilityKinds;

template <> struct FacilityKinds<BoxKind::Bench> {
  using Visits = RestVisits;
  using Servings = Rests;
  using Offer = RestOffer;
  static constexpr double Relief = REST_RELIEF;
  static constexpr int64_t Places = BENCH_SEATS;
  static constexpr uint32_t Time = SITTING_TIME;
};

template <> struct FacilityKinds<BoxKind::ToiletBlock> {
  using Visits = ToiletVisits;
  using Servings = ToiletUses;
  using Offer = ToiletOffer;
  static constexpr double Relief = TOILET_RELIEF;
  static constexpr int64_t Places = TOILET_STALLS;
  static constexpr uint32_t Time = TOILET_TIME;
};

// Every facility kind, in the order they register and step.
inline constexpr std::array<BoxKind, 2> FACILITIES{BoxKind::Bench, BoxKind::ToiletBlock};

template <typename Action, size_t... At>
void forEachFacilityAt(Action &action, std::index_sequence<At...> /*at*/) {
  (action.template operator()<FACILITIES[At]>(), ...);
}

// Calls the action's operator()<K>() for each facility kind K of FACILITIES, in order.
template <typename Action> void forEachFacility(Action &&action) {
  forEachFacilityAt(action, std::make_index_sequence<FACILITIES.size()>{});
}

template <size_t... At> constexpr int64_t mostPlacesAt(std::index_sequence<At...> /*at*/) {
  return std::max({FacilityKinds<FACILITIES[At]>::Places...});
}

// The most places any facility kind has.
constexpr int64_t MOST_PLACES = mostPlacesAt(std::make_index_sequence<FACILITIES.size()>{});

// A count for each key, keys ascending.
using KeyCounts = std::vector<std::pair<EntityKey, int64_t>>;

// The lists stepFacilities reuses for every facility, so its loop allocates only while they grow.
struct FacilityBuffers {
  // Every box, ascending, read once a tick.
  std::vector<ParkBox> Boxes;
  // A copy of the stock entries being consumed from.
  std::vector<FlowStock> Held;
  // The nodes an offer's entries are placed at.
  std::vector<uint32_t> Nodes;
  // The facility's queue as it is being served.
  std::vector<EntityKey> Queue;
  // The tick each of its taken places frees, ascending.
  std::vector<uint64_t> FreeAt;
  KeyCounts Counts;
};

// The key's count, added at 0 when the counts have none.
int64_t &countOf(KeyCounts &counts, EntityKey key) {
  auto at = std::ranges::lower_bound(counts, key, {}, &std::pair<EntityKey, int64_t>::first);
  if (at == counts.end() || at->first != key) {
    at = counts.insert(at, {key, 0});
  }
  return at->second;
}

// The endpoint's stock entries of the kind, handles ascending, read in place.
template <typename K> std::span<const FlowStock> stocksAt(const World &world, EntityKey endpoint) {
  const Ledger *ledger = ledgerOf<K>(world);
  if (ledger == nullptr) {
    return {};
  }
  const auto [first, last] =
      std::ranges::equal_range(ledger->Stocks, endpoint, {}, &FlowStock::Endpoint);
  return {first, last};
}

bool isLive(const World &world, EntityKey key) { return key != NULL_KEY && world.isLive(key); }

// Consumes, as abandoned, every serving of the kind the facility holds, which only a gone guest's
// returned serving gives it, and every visit it holds for a guest that is gone. Copies each stock
// into held before consuming from it.
template <BoxKind K>
void abandonGone(World &world, EntityKey facility, std::vector<FlowStock> &held) {
  using Visits = typename FacilityKinds<K>::Visits;
  using Servings = typename FacilityKinds<K>::Servings;
  const std::span<const FlowStock> servings = stocksAt<Servings>(world, facility);
  copyGrowing(held, servings);
  for (const FlowStock &serving : held) {
    consumeUnits<Servings>(world, facility, serving.Handle, serving.Units, ABANDONED_CAUSE);
  }
  const std::span<const FlowStock> visits = stocksAt<Visits>(world, facility);
  if (std::ranges::all_of(
          visits, [&world](const FlowStock &visit) { return isLive(world, visit.Handle); })) {
    return;
  }
  copyGrowing(held, visits);
  for (const FlowStock &visit : held) {
    if (!isLive(world, visit.Handle)) {
      consumeUnits<Visits>(world, facility, visit.Handle, visit.Units, ABANDONED_CAUSE);
    }
  }
}

// Makes the buffers' queue the stored queue brought up to the visits the facility holds: each
// guest's earliest entries, up to its held units, then one entry for each further unit, guests in
// ascending key order. Makes the buffers' free ticks the stored ones still to come.
template <BoxKind K>
void reconcileQueue(const World &world, EntityKey facility, const FacilityService *stored,
                    FacilityBuffers &buffers) {
  using Visits = typename FacilityKinds<K>::Visits;
  buffers.Queue.clear();
  buffers.FreeAt.clear();
  buffers.Counts.clear();
  if (stored != nullptr) {
    for (const EntityKey guest : stored->Queue) {
      int64_t &kept = countOf(buffers.Counts, guest);
      if (kept < unitsHeld<Visits>(world, facility, guest)) {
        ++kept;
        buffers.Queue.push_back(guest);
      }
    }
    for (const uint64_t freeAt : stored->FreeAt) {
      if (freeAt > world.Tick) {
        buffers.FreeAt.push_back(freeAt);
      }
    }
  }
  for (const FlowStock &held : stocksAt<Visits>(world, facility)) {
    for (int64_t unit = countOf(buffers.Counts, held.Handle); unit < held.Units; ++unit) {
      buffers.Queue.push_back(held.Handle);
    }
  }
}

// Gives each free place to the guest at the front of the buffers' queue, sending its visit and
// serving back after the kind's time.
template <BoxKind K> void seatGuests(World &world, EntityKey facility, FacilityBuffers &buffers) {
  using Kinds = FacilityKinds<K>;
  size_t seated = 0;
  while (seated < buffers.Queue.size() && std::cmp_less(buffers.FreeAt.size(), Kinds::Places)) {
    const EntityKey guest = buffers.Queue[seated];
    createUnits<typename Kinds::Servings>(world, facility, guest, 1);
    sendUnits<typename Kinds::Servings>(world, facility, guest, guest, 1, Kinds::Time);
    sendUnits<typename Kinds::Visits>(world, facility, guest, guest, 1, Kinds::Time);
    buffers.FreeAt.push_back(world.Tick + Kinds::Time);
    ++seated;
  }
  buffers.Queue.erase(buffers.Queue.begin(),
                      buffers.Queue.begin() + static_cast<std::ptrdiff_t>(seated));
}

// The facility abandons what gone guests left, queues the visits it holds in arrival order, and
// gives each free place to the guest at the front. Leaves the queue and the taken places' free
// ticks in the buffers.
template <BoxKind K>
void serveFacility(World &world, EntityKey facility, FacilityBuffers &buffers) {
  abandonGone<K>(world, facility, buffers.Held);
  FacilityService *stored = world.tryGet<FacilityService>(facility);
  reconcileQueue<K>(world, facility, stored, buffers);
  seatGuests<K>(world, facility, buffers);
  if (stored != nullptr) {
    copyGrowing(stored->Queue, std::span<const EntityKey>(buffers.Queue));
    copyGrowing(stored->FreeAt, std::span<const uint64_t>(buffers.FreeAt));
  } else if (!buffers.Queue.empty() || !buffers.FreeAt.empty()) {
    world.emplace<FacilityService>(facility, FacilityService{buffers.Queue, buffers.FreeAt});
  }
}

// The ticks the next visit waits, from the cycle stepping from, in which the facility first holds
// it, to the one that seats its guest, behind the queued guests. Each place is free at its tick in
// freeAt, or at from when it is not taken. Each queued guest takes the earliest free place, no
// earlier than from, and keeps it for the time, and the arriving guest takes the earliest place
// left.
uint64_t facilityWait(uint64_t from, std::span<const uint64_t> freeAt, size_t queued,
                      int64_t places, uint32_t time) {
  std::array<uint64_t, MOST_PLACES> free{};
  const auto count = static_cast<size_t>(places);
  for (size_t place = 0; place < count; ++place) {
    free[place] = place < freeAt.size() ? std::max(freeAt[place], from) : from;
  }
  const auto earliest = [&free, count]() {
    return std::min_element(free.begin(), free.begin() + static_cast<std::ptrdiff_t>(count));
  };
  for (size_t guest = 0; guest < queued; ++guest) {
    uint64_t &place = *earliest();
    place += time;
  }
  return *earliest() - from;
}

// Publishes the facility's offer for the next visit to arrive, from its queue and taken places
// after serving.
template <BoxKind K>
void publishFacilityOffer(World &world, EntityKey facility, FacilityBuffers &buffers) {
  using Kinds = FacilityKinds<K>;
  const OfferEntry offer{.Relief = Kinds::Relief,
                         .Wait = facilityWait(world.Tick + 1, buffers.FreeAt, buffers.Queue.size(),
                                              Kinds::Places, Kinds::Time),
                         .Supplied = true};
  fillOfferEntries(world, facility, offer, buffers.Nodes,
                   publishStepped<typename Kinds::Offer>(world, facility));
}

// Each bench and toilet block, in ascending key order, serves its guests and publishes its offer.
void stepFacilities(World &world, TickBuffers &tick) {
  FacilityBuffers &buffers = tick.get<FacilityBuffers>();
  parkBoxes(world, buffers.Boxes);
  for (const ParkBox &box : buffers.Boxes) {
    forEachFacility([&]<BoxKind K>() {
      if (box.Kind == K) {
        serveFacility<K>(world, box.Key, buffers);
        publishFacilityOffer<K>(world, box.Key, buffers);
      }
    });
  }
}

// Publishes the offer of each box of the kind, as for a facility that has not stepped: every place
// free, so the first guest waits 0.
template <BoxKind K> void resolveFacilityOffers(World &world) {
  using Kinds = FacilityKinds<K>;
  const OfferEntry offer{.Relief = Kinds::Relief, .Wait = 0, .Supplied = true};
  for (const ParkBox &box : parkBoxes(world)) {
    if (box.Kind == K) {
      publishResolved<typename Kinds::Offer>(world, box.Key, offerEntries(world, box.Key, offer));
    }
  }
}

} // namespace

std::optional<FacilityRecord> facilityRecord(const World &world, EntityKey facility) {
  std::optional<FacilityRecord> record;
  const std::optional<ParkBox> box = parkBox(world, facility);
  if (!box) {
    return record;
  }
  forEachFacility([&]<BoxKind K>() {
    using Kinds = FacilityKinds<K>;
    if (box->Kind != K) {
      return;
    }
    record = FacilityRecord{.Places = Kinds::Places};
    if (const FacilityService *service = world.tryGet<FacilityService>(facility)) {
      record->Taken = std::ranges::count_if(
          service->FreeAt, [&world](uint64_t freeAt) { return freeAt > world.Tick; });
    }
    for (const FlowStock &held : stocksAt<typename Kinds::Visits>(world, facility)) {
      if (isLive(world, held.Handle)) {
        record->Queue += held.Units;
      }
    }
  });
  return record;
}

void addFacilities(WorldSchema &schema) {
  forEachFacility([&]<BoxKind K>() {
    addFlow<typename FacilityKinds<K>::Visits>(schema);
    addFlow<typename FacilityKinds<K>::Servings>(schema);
  });
  schema.addComponent<FacilityService>("facility-service", DataKind::State);
  forEachFacility([&]<BoxKind K>() {
    using Offer = typename FacilityKinds<K>::Offer;
    addField<Offer>(schema);
    schema.addResolver(Offer::Name, &resolveFacilityOffers<K>,
                       {"path-networks", std::string(Offer::Name) + "-field"});
  });
  schema.addSystem(&stepFacilities);
}

} // namespace tpj
