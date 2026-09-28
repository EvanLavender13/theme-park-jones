#include "sim/medium/flow.h"

#include "sim/field_text.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace tpj {

namespace {

bool isLive(const World &world, EntityKey key) {
  return key != NULL_KEY && world.findEntity(key) != entt::null;
}

std::string keyText(EntityKey key) { return std::to_string(static_cast<uint64_t>(key)); }

std::pair<EntityKey, EntityKey> stockOrder(const FlowStock &stock) {
  return {stock.Endpoint, stock.Handle};
}

// The stock entry for the endpoint and handle, or where it would go.
std::vector<FlowStock>::iterator findStock(std::vector<FlowStock> &stocks, EntityKey endpoint,
                                           EntityKey handle) {
  return std::ranges::lower_bound(stocks, std::pair{endpoint, handle}, {}, &stockOrder);
}

void addToStock(Ledger &ledger, EntityKey endpoint, EntityKey handle, int64_t units) {
  const auto at = findStock(ledger.Stocks, endpoint, handle);
  if (at != ledger.Stocks.end() && at->Endpoint == endpoint && at->Handle == handle) {
    at->Units += units;
  } else {
    ledger.Stocks.insert(at, FlowStock{endpoint, handle, units});
  }
}

// Takes units the caller has checked the stock holds.
void takeFromStock(Ledger &ledger, EntityKey endpoint, EntityKey handle, int64_t units) {
  const auto at = findStock(ledger.Stocks, endpoint, handle);
  at->Units -= units;
  if (at->Units == 0) {
    ledger.Stocks.erase(at);
  }
}

void addConsumed(Ledger &ledger, uint64_t cause, int64_t units) {
  const auto at = std::ranges::lower_bound(ledger.Consumed, cause, {}, &FlowConsumption::Cause);
  if (at != ledger.Consumed.end() && at->Cause == cause) {
    at->Units += units;
  } else {
    ledger.Consumed.insert(at, FlowConsumption{cause, units});
  }
}

void addPacket(Ledger &ledger, const FlowPacket &packet) {
  ledger.Packets.insert(std::ranges::upper_bound(ledger.Packets, packet), packet);
}

// Throws std::invalid_argument unless the endpoint is live and the units are at least 1.
void requireEndpoint(const World &world, std::string_view kind, EntityKey endpoint, int64_t units) {
  const std::string name(kind);
  if (endpoint == NULL_KEY) {
    throw std::invalid_argument("flow " + name + " takes no operation from the null key");
  }
  if (!isLive(world, endpoint)) {
    throw std::invalid_argument("flow " + name + " takes no operation from entity " +
                                keyText(endpoint) + ", which is not live");
  }
  if (units < 1) {
    throw std::invalid_argument("flow " + name + " takes at least 1 unit");
  }
}

// Throws std::invalid_argument when the endpoint holds fewer units of the handle.
void requireHeld(const Ledger &ledger, std::string_view kind, EntityKey endpoint, EntityKey handle,
                 int64_t units) {
  if (ledgerHeld(ledger, endpoint, handle) < units) {
    throw std::invalid_argument("entity " + keyText(endpoint) + " holds fewer than " +
                                std::to_string(units) + " units of flow " + std::string(kind) +
                                " under handle " + keyText(handle));
  }
}

} // namespace

int64_t ledgerHeld(const Ledger &ledger, EntityKey endpoint, EntityKey handle) {
  const auto at =
      std::ranges::lower_bound(ledger.Stocks, std::pair{endpoint, handle}, {}, &stockOrder);
  return at != ledger.Stocks.end() && at->Endpoint == endpoint && at->Handle == handle ? at->Units
                                                                                       : 0;
}

void ledgerCreate(const World &world, Ledger &ledger, std::string_view kind, EntityKey endpoint,
                  EntityKey handle, int64_t units) {
  requireEndpoint(world, kind, endpoint, units);
  if (units > std::numeric_limits<int64_t>::max() - ledger.Created) {
    throw std::invalid_argument("flow " + std::string(kind) +
                                "'s created count would exceed the largest int64_t");
  }
  addToStock(ledger, endpoint, handle, units);
  ledger.Created += units;
}

void ledgerSend(const World &world, Ledger &ledger, std::string_view kind, EntityKey from,
                EntityKey to, EntityKey handle, int64_t units, uint32_t delay) {
  requireEndpoint(world, kind, from, units);
  if (to == NULL_KEY) {
    throw std::invalid_argument("flow " + std::string(kind) + " sends nothing to the null key");
  }
  if (delay == 0) {
    throw std::invalid_argument("flow " + std::string(kind) + " takes a delay of at least 1 tick");
  }
  requireHeld(ledger, kind, from, handle, units);
  takeFromStock(ledger, from, handle, units);
  addPacket(ledger, FlowPacket{.Arrival = world.Tick + delay,
                               .From = from,
                               .To = to,
                               .Handle = handle,
                               .Units = units,
                               .Delay = delay,
                               .Returning = false});
}

void ledgerConsume(const World &world, Ledger &ledger, std::string_view kind, EntityKey endpoint,
                   EntityKey handle, int64_t units, std::string_view cause) {
  requireEndpoint(world, kind, endpoint, units);
  if (!isValidName(cause)) {
    throw std::invalid_argument("flow " + std::string(kind) + " takes no malformed cause");
  }
  requireHeld(ledger, kind, endpoint, handle, units);
  takeFromStock(ledger, endpoint, handle, units);
  addConsumed(ledger, hashName(cause), units);
}

void ledgerSwap(const World &world, Ledger &ledger) {
  // Packets are ordered by arrival first, so the due ones lead.
  const auto due = std::ranges::find_if(
      ledger.Packets, [&world](const FlowPacket &packet) { return packet.Arrival > world.Tick; });
  const std::vector<FlowPacket> arriving(ledger.Packets.begin(), due);
  ledger.Packets.erase(ledger.Packets.begin(), due);
  for (const FlowPacket &packet : arriving) {
    if (isLive(world, packet.To)) {
      addToStock(ledger, packet.To, packet.Handle, packet.Units);
    } else if (!packet.Returning && isLive(world, packet.From)) {
      addPacket(ledger, FlowPacket{.Arrival = world.Tick + packet.Delay,
                                   .From = packet.To,
                                   .To = packet.From,
                                   .Handle = packet.Handle,
                                   .Units = packet.Units,
                                   .Delay = packet.Delay,
                                   .Returning = true});
    } else {
      addConsumed(ledger, hashName(UNDELIVERABLE_CAUSE), packet.Units);
    }
  }
  std::vector<FlowStock> orphaned;
  std::erase_if(ledger.Stocks, [&world, &orphaned](const FlowStock &stock) {
    if (isLive(world, stock.Endpoint)) {
      return false;
    }
    orphaned.push_back(stock);
    return true;
  });
  for (const FlowStock &stock : orphaned) {
    if (isLive(world, stock.Handle)) {
      addToStock(ledger, stock.Handle, stock.Handle, stock.Units);
    } else {
      addConsumed(ledger, hashName(DISCARDED_CAUSE), stock.Units);
    }
  }
}

} // namespace tpj
