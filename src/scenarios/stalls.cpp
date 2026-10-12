#include "scenarios/stalls.h"
#include "scenarios/synthetic.h"
#include "sim/command_queue.h"
#include "sim/draw.h"
#include "sim/entity_key.h"
#include "sim/medium/field.h"
#include "sim/medium/flow.h"
#include "sim/medium/ground_field.h"
#include "sim/medium/network.h"
#include "sim/mix.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <bit>
#include <cmath>
#include <memory>
#include <span>
#include <stdint.h>
#include <vector>

namespace tpj {
namespace {

// Intent: the one straight track everything stands on, cut into edges by evenly spaced stops.
struct Track {
  double Length = 0.0;
  uint32_t Stops = 0;
};

template <typename Visitor> void visitFields(Visitor &visitor, Track &track) {
  visitor.field("length", track.Length);
  visitor.field("stops", track.Stops);
}

// Intent: a stall the player placed at a distance along the track.
struct Stall {
  double Distance = 0.0;
};

template <typename Visitor> void visitFields(Visitor &visitor, Stall &stall) {
  visitor.field("distance", stall.Distance);
}

// State: the depot that makes goods and ships them to stalls.
struct Depot {
  double Distance = 0.0;
};

template <typename Visitor> void visitFields(Visitor &visitor, Depot &depot) {
  visitor.field("distance", depot.Distance);
}

// State: a visitor that sends visits to stalls, one at a time, until it retires.
struct Visitor {
  double Distance = 0.0;
  uint32_t Age = 0;
  bool Out = false;
};

template <typename V> void visitFields(V &visitor, Visitor &value) {
  visitor.field("distance", value.Distance);
  visitor.field("age", value.Age);
  visitor.field("out", value.Out);
}

struct PlaceStall {
  double Distance = 0.0;
};

void applyCommand(World &world, const PlaceStall &command) {
  const EntityKey key = world.createEntity();
  world.emplace<Stall>(key, Stall{command.Distance});
}

struct RemoveStall {
  EntityKey Stall = NULL_KEY;
};

void applyCommand(World &world, const RemoveStall &command) { world.destroyEntity(command.Stall); }

constexpr uint64_t TRACK_PURPOSE = hashName("track-network");
// How wide the track is. Nothing in the scenario reads it, but every carrier has a width.
constexpr double TRACK_WIDTH = 3.0;
constexpr uint64_t SHIP_PURPOSE = hashName("stalls-ship");
constexpr uint64_t CHOOSE_PURPOSE = hashName("stalls-choose");
constexpr uint64_t SPAWN_PURPOSE = hashName("stalls-spawn");
constexpr uint32_t RETIREMENT_AGE = 240;
// How far a visitor looks for other visitors when it chooses a stall. A sample this wide covers at
// most 4 cells, fewer than the visitors present from tick 1 on, so it reads through the cell order.
constexpr double VISITOR_REACH = 2.0;
constexpr uint64_t SPAWN_INTERVAL = 20;
constexpr uint32_t SERVICE_DELAY = 3;

// The lowest key holding T, or NULL_KEY.
template <typename T> EntityKey lowestKeyWith(const World &world) {
  for (const EntityKey key : world.keys()) {
    if (world.has<T>(key)) {
      return key;
    }
  }
  return NULL_KEY;
}

// The keys holding T, ascending.
template <typename T> std::vector<EntityKey> keysWith(const World &world) {
  std::vector<EntityKey> found;
  for (const EntityKey key : world.keys()) {
    if (world.has<T>(key)) {
      found.push_back(key);
    }
  }
  return found;
}

const Network *trackNetwork(const World &world) {
  return world.tryGet<Network>(deriveKey(NULL_KEY, TRACK_PURPOSE, 0));
}

Place trackPlace(const World &world, double distance) {
  return Place{lowestKeyWith<Track>(world), distance};
}

double stallDistance(const World &world, EntityKey stall) {
  return world.get<Stall>(stall).Distance;
}

uint32_t travelDelay(double from, double to) {
  return 1 + static_cast<uint32_t>(std::fabs(from - to) / 4.0);
}

// Derives the track's network: one carrier keyed by the track, with a node at each stop.
void resolveTrack(World &world) {
  const EntityKey owner = lowestKeyWith<Track>(world);
  if (owner == NULL_KEY) {
    return;
  }
  // Creating an entity may move the registry's storage, so the track is copied first.
  const Track track = world.get<Track>(owner);
  Carrier carrier{.Key = owner,
                  .Width = TRACK_WIDTH,
                  .Points = {{.X = 0.0, .Z = 0.0, .Distance = 0.0},
                             {.X = track.Length, .Z = 0.0, .Distance = track.Length}},
                  .Stops = {}};
  for (uint32_t i = 0; i < track.Stops; ++i) {
    const double distance = i + 1 == track.Stops ? track.Length
                                                 : track.Length * static_cast<double>(i) /
                                                       static_cast<double>(track.Stops - 1);
    carrier.Stops.push_back({.Distance = distance, .Node = i});
  }
  const EntityKey key = world.createDerivedEntity(NULL_KEY, TRACK_PURPOSE, 0);
  world.emplaceOrReplace<Network>(key, Network({carrier}, track.Stops, {}));
}

// Publishes each stall's offer, its distance over 10, and a crowd of 1 at its place.
void resolveStallEntries(World &world) {
  for (const EntityKey stall : keysWith<Stall>(world)) {
    const double distance = stallDistance(world, stall);
    const Place place = trackPlace(world, distance);
    publishResolved<StallOffer>(world, stall, {{.At = place, .Value = distance / 10.0}});
    publishResolved<Crowd>(world, stall, {{.At = place, .Value = 1.0}});
  }
}

// Every 5 ticks the depot makes 2 goods, and it ships all it holds, once it holds 4, to a stall
// it draws.
void stepDepot(World &world, TickBuffers & /*buffers*/) {
  const EntityKey depot = lowestKeyWith<Depot>(world);
  if (depot == NULL_KEY) {
    return;
  }
  if (world.Tick % 5 == 0) {
    createUnits<Goods>(world, depot, NULL_KEY, 2);
  }
  const std::vector<EntityKey> stalls = keysWith<Stall>(world);
  const int64_t held = unitsHeld<Goods>(world, depot, NULL_KEY);
  if (stalls.empty() || held < 4) {
    return;
  }
  const std::vector<uint64_t> weights(stalls.size(), 1);
  const EntityKey stall =
      stalls[drawPick(drawKey(world, depot, SHIP_PURPOSE, 0), std::span<const uint64_t>(weights))];
  const double from = world.get<Depot>(depot).Distance;
  sendUnits<Goods>(world, depot, stall, NULL_KEY, held,
                   travelDelay(from, stallDistance(world, stall)));
}

// Each stall offers the goods it holds, and serves each visit it holds while its goods last,
// sending the visit back to its visitor. Visits whose visitor is gone are abandoned.
void stepStalls(World &world, TickBuffers & /*buffers*/) {
  for (const EntityKey stall : keysWith<Stall>(world)) {
    int64_t goods = unitsHeld<Goods>(world, stall, NULL_KEY);
    publishStepped<StallOffer>(world, stall)
        .push_back({.At = trackPlace(world, stallDistance(world, stall)),
                    .Value = static_cast<double>(goods)});
    for (const FlowHolding &holding : stockOf<Visits>(world, stall)) {
      if (!world.isLive(holding.Handle)) {
        consumeUnits<Visits>(world, stall, holding.Handle, holding.Units, "abandoned");
      } else if (goods >= holding.Units) {
        consumeUnits<Goods>(world, stall, NULL_KEY, holding.Units, "sold");
        goods -= holding.Units;
        sendUnits<Visits>(world, stall, holding.Handle, holding.Handle, holding.Units,
                          SERVICE_DELAY);
      }
    }
  }
}

// Each visitor adds to the crowd where it stands, and marks where it stands in visitor presence,
// dropping its mark in its last tick if its key is even. It finishes a visit that came back, and
// otherwise, on its turn, sends a visit to a stall it draws, weighted by the offers there. The draw
// also depends on the visitors near it.
void stepVisitors(World &world, TickBuffers & /*buffers*/) {
  const Network *network = trackNetwork(world);
  if (network == nullptr) {
    return;
  }
  const std::vector<EntityKey> stalls = keysWith<Stall>(world);
  std::vector<GroundSample<double>> nearby;
  for (const EntityKey key : keysWith<Visitor>(world)) {
    Visitor visitor = world.get<Visitor>(key);
    visitor.Age += 1;
    const GroundPoint standing{.X = visitor.Distance, .Z = 0.0};
    if (visitor.Age >= RETIREMENT_AGE && static_cast<uint64_t>(key) % 2 == 0) {
      dropGroundEntry<VisitorPresence>(world, key);
    } else {
      setGroundEntry<VisitorPresence>(world, key, standing, visitor.Distance);
    }
    publishStepped<Crowd>(world, key)
        .push_back({.At = trackPlace(world, visitor.Distance), .Value = 1.0});
    const int64_t returned = unitsHeld<Visits>(world, key, key);
    if (returned > 0) {
      consumeUnits<Visits>(world, key, key, returned, "done");
      visitor.Out = false;
    } else if (!visitor.Out && (world.Tick + static_cast<uint64_t>(key)) % 9 == 0 &&
               !stalls.empty()) {
      std::vector<double> weights;
      for (const EntityKey stall : stalls) {
        double weight = 1.0;
        for (const SampledEntry<double> &entry : sampleField<StallOffer>(
                 world, *network, trackPlace(world, stallDistance(world, stall)))) {
          weight += entry.Value;
        }
        weights.push_back(weight);
      }
      sampleGroundField<VisitorPresence>(world, standing, VISITOR_REACH, nearby);
      Hasher neighbours;
      for (const GroundSample<double> &sample : nearby) {
        if (sample.Key != key) {
          neighbours.add(static_cast<uint64_t>(sample.Key));
          neighbours.add(std::bit_cast<uint64_t>(sample.Value));
        }
      }
      const EntityKey stall =
          stalls[drawPick(drawKey(world, key, CHOOSE_PURPOSE, neighbours.value()),
                          std::span<const double>(weights))];
      createUnits<Visits>(world, key, key, 1);
      sendUnits<Visits>(world, key, stall, key, 1,
                        travelDelay(visitor.Distance, stallDistance(world, stall)));
      visitor.Out = true;
    }
    world.get<Visitor>(key) = visitor;
  }
}

// Visitors retire at RETIREMENT_AGE, and every SPAWN_INTERVAL ticks one arrives at a drawn place.
void turnOverVisitors(World &world, TickBuffers & /*buffers*/) {
  for (const EntityKey key : keysWith<Visitor>(world)) {
    if (world.get<Visitor>(key).Age >= RETIREMENT_AGE) {
      world.destroyEntity(key);
    }
  }
  if (world.Tick % SPAWN_INTERVAL == 0) {
    const double distance = 60.0 * drawUniform(drawKey(world, NULL_KEY, SPAWN_PURPOSE, 0));
    const EntityKey key = world.createEntity();
    world.emplace<Visitor>(key, Visitor{.Distance = distance});
  }
}

} // namespace

Scenario stallsScenario() {
  return Scenario{
      .Name = "stalls",
      .Seed = 3003,
      .MakeSchema = []() -> std::shared_ptr<const WorldSchema> {
        auto schema = std::make_shared<WorldSchema>();
        addNetworkComponent(*schema);
        schema->addComponent<Track>("track", DataKind::Intent);
        schema->addComponent<Stall>("stall", DataKind::Intent);
        schema->addComponent<Depot>("depot", DataKind::State);
        schema->addComponent<Visitor>("visitor", DataKind::State);
        addField<StallOffer>(*schema);
        addField<Crowd>(*schema);
        addGroundField<VisitorPresence>(*schema);
        addFlow<Goods>(*schema);
        addFlow<Visits>(*schema);
        schema->addResolver("track-network", resolveTrack);
        schema->addResolver("stall-entries", resolveStallEntries,
                            {"track-network", "stall-offer-field", "crowd-field"});
        schema->addSystem(stepDepot);
        schema->addSystem(stepStalls);
        schema->addSystem(stepVisitors);
        schema->addSystem(turnOverVisitors);
        schema->addCommand<PlaceStall>();
        schema->addCommand<RemoveStall>();
        return schema;
      },
      .Populate =
          [](World &world) {
            const EntityKey track = world.createEntity();
            world.emplace<Track>(track, Track{.Length = 60.0, .Stops = 7});
            const EntityKey depot = world.createEntity();
            world.emplace<Depot>(depot, Depot{.Distance = 0.0});
            for (const double distance : {15.0, 32.5, 50.0}) {
              const EntityKey stall = world.createEntity();
              world.emplace<Stall>(stall, Stall{distance});
            }
            for (uint32_t i = 0; i < 6; ++i) {
              const EntityKey visitor = world.createEntity();
              world.emplace<Visitor>(
                  visitor, Visitor{.Distance = 5.0 + 8.0 * i, .Age = 40 * i, .Out = false});
            }
          },
      .QueueCommands =
          [](const World &world, CommandQueue &commands) {
            const std::vector<EntityKey> stalls = keysWith<Stall>(world);
            if (world.Tick % 400 == 200 && stalls.size() >= 2) {
              commands.push(RemoveStall{stalls.front()});
            }
            if (world.Tick % 400 == 0 && world.Tick > 0) {
              commands.push(PlaceStall{static_cast<double>((world.Tick / 400 * 17) % 60)});
            }
          },
  };
}

} // namespace tpj
