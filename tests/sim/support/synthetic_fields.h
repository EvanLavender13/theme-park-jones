#ifndef TPJ_TESTS_SIM_SUPPORT_SYNTHETIC_FIELDS_H
#define TPJ_TESTS_SIM_SUPPORT_SYNTHETIC_FIELDS_H

#include "sim/entity_key.h"
#include "sim/medium/field.h"
#include "sim/medium/network.h"
#include "sim/mix.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <algorithm>
#include <memory>
#include <stdint.h>
#include <string_view>
#include <utility>
#include <vector>

// Synthetic fields owned by the tests, the intent their sources publish from, and a synthetic
// network producer, all registered through the public schema.
namespace tpj::test {

// A scalar field sampled by the default rule.
struct Footfall {
  using Entry = double;
  static constexpr std::string_view Name = "footfall";
  static constexpr FieldKind Kind = FieldKind::Scalar;
};

// What Reach's rule was given, and what it returned, for one call.
struct ReachCall {
  EdgeSample<double> Sample;
  std::vector<double> Returned;
};

// Reach's calls, in call order. The rule takes only its sample, so the record lives outside it.
inline std::vector<ReachCall> &reachCalls() {
  static std::vector<ReachCall> calls;
  return calls;
}

// An entry field with its owner's rule for places inside an edge. The rule returns the total of the
// values it was given, then the place's FromOffset, so each call's result is traceable to its
// sample.
struct Reach {
  using Entry = double;
  static constexpr std::string_view Name = "reach";
  static constexpr FieldKind Kind = FieldKind::Entry;

