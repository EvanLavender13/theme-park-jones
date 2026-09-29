#ifndef TPJ_TESTS_SIM_SUPPORT_LEDGER_WRITES_H
#define TPJ_TESTS_SIM_SUPPORT_LEDGER_WRITES_H

#include "sim/entity_key.h"
#include "sim/medium/flow.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <iterator>
#include <stdint.h>
#include <tuple>
#include <vector>

// A world's ledgers are the medium's state, so a test states any world between cycles by writing
// them, keeping each ledger's order and identity.
namespace tpj::test {

// The kind's ledger, which resolution has created.
template <FlowDefinition K> Ledger &ledgerIn(World &world) {
  const entt::entity holder = world.findEntity(flowKey(K::Name));
  auto *ledger = holder == entt::null ? nullptr : world.Registry.try_get<FlowLedger<K>>(holder);
  REQUIRE(ledger != nullptr);
  return *ledger;
}

// Puts units in the endpoint's stock under the handle.
template <FlowDefinition K>
void hold(World &world, EntityKey endpoint, EntityKey handle, int64_t units) {
  Ledger &ledger = ledgerIn<K>(world);
  const auto at = std::ranges::find_if(ledger.Stocks, [&](const FlowStock &stock) {
    return std::tie(stock.Endpoint, stock.Handle) >= std::tie(endpoint, handle);
  });
  if (at != ledger.Stocks.end() && at->Endpoint == endpoint && at->Handle == handle) {
    at->Units += units;
  } else {
    ledger.Stocks.insert(at, FlowStock{.Endpoint = endpoint, .Handle = handle, .Units = units});
  }
  ledger.Created += units;
}

// Puts a packet in transit.
template <FlowDefinition K> void carry(World &world, const FlowPacket &packet) {
  Ledger &ledger = ledgerIn<K>(world);
  ledger.Packets.insert(std::ranges::upper_bound(ledger.Packets, packet), packet);
  ledger.Created += packet.Units;
}

// The kind's packets in transit from the endpoint, in the ledger's order.
template <FlowDefinition K>
std::vector<FlowPacket> packetsFrom(const World &world, EntityKey from) {
  std::vector<FlowPacket> packets;
  const Ledger *ledger = ledgerOf<K>(world);
  REQUIRE(ledger != nullptr);
  std::ranges::copy_if(ledger->Packets, std::back_inserter(packets),
                       [from](const FlowPacket &packet) { return packet.From == from; });
  return packets;
}

template <FlowDefinition K> bool conserved(const World &world) {
  return unitsCreated<K>(world) ==
         unitsInTransit<K>(world) + unitsHeld<K>(world) + unitsConsumed<K>(world);
}

} // namespace tpj::test

#endif
