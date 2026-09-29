#ifndef TPJ_SIM_MEDIUM_FLOW_H
#define TPJ_SIM_MEDIUM_FLOW_H

#include "sim/entity_key.h"
#include "sim/mix.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <algorithm>
#include <compare>
#include <concepts>
#include <iterator>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace tpj {

// A flow kind's definition, a type its owning module supplies:
//   struct Meals {
//     static constexpr std::string_view Name = "meals";
//   };
template <typename K>
concept FlowDefinition = requires {
  { K::Name } -> std::convertible_to<std::string_view>;
};

inline constexpr uint64_t FLOW_PURPOSE = hashName("flow");

// The key of the entity holding the named kind's ledger.
constexpr EntityKey flowKey(std::string_view name) {
  return deriveKey(NULL_KEY, FLOW_PURPOSE, hashName(name));
}

// The causes the ledger itself consumes units with.
inline constexpr std::string_view UNDELIVERABLE_CAUSE = "undeliverable";
inline constexpr std::string_view DISCARDED_CAUSE = "discarded";

// Units in transit from one endpoint to another. Packets are ordered by their fields, in the
// order they are declared.
struct FlowPacket {
  uint64_t Arrival = 0;
  EntityKey From = NULL_KEY;
  EntityKey To = NULL_KEY;
  EntityKey Handle = NULL_KEY;
  int64_t Units = 0;
  uint32_t Delay = 0;
  // Set on a packet going back to its sender, which is then its To.
  bool Returning = false;

  bool operator==(const FlowPacket &) const = default;
  std::strong_ordering operator<=>(const FlowPacket &other) const;
};

// Compares the fields in order, with Returning as 0 or 1: a defaulted comparison would read the
// bool as an int implicitly.
inline std::strong_ordering FlowPacket::operator<=>(const FlowPacket &other) const {
  const auto order = [](const FlowPacket &packet) {
    return std::tuple(packet.Arrival, packet.From, packet.To, packet.Handle, packet.Units,
                      packet.Delay, packet.Returning ? 1 : 0);
  };
  return order(*this) <=> order(other);
}

template <typename Visitor> void visitFields(Visitor &visitor, FlowPacket &packet) {
  visitor.field("arrival", packet.Arrival);
  visitor.field("from", packet.From);
  visitor.field("to", packet.To);
  visitor.field("handle", packet.Handle);
  visitor.field("units", packet.Units);
  visitor.field("delay", packet.Delay);
  visitor.field("returning", packet.Returning);
}

// The units an endpoint holds under one handle.
struct FlowStock {
  EntityKey Endpoint = NULL_KEY;
  EntityKey Handle = NULL_KEY;
  int64_t Units = 0;
};

template <typename Visitor> void visitFields(Visitor &visitor, FlowStock &stock) {
  visitor.field("endpoint", stock.Endpoint);
  visitor.field("handle", stock.Handle);
  visitor.field("units", stock.Units);
}

// The units consumed with one cause, held as the hashName of its name.
struct FlowConsumption {
  uint64_t Cause = 0;
  int64_t Units = 0;
};

template <typename Visitor> void visitFields(Visitor &visitor, FlowConsumption &consumption) {
  visitor.field("cause", consumption.Cause);
  visitor.field("units", consumption.Units);
}

// One kind's ledger. State. Packets in their order, stocks by endpoint and then handle with no
// empty entries, and consumption by cause.
struct Ledger {
  std::vector<FlowPacket> Packets;
  std::vector<FlowStock> Stocks;
  int64_t Created = 0;
  std::vector<FlowConsumption> Consumed;
};

template <typename Visitor> void visitFields(Visitor &visitor, Ledger &ledger) {
  visitor.field("packets", ledger.Packets);
  visitor.field("stocks", ledger.Stocks);
  visitor.field("created", ledger.Created);
  visitor.field("consumed", ledger.Consumed);
}

// The registered component holding kind K's ledger.
template <FlowDefinition K> struct FlowLedger : Ledger {};

template <typename Visitor, typename K> void visitFields(Visitor &visitor, FlowLedger<K> &ledger) {
  visitFields(visitor, static_cast<Ledger &>(ledger));
}