  static std::vector<double> sampleEdge(const EdgeSample<double> &sample) {
    double total = 0.0;
    for (const double value : sample.AtFrom) {
      total += value;
    }
    for (const double value : sample.AtTo) {
      total += value;
    }
    for (const EdgeEntry<double> &entry : sample.Along) {
      total += entry.Value;
    }
    std::vector<double> returned = {total, sample.FromOffset};
    reachCalls().push_back({.Sample = sample, .Returned = returned});
    return returned;
  }
};

// A source's intent for field F: the entries it publishes, in order.
template <typename F> struct Emits {
  std::vector<PlacedEntry<double>> Entries;
};

template <typename Visitor, typename F> void visitFields(Visitor &visitor, Emits<F> &emits) {
  visitor.field("entries", emits.Entries);
}

// The order the synthetic publishers publish their sources in, relative to ascending key order.
enum class PublishOrder : uint8_t { Ascending, Descending, Rotated };

inline PublishOrder &publishOrder() {
  static PublishOrder order = PublishOrder::Descending;
  return order;
}

// Sets the publish order for a scope, restoring the previous one after it.
class PublishOrderScope {
public:
  explicit PublishOrderScope(PublishOrder order) : Previous(publishOrder()) {
    publishOrder() = order;
  }
  PublishOrderScope(const PublishOrderScope &) = delete;
  PublishOrderScope &operator=(const PublishOrderScope &) = delete;
  PublishOrderScope(PublishOrderScope &&) = delete;
  PublishOrderScope &operator=(PublishOrderScope &&) = delete;
  ~PublishOrderScope() { publishOrder() = Previous; }

private:
  PublishOrder Previous;
};

// Publishes each entity holding Emits<F> as a source, in the current publish order.
template <typename F> void publishSources(World &world) {
  std::vector<std::pair<EntityKey, std::vector<PlacedEntry<double>>>> sources;
  world.Registry.view<Emits<F>>().each([&](entt::entity entity, const Emits<F> &emits) {
    sources.emplace_back(world.keyOf(entity), emits.Entries);
  });
  std::ranges::sort(sources, [](const auto &left, const auto &right) {
    return static_cast<uint64_t>(left.first) < static_cast<uint64_t>(right.first);
  });
  if (publishOrder() == PublishOrder::Descending) {
    std::ranges::reverse(sources);
  } else if (publishOrder() == PublishOrder::Rotated && !sources.empty()) {
    std::ranges::rotate(sources, sources.begin() + 1);
  }
  for (auto &[source, entries] : sources) {
    publishResolved<F>(world, source, std::move(entries));
  }
}

// The intent a network is derived from.
struct Layout {
  std::vector<Carrier> Carriers;
  uint32_t NodeCount = 0;
};

template <typename Visitor> void visitFields(Visitor &visitor, Layout &layout) {
  visitor.field("carriers", layout.Carriers);
  visitor.field("nodes", layout.NodeCount);
}

inline constexpr uint64_t NETWORK_PURPOSE = hashName("network");

// The synthetic producer: each layout derives an entity holding its network.
inline void produceNetworks(World &world) {
  std::vector<std::pair<EntityKey, Layout>> layouts;
  world.Registry.view<Layout>().each([&](entt::entity entity, const Layout &layout) {
    layouts.emplace_back(world.keyOf(entity), layout);
  });
  for (const auto &[owner, layout] : layouts) {
    const EntityKey key = world.createDerivedEntity(owner, NETWORK_PURPOSE, 0);
    world.Registry.emplace_or_replace<Network>(world.findEntity(key),
                                               Network(layout.Carriers, layout.NodeCount, {}));
  }
}

// Carriers of the standard layout. A runs straight over distance 10 and stops at nodes 0, 1, and 2
// at 0, 4, and 10. B leaves the junction at node 1 and reaches node 3 at 6. C leaves node 3 and
// returns to it at 8, so its one edge has the same node at both ends.
inline constexpr EntityKey CARRIER_A{10};
inline constexpr EntityKey CARRIER_B{20};
inline constexpr EntityKey CARRIER_C{30};

inline Layout standardLayout() {
  return Layout{
      .Carriers =
          {
              Carrier{
                  .Key = CARRIER_A,
                  .Points = {{.X = 0, .Z = 0, .Distance = 0}, {.X = 10, .Z = 0, .Distance = 10}},
                  .Stops = {{.Distance = 0, .Node = 0},
                            {.Distance = 4, .Node = 1},
                            {.Distance = 10, .Node = 2}}},
              Carrier{.Key = CARRIER_B,
                      .Points = {{.X = 4, .Z = 0, .Distance = 0}, {.X = 4, .Z = 6, .Distance = 6}},
                      .Stops = {{.Distance = 0, .Node = 1}, {.Distance = 6, .Node = 3}}},
              Carrier{.Key = CARRIER_C,
                      .Points = {{.X = 4, .Z = 6, .Distance = 0},
                                 {.X = 8, .Z = 6, .Distance = 4},
                                 {.X = 4, .Z = 6, .Distance = 8}},
                      .Stops = {{.Distance = 0, .Node = 3}, {.Distance = 8, .Node = 3}}},
          },
      .NodeCount = 4,
  };
}

// Registers network (derived), layout (intent), and the producer's resolver, layout-network.
inline void addSyntheticNetwork(WorldSchema &schema) {
  addNetworkComponent(schema);
  schema.addComponent<Layout>("layout", DataKind::Intent);
  schema.addResolver("layout-network", produceNetworks);
}

// Registers footfall and reach, their sources' intent, footfall-source and reach-source, and their
// publishers, footfall-sources and reach-sources, each after its field's resolver.
inline void addSyntheticFields(WorldSchema &schema) {
  addField<Footfall>(schema);
  addField<Reach>(schema);
  schema.addComponent<Emits<Footfall>>("footfall-source", DataKind::Intent);
  schema.addComponent<Emits<Reach>>("reach-source", DataKind::Intent);
  schema.addResolver("footfall-sources", publishSources<Footfall>, {"footfall-field"});
  schema.addResolver("reach-sources", publishSources<Reach>, {"reach-field"});
}

inline std::shared_ptr<WorldSchema> makeFieldSchema() {
  auto schema = std::make_shared<WorldSchema>();
  addSyntheticNetwork(*schema);
  addSyntheticFields(*schema);
  return schema;
}

// The key the standard world's layout entity takes, as the first from the counter.
inline constexpr EntityKey LAYOUT_KEY{1};

// A world whose first entity holds the standard layout. Not yet resolved.
inline World makeFieldWorld(std::shared_ptr<const WorldSchema> schema) {
  World world(std::move(schema), 2024);
  const EntityKey layout = world.createEntity();
  world.Registry.emplace<Layout>(world.findEntity(layout), standardLayout());
  return world;
}

// The network derived from the standard world's layout. The world must have been resolved.
inline const Network &networkOf(const World &world) {
  return world.Registry.get<Network>(world.findEntity(deriveKey(LAYOUT_KEY, NETWORK_PURPOSE, 0)));
}

// A new source publishing the entries into F.
template <typename F>
EntityKey addSource(World &world, const std::vector<PlacedEntry<double>> &entries) {
  const EntityKey key = world.createEntity();
  world.Registry.emplace<Emits<F>>(world.findEntity(key), Emits<F>{.Entries = entries});
  return key;
}

inline PlacedEntry<double> entryAt(EntityKey carrier, double distance, double value) {
  return {.At = {.Carrier = carrier, .Distance = distance}, .Value = value};
}

} // namespace tpj::test

#endif