// A handle's units in an endpoint's stock.
struct FlowHolding {
  EntityKey Handle = NULL_KEY;
  int64_t Units = 0;
};

// The kind-independent work behind the templates below. The kind's name is for messages.
void ledgerCreate(const World &world, Ledger &ledger, std::string_view kind, EntityKey endpoint,
                  EntityKey handle, int64_t units);
void ledgerSend(const World &world, Ledger &ledger, std::string_view kind, EntityKey from,
                EntityKey to, EntityKey handle, int64_t units, uint32_t delay);
void ledgerConsume(const World &world, Ledger &ledger, std::string_view kind, EntityKey endpoint,
                   EntityKey handle, int64_t units, std::string_view cause);
// Delivers due packets, returns those whose destination is gone, and consumes as undeliverable
// those whose sender is gone too. Then moves each gone endpoint's units to its handle's entity, or
// consumes them as discarded when that is gone as well. World::Tick is the tick just reached.
void ledgerSwap(const World &world, Ledger &ledger);
[[nodiscard]] int64_t ledgerHeld(const Ledger &ledger, EntityKey endpoint, EntityKey handle);

// The kind's ledger, or null when the world holds none.
template <FlowDefinition K> const Ledger *ledgerOf(const World &world) {
  const entt::entity entity = world.findEntity(flowKey(K::Name));
  return entity == entt::null ? nullptr : world.Registry.try_get<FlowLedger<K>>(entity);
}

// The kind's ledger for an operation. Throws std::logic_error when the world is not stepping or
// holds no ledger for the kind.
template <FlowDefinition K> Ledger &steppingLedger(World &world) {
  const std::string name(K::Name);
  if (!world.isStepping()) {
    throw std::logic_error("flow " + name + " takes operations only from systems");
  }
  const entt::entity entity = world.findEntity(flowKey(K::Name));
  auto *ledger = entity == entt::null ? nullptr : world.Registry.try_get<FlowLedger<K>>(entity);
  if (ledger == nullptr) {
    throw std::logic_error("flow " + name + " has no ledger; register it and resolve the world");
  }
  return *ledger;
}

// The kind's resolver: creates the entity holding its ledger, with an empty ledger if it has none.
template <FlowDefinition K> void ensureLedger(World &world) {
  const EntityKey key = world.createDerivedEntity(NULL_KEY, FLOW_PURPOSE, hashName(K::Name));
  static_cast<void>(world.Registry.get_or_emplace<FlowLedger<K>>(world.findEntity(key)));
}

// The kind's swap: delivers, returns, and settles, as ledgerSwap does.
template <FlowDefinition K> void swapLedger(World &world) {
  const entt::entity entity = world.findEntity(flowKey(K::Name));
  auto *ledger = entity == entt::null ? nullptr : world.Registry.try_get<FlowLedger<K>>(entity);
  if (ledger != nullptr) {
    ledgerSwap(world, *ledger);
  }
}

// Registers the kind's state component, <name>-ledger, its resolver, <name>-flow, and its swap.
// Throws std::invalid_argument when a name is malformed or already registered.
template <FlowDefinition K> void addFlow(WorldSchema &schema) {
  const std::string name(K::Name);
  schema.addComponent<FlowLedger<K>>(name + "-ledger", DataKind::State);
  schema.addResolver(name + "-flow", &ensureLedger<K>);
  schema.addSwap(&swapLedger<K>);
}

// Adds units under the handle to the endpoint's stock. Systems only. Throws std::logic_error when
// the world is not stepping or holds no ledger for the kind, and std::invalid_argument when the
// endpoint is not a live entity, the units are below 1, or the count would overflow; a throw
// changes nothing.
template <FlowDefinition K>
void createUnits(World &world, EntityKey endpoint, EntityKey handle, int64_t units) {
  ledgerCreate(world, steppingLedger<K>(world), K::Name, endpoint, handle, units);
}

// Sends units of the handle from the endpoint's stock to the destination, arriving after the
// delay. Systems only. Throws as createUnits does.
template <FlowDefinition K>
void sendUnits(World &world, EntityKey from, EntityKey to, EntityKey handle, int64_t units,
               uint32_t delay) {
  ledgerSend(world, steppingLedger<K>(world), K::Name, from, to, handle, units, delay);
}

// Consumes units of the handle from the endpoint's stock with the cause. Systems only. Throws as
// createUnits does.
template <FlowDefinition K>
void consumeUnits(World &world, EntityKey endpoint, EntityKey handle, int64_t units,
                  std::string_view cause) {
  ledgerConsume(world, steppingLedger<K>(world), K::Name, endpoint, handle, units, cause);
}

// The units the endpoint holds under the handle.
template <FlowDefinition K>
int64_t unitsHeld(const World &world, EntityKey endpoint, EntityKey handle) {
  const Ledger *ledger = ledgerOf<K>(world);
  return ledger == nullptr ? 0 : ledgerHeld(*ledger, endpoint, handle);
}

// The units held in every stock.
template <FlowDefinition K> int64_t unitsHeld(const World &world) {
  const Ledger *ledger = ledgerOf<K>(world);
  int64_t total = 0;
  if (ledger != nullptr) {
    for (const FlowStock &stock : ledger->Stocks) {
      total += stock.Units;
    }
  }
  return total;
}

// The endpoint's holdings, handles ascending.
template <FlowDefinition K>
std::vector<FlowHolding> stockOf(const World &world, EntityKey endpoint) {
  std::vector<FlowHolding> holdings;
  const Ledger *ledger = ledgerOf<K>(world);
  if (ledger == nullptr) {
    return holdings;
  }
  for (auto at = std::ranges::lower_bound(ledger->Stocks, endpoint, {}, &FlowStock::Endpoint);
       at != ledger->Stocks.end() && at->Endpoint == endpoint; ++at) {
    holdings.push_back(FlowHolding{at->Handle, at->Units});
  }
  return holdings;
}

template <FlowDefinition K> int64_t unitsInTransit(const World &world) {
  const Ledger *ledger = ledgerOf<K>(world);
  int64_t total = 0;
  if (ledger != nullptr) {
    for (const FlowPacket &packet : ledger->Packets) {
      total += packet.Units;
    }
  }
  return total;
}

template <FlowDefinition K> int64_t unitsCreated(const World &world) {
  const Ledger *ledger = ledgerOf<K>(world);
  return ledger == nullptr ? 0 : ledger->Created;
}

template <FlowDefinition K> int64_t unitsConsumed(const World &world) {
  const Ledger *ledger = ledgerOf<K>(world);
  int64_t total = 0;
  if (ledger != nullptr) {
    for (const FlowConsumption &consumption : ledger->Consumed) {
      total += consumption.Units;
    }
  }
  return total;
}

// The units consumed with the cause, 0 for a cause never used.
template <FlowDefinition K> int64_t unitsConsumed(const World &world, std::string_view cause) {
  const Ledger *ledger = ledgerOf<K>(world);
  if (ledger == nullptr) {
    return 0;
  }
  const uint64_t hashed = hashName(cause);
  const auto at = std::ranges::lower_bound(ledger->Consumed, hashed, {}, &FlowConsumption::Cause);
  return at != ledger->Consumed.end() && at->Cause == hashed ? at->Units : 0;
}

// The units of a kind addressed to an entity: the packets and stocks whose handle is its key.
struct FlowAddressed {
  std::vector<FlowPacket> Packets;
  std::vector<FlowStock> Stocks;
};

// Packets and stocks each in the ledger's order.
template <FlowDefinition K> FlowAddressed addressedTo(const World &world, EntityKey handle) {
  FlowAddressed addressed;
  const Ledger *ledger = ledgerOf<K>(world);
  if (ledger == nullptr) {
    return addressed;
  }
  std::ranges::copy_if(ledger->Packets, std::back_inserter(addressed.Packets),
                       [handle](const FlowPacket &packet) { return packet.Handle == handle; });
  std::ranges::copy_if(ledger->Stocks, std::back_inserter(addressed.Stocks),
                       [handle](const FlowStock &stock) { return stock.Handle == handle; });
  return addressed;
}

} // namespace tpj

#endif
